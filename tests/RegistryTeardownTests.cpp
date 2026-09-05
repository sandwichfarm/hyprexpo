// The overview registry teardown hazard, reduced to plain C++ so it runs
// without a compositor.
//
// ~COverview() is not passive: it resets the cursor override, Hyprland
// re-sends the cursor shape, that damages the monitor, and the damage comes
// straight back into the plugin through hkAddDamage -> overviewForMonitor(),
// which walks g_overviews. So the registry must already be consistent before
// any COverview destructor runs.
//
// Two teardown strategies do NOT give you that:
//
//   std::erase_if(g_overviews, ...) runs std::remove_if first, which
//   move-assigns the unique_ptrs. unique_ptr's move-assignment releases the
//   source, stores the new pointer, and only then deletes the old object. So
//   the destructor runs while the vector still has its old size and holds a
//   released (null) slot. The pre-fix overviewForMonitor() dereferenced that
//   slot -- that is the reported SIGSEGV in
//   CWeakPointer<CMonitor>::operator==.
//
//   g_overviews.clear() destroys the elements before it drops the size, so a
//   destructor sees a fully populated registry of already-destroyed objects.
//
// The lookup below deliberately never dereferences a slot, so these cases are
// safe to execute; it only reports what state the registry was in. That is
// enough to pin the invariant.

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const std::string& label) {
    if (condition)
        return;

    ++failures;
    std::cerr << "FAIL: " << label << '\n';
}

struct Node;

// Stands in for g_overviews.
std::vector<std::unique_ptr<Node>> g_registry;

// What the re-entrant lookup saw the last time a destructor triggered it.
struct SObservation {
    bool   ran          = false;
    size_t reportedSize = 0;
    int    nullSlots    = 0;
    int    liveSlots    = 0;
    bool   sawDyingNode = false;
};

SObservation g_observed;
Node*        g_dying = nullptr;

void reentrantLookup();

struct Node {
    explicit Node(int id_) : id(id_) {}

    ~Node() {
        // Stands in for unsetOverride() -> cursor re-send -> damage -> hook.
        g_dying = this;
        reentrantLookup();
        g_dying = nullptr;
    }

    int id = 0;
};

// Stands in for overviewForMonitor(). Never dereferences a slot: inspecting
// the slot state is all we need, and it keeps the hazard cases executable.
void reentrantLookup() {
    g_observed              = SObservation{};
    g_observed.ran          = true;
    g_observed.reportedSize = g_registry.size();

    for (const auto& entry : g_registry) {
        if (!entry) {
            ++g_observed.nullSlots;
            continue;
        }
        ++g_observed.liveSlots;
        if (entry.get() == g_dying)
            g_observed.sawDyingNode = true;
    }
}

void seedRegistry(int count) {
    g_registry.clear();
    g_observed = SObservation{};
    for (int i = 0; i < count; ++i)
        g_registry.push_back(std::make_unique<Node>(i));
    g_observed = SObservation{};
}

// The pre-fix destroyOverview().
void destroyWithEraseIf(Node* node) {
    std::erase_if(g_registry, [node](const auto& entry) { return entry.get() == node; });
}

// The shipped destroyOverview(): unregister the owner, then release it.
void destroyDetachFirst(Node* node) {
    const auto IT = std::find_if(g_registry.begin(), g_registry.end(), [node](const auto& entry) { return entry && entry.get() == node; });
    if (IT == g_registry.end())
        return;

    auto OWNER = std::move(*IT);
    g_registry.erase(IT);
    OWNER.reset();
}

// The pre-fix destroyAllOverviews().
void destroyAllWithClear() {
    g_registry.clear();
}

// The shipped destroyAllOverviews().
void destroyAllSwapFirst() {
    std::vector<std::unique_ptr<Node>> OWNERS;
    OWNERS.swap(g_registry);
    OWNERS.clear();
}

} // namespace

int main() {
    // --- single teardown, the path that actually crashed --------------------
    // Remove a middle element so remove_if has later elements to shift down;
    // removing the last one never move-assigns and would hide the bug.
    seedRegistry(4);
    destroyWithEraseIf(g_registry[1].get());
    expect(g_observed.ran, "erase_if runs the destructor re-entrantly");
    expect(g_observed.nullSlots > 0, "erase_if lets the destructor observe a released registry slot");
    expect(g_observed.reportedSize == 4, "erase_if has not shrunk the registry when the destructor runs");

    seedRegistry(4);
    destroyDetachFirst(g_registry[1].get());
    expect(g_observed.ran, "detach-first still runs the destructor re-entrantly");
    expect(g_observed.nullSlots == 0, "detach-first never exposes a released registry slot");
    expect(!g_observed.sawDyingNode, "detach-first unregisters the owner before its destructor runs");
    expect(g_observed.reportedSize == 3, "detach-first shrinks the registry before the destructor runs");
    expect(g_registry.size() == 3, "detach-first leaves the surviving entries behind");
    expect(g_registry[0]->id == 0 && g_registry[1]->id == 2 && g_registry[2]->id == 3, "detach-first preserves the order of the survivors");

    // Removing the only entry must still unregister before destroying.
    seedRegistry(1);
    destroyDetachFirst(g_registry[0].get());
    expect(g_observed.reportedSize == 0 && g_observed.nullSlots == 0, "detach-first empties the registry before the last owner is released");
    expect(g_registry.empty(), "detach-first removes the last entry");

    // Destroying something that is not registered must be a no-op.
    seedRegistry(2);
    auto stray = std::make_unique<Node>(99);
    destroyDetachFirst(stray.get());
    expect(g_registry.size() == 2, "detach-first ignores a node that is not in the registry");
    stray.reset();

    // --- bulk teardown ------------------------------------------------------
    seedRegistry(3);
    destroyAllWithClear();
    expect(g_observed.ran, "clear() runs destructors re-entrantly");
    // How much of the registry is still visible part-way through clear() is
    // unspecified -- libstdc++ destroys the elements before it drops the size,
    // and ~unique_ptr nulls its slot after running the deleter. Assert only
    // that the registry is still reachable at all, which is the actual hazard;
    // the exact counts are a libstdc++ characterisation, not a guarantee.
    expect(g_observed.reportedSize > 0 || g_observed.sawDyingNode, "clear() can still expose the registry while destructors run");

    seedRegistry(3);
    destroyAllSwapFirst();
    expect(g_observed.ran, "swap-first still runs destructors re-entrantly");
    expect(g_observed.reportedSize == 0, "swap-first empties the registry before any destructor runs");
    expect(g_observed.nullSlots == 0, "swap-first never exposes a released registry slot");
    expect(g_registry.empty(), "swap-first leaves the registry empty");

    if (failures > 0) {
        std::cerr << "RegistryTeardownTests failed with " << failures << " error(s)\n";
        return EXIT_FAILURE;
    }

    std::cout << "RegistryTeardownTests passed\n";
    return EXIT_SUCCESS;
}
