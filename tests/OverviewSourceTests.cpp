#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string& label) {
    if (condition)
        return;

    ++failures;
    std::cerr << "FAIL: " << label << '\n';
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file)
        return {};

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string extractFunction(const std::string& source, const std::string& signature) {
    const auto start = source.find(signature);
    if (start == std::string::npos)
        return {};

    const auto open = source.find('{', start);
    if (open == std::string::npos)
        return {};

    int depth = 0;
    for (size_t i = open; i < source.size(); ++i) {
        if (source[i] == '{')
            ++depth;
        else if (source[i] == '}') {
            --depth;
            if (depth == 0)
                return source.substr(start, i - start + 1);
        }
    }

    return {};
}

}

int main() {
    const auto source = readFile("Overview.cpp");
    expect(!source.empty(), "Overview.cpp can be read from repo root");

    const auto function = extractFunction(source, "void removeOverview(");
    expect(!function.empty(), "removeOverview function exists");

    const auto lockPos     = function.find("const auto MON = g_pOverview->pMonitor.lock();");
    const auto resetPos    = function.find("g_pOverview.reset();");
    const auto damagePos   = function.find("g_pHyprRenderer->damageMonitor(MON);");
    const auto schedulePos = function.find("MON->scheduleFrame();");

    expect(lockPos != std::string::npos, "removeOverview captures monitor before teardown");
    expect(resetPos != std::string::npos, "removeOverview destroys active overview");
    expect(damagePos != std::string::npos, "removeOverview damages monitor after teardown");
    expect(schedulePos != std::string::npos, "removeOverview schedules a frame after teardown");
    expect(lockPos < resetPos, "monitor is captured before overview reset");
    expect(resetPos < damagePos, "monitor damage happens after overview reset");
    expect(damagePos < schedulePos, "frame scheduling follows monitor damage");

    const auto mainSource = readFile("main.cpp");
    expect(!mainSource.empty(), "main.cpp can be read from repo root");
    expect(mainSource.find("const Time::steady_tp& now") != std::string::npos, "render hook uses the Hyprland 0.56 time-point ABI");
    expect(mainSource.find("_ZN7Monitor8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE") != std::string::npos,
           "damage hook uses the Hyprland 0.56 namespaced monitor symbol");
    expect(mainSource.find("_ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE") == std::string::npos,
           "damage hook no longer uses the pre-0.56 monitor symbol");

    const auto dispatchersSource = readFile("Dispatchers.cpp");
    expect(!dispatchersSource.empty(), "Dispatchers.cpp can be read from repo root");
    const auto expoDispatcher = extractFunction(dispatchersSource, "static SDispatchResult onExpoDispatcher(std::string arg) {");
    expect(!expoDispatcher.empty(), "expo dispatcher function exists");

    const auto toggleStart = expoDispatcher.find("if (arg == \"toggle\")");
    const auto cancelStart = expoDispatcher.find("if (arg == \"cancel\")", toggleStart);
    const auto toggleBlock = toggleStart == std::string::npos || cancelStart == std::string::npos ? std::string{} : expoDispatcher.substr(toggleStart, cancelStart - toggleStart);
    expect(toggleBlock.find("g_pOverview->close(false);") != std::string::npos, "plain toggle close does not select a fallback workspace");

    const auto offStart = expoDispatcher.find("if (arg == \"off\" || arg == \"close\" || arg == \"disable\")");
    const auto offEnd   = expoDispatcher.find("\n    if (g_pOverview)\n        return {};", offStart);
    const auto offBlock = offStart == std::string::npos || offEnd == std::string::npos ? std::string{} : expoDispatcher.substr(offStart, offEnd - offStart);
    expect(offBlock.find("g_pOverview->close(false);") != std::string::npos, "plain off and close commands do not select a fallback workspace");

    const auto overviewConstructor = extractFunction(source, "COverview::COverview(");
    expect(!overviewConstructor.empty(), "overview constructor exists");
    const auto gapExpansionPos = overviewConstructor.find("Hyprexpo::expandDynamicWorkspaceIDs(");
    const auto dynamicResizePos = overviewConstructor.find("images.resize(visibleWorkspaceIDs.size())");
    expect(gapExpansionPos != std::string::npos, "dynamic workspace enumeration uses the bounded expansion helper");
    expect(dynamicResizePos != std::string::npos && gapExpansionPos < dynamicResizePos, "dynamic expansion is bounded before image allocation");
    expect(overviewConstructor.find("for (int64_t id = minID; id <= maxID; ++id)") == std::string::npos,
           "dynamic workspace enumeration has no unbounded min-to-max fill loop");

    const auto renderSource = readFile("OverviewRender.cpp");
    expect(!renderSource.empty(), "OverviewRender.cpp can be read from repo root");
    const auto fullRender = extractFunction(renderSource, "void COverview::fullRender(");
    expect(!fullRender.empty(), "overview fullRender function exists");
    expect(fullRender.find("Hyprexpo::shouldShowWorkspaceLabel(") != std::string::npos,
           "runtime label rendering uses modern label_enable and label_show policy in every grid mode");
    expect(fullRender.find("Hyprexpo::resolveBorderSpec(") != std::string::npos,
           "runtime border rendering uses modern-first border resolution with legacy fallback");

    if (failures != 0)
        return 1;

    std::cout << "OverviewSourceTests passed\n";
    return 0;
}
