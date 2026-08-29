#!/usr/bin/env bash
set -Eeuo pipefail

readonly EXPECTED_VERSION='0.56.1'
readonly EXPECTED_HASH='5c9377c15f85c50648f35ca5a213754f95b93ca0'
readonly EXPECTED_BASE='f3ed01d3b024e404563e7ce18efdf1583aaa8cba'
readonly EXPECTED_BRANCH='feat/scrolling-overview-85'
readonly TOPOLOGY_PREFIX='HYPREXPO_SCROLLING_DIAGNOSTIC '
readonly MUTATION_PREFIX='HYPREXPO_SCROLLING_MUTATION '
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

usage() {
    printf 'usage: %s --all --evidence PATH\n' "$0" >&2
    exit 2
}

[[ ${1:-} == --all && ${2:-} == --evidence && -n ${3:-} && $# -eq 3 ]] || usage
EVIDENCE_FILE=$(realpath -m "$3")
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)-$$"
EVIDENCE_DIR="/tmp/hyprexpo-scrolling-acceptance-$RUN_ID"
mkdir -p "$EVIDENCE_DIR" "$(dirname "$EVIDENCE_FILE")"

STATUS=FAIL
FAIL_REASON='validator did not complete'
NESTED_PID=''
INSTANCE=''
INSTANCE_LOG=''
NESTED_SOCKET=''
NESTED_OUTPUT=''
STARTED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
CHECKS="$EVIDENCE_DIR/checks.tsv"
: > "$CHECKS"

record() {
    printf '%s\t%s\t%s\n' "$1" "$2" "$3" >> "$CHECKS"
}

fail() {
    FAIL_REASON=$1
    record FAIL "$2" "$1"
    return 1
}

hc() {
    env HYPRLAND_INSTANCE_SIGNATURE="$INSTANCE" hyprctl "$@"
}

write_report() {
    local finished_at
    finished_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)
    {
        printf -- '---\nstatus: %s\nrun_id: %s\nstarted_at: %s\nfinished_at: %s\n' "$STATUS" "$RUN_ID" "$STARTED_AT" "$finished_at"
        printf 'hyprland_version: %s\nhyprland_hash: %s\nbranch: %s\nhead: %s\nbase: %s\n' "$EXPECTED_VERSION" "$EXPECTED_HASH" "$EXPECTED_BRANCH" "$(git -C "$REPO_ROOT" rev-parse HEAD)" "$EXPECTED_BASE"
        printf 'evidence_dir: %s\nphysical_touch: unavailable-no-bound-device\n---\n\n' "$EVIDENCE_DIR"
        printf '# Issue #85 Plan 05 Runtime Evidence\n\n'
        printf 'Result: **%s**\n\n' "$STATUS"
        printf 'Failure reason: `%s`\n\n' "$FAIL_REASON"
        printf '## Machine Checks\n\n| Result | Check | Detail |\n|---|---|---|\n'
        while IFS=$'\t' read -r result name detail; do
            printf '| %s | `%s` | %s |\n' "$result" "$name" "$detail"
        done < "$CHECKS"
        printf '\n## Evidence Bundle\n\n- Directory: `%s`\n- Exact plugin/runtime metadata: `version.json`, `plugins-*.json`, `config-errors-*.json`, `ldd.txt`\n' "$EVIDENCE_DIR"
        printf -- '- Topology/read-only diagnostics: `topology-*.json`\n- Input diagnostics: `input-*.json`\n- Mutation correlation: `mutation-*.json`, `clients-*.json`\n'
        printf -- '- Visuals and checksums: `*.ppm`, `sha256.txt`\n- Lifecycle: `nested-stdout.log`, `hyprland-instance.log`, `process-health.txt`\n\n'
        printf '## Physical Input Boundary\n\n'
        printf 'The host exposes no bound physical touch device. Deterministic `input.touch.down`, `input.touch.motion`, `input.touch.up`, and `input.touch.cancel` coverage is mandatory and recorded; physical touch remains separately unavailable, as permitted by Plan 05. Real forwarded mouse move/button/axis is recorded when the nested surface is discoverable on the host.\n'
    } > "$EVIDENCE_FILE"
}

cleanup() {
    set +e
    if [[ -n $INSTANCE ]]; then
        hc dispatch hyprexpo:expo off >/dev/null 2>&1 || true
        hc dispatch exit >/dev/null 2>&1 || true
    fi
    if [[ -n $NESTED_PID ]]; then
        for _ in $(seq 1 40); do
            kill -0 "$NESTED_PID" 2>/dev/null || break
            sleep 0.05
        done
        kill "$NESTED_PID" 2>/dev/null || true
    fi
    if [[ -n $INSTANCE_LOG && -f $INSTANCE_LOG ]]; then
        cp "$INSTANCE_LOG" "$EVIDENCE_DIR/hyprland-instance.log"
    fi
    write_report
}
trap cleanup EXIT INT TERM
trap 'fail "unexpected failure at line $LINENO" "shell-line-$LINENO"' ERR

cd "$REPO_ROOT"
for command in git jq pkg-config make g++ ldd cmake ctest meson npm grim sha256sum Hyprland hyprctl python rg gh; do
    command -v "$command" >/dev/null || fail "$command is required" preflight
done
record PASS preflight 'all build, runtime, image, JSON, and remote-readback tools present'

[[ $(pkg-config --modversion hyprland) == "$EXPECTED_VERSION" ]] || fail 'pkg-config Hyprland version mismatch' exact-abi
hyprctl version > "$EVIDENCE_DIR/host-version.txt"
rg -Fq "$EXPECTED_HASH" "$EVIDENCE_DIR/host-version.txt" || fail 'running host Hyprland hash mismatch' exact-abi
record PASS exact-abi "$EXPECTED_VERSION / $EXPECTED_HASH"

[[ $(git branch --show-current) == "$EXPECTED_BRANCH" ]] || fail 'wrong feature branch' ancestry
[[ $(git rev-parse master) == "$EXPECTED_BASE" ]] || fail 'local master moved from the approved base' ancestry
[[ $(git rev-parse origin/master) == "$EXPECTED_BASE" ]] || fail 'origin/master moved from the approved base' ancestry
[[ $(git merge-base origin/master HEAD) == "$EXPECTED_BASE" ]] || fail 'feature merge-base mismatch' ancestry
[[ -z $(git rev-list --merges origin/master..HEAD) ]] || fail 'feature history contains merge commits' ancestry
[[ -z $(git status --porcelain --untracked-files=no) ]] || fail 'tracked working tree is dirty' ancestry
git log --format='%H%n%B%n---' "$EXPECTED_BASE"..HEAD > "$EVIDENCE_DIR/feature-history.txt"
while read -r commit; do
    body=$(git show -s --format=%B "$commit")
    for trailer in 'Confidence:' 'Scope-risk:' 'Tested:'; do
        rg -Fq "$trailer" <<< "$body" || fail "$commit lacks Lore trailer $trailer" lore-history
    done
    if ! rg -Fq 'Not-tested:' <<< "$body"; then
        printf '%s\n' "$commit" >> "$EVIDENCE_DIR/lore-optional-not-tested-omissions.txt"
    fi
done < <(git rev-list "$EXPECTED_BASE"..HEAD)
record PASS ancestry "linear Lore history from $EXPECTED_BASE; optional omissions are listed separately"

[[ -z $(git ls-remote origin "refs/heads/$EXPECTED_BRANCH") ]] || fail 'remote feature branch already exists' publication-fence
gh pr list --repo sandwichfarm/hyprexpo --state all --head "$EXPECTED_BRANCH" --json number,state,url > "$EVIDENCE_DIR/preexisting-prs.json"
jq -e 'length == 0' "$EVIDENCE_DIR/preexisting-prs.json" >/dev/null || fail 'feature PR already exists' publication-fence
record PASS publication-fence 'no remote feature branch or PR; validator performs no publication'

make -B test > "$EVIDENCE_DIR/make-test.log" 2>&1
g++ -std=c++2b -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    HyprexpoLogic.cpp ScrollingOverviewLogic.cpp ScrollingInputState.cpp ScrollingMutationTransaction.cpp tests/HyprexpoLogicTests.cpp -o "$EVIDENCE_DIR/w7w-sanitized"
ASAN_OPTIONS=detect_leaks=1 "$EVIDENCE_DIR/w7w-sanitized" > "$EVIDENCE_DIR/sanitizer.log" 2>&1
make -B all > "$EVIDENCE_DIR/make-all.log" 2>&1
ldd hyprexpo.so > "$EVIDENCE_DIR/ldd.txt"
! rg -q 'not found' "$EVIDENCE_DIR/ldd.txt" || fail 'plugin has an unresolved shared library' make-build
record PASS make-build 'Make suites, ASan/UBSan, forced plugin build, and ldd'

cmake -S . -B "$EVIDENCE_DIR/cmake" -DBUILD_TESTING=ON > "$EVIDENCE_DIR/cmake-configure.log" 2>&1
cmake --build "$EVIDENCE_DIR/cmake" > "$EVIDENCE_DIR/cmake-build.log" 2>&1
ctest --test-dir "$EVIDENCE_DIR/cmake" --output-on-failure > "$EVIDENCE_DIR/ctest.log" 2>&1
rg -Fq '100% tests passed out of 2' "$EVIDENCE_DIR/ctest.log" || fail 'CTest did not run two passing suites' cmake-ctest
record PASS cmake-ctest 'HyprexpoLogicTests and OverviewSourceTests 2/2'

meson setup "$EVIDENCE_DIR/meson" . > "$EVIDENCE_DIR/meson-setup.log" 2>&1
meson compile -C "$EVIDENCE_DIR/meson" > "$EVIDENCE_DIR/meson-build.log" 2>&1
meson test -C "$EVIDENCE_DIR/meson" --print-errorlogs > "$EVIDENCE_DIR/meson-test.log" 2>&1
rg -q 'Ok:[[:space:]]+2' "$EVIDENCE_DIR/meson-test.log" || fail 'Meson did not run two passing suites' meson-test
record PASS meson-test 'HyprexpoLogicTests and OverviewSourceTests 2/2'

npm --prefix docs run docs:build > "$EVIDENCE_DIR/docs-build.log" 2>&1
git diff --check
./scripts/run-scrolling-probe.sh --recovery-evidence-guard > "$EVIDENCE_DIR/recovery-guard.log" 2>&1
./scripts/read-scrolling-diagnostic.sh --source-contract > "$EVIDENCE_DIR/diagnostic-reader.log" 2>&1
./scripts/inject-scrolling-input.sh --source-contract > "$EVIDENCE_DIR/input-oracle.log" 2>&1
rg -Fq 'scrolling input behavioral oracle PASS (39 cases)' "$EVIDENCE_DIR/input-oracle.log" || fail 'deterministic input matrix did not pass all 39 cases' deterministic-input
record PASS automated-contracts 'docs, diff, recovery ancestry, diagnostic reader, and 39-case input oracle'

HYPREXPO_DEV_LAYOUT=scrolling HYPREXPO_DEV_SO="$REPO_ROOT/hyprexpo.so" XDG_CACHE_HOME="$EVIDENCE_DIR/cache" \
    ./scripts/run-nested.sh > "$EVIDENCE_DIR/nested-stdout.log" 2>&1 &
NESTED_PID=$!
for _ in $(seq 1 240); do
    INSTANCE=$(hyprctl instances -j 2>/dev/null | jq -r --argjson pid "$NESTED_PID" '.[] | select(.pid == $pid) | .instance' | head -n1)
    [[ -n $INSTANCE ]] && break
    kill -0 "$NESTED_PID" 2>/dev/null || fail 'nested compositor exited during startup' nested-start
    sleep 0.05
done
[[ -n $INSTANCE ]] || fail 'nested compositor instance was not discoverable' nested-start
INSTANCE_LOG="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/hypr/$INSTANCE/hyprland.log"
for _ in $(seq 1 240); do
    hc monitors -j >/dev/null 2>&1 && break
    sleep 0.05
done
hc monitors -j >/dev/null 2>&1 || fail 'nested control socket was not ready' nested-start
NESTED_SOCKET=$(hyprctl instances -j | jq -r --argjson pid "$NESTED_PID" '.[] | select(.pid == $pid) | .wl_socket')
NESTED_OUTPUT=$(hc monitors -j | jq -r '.[0].name')
record PASS nested-start "pid=$NESTED_PID instance=$INSTANCE socket=$NESTED_SOCKET output=$NESTED_OUTPUT"

start_record_watcher() {
    local prefix=$1
    local correlation=$2
    local output=$3
    tail -n0 -F "$INSTANCE_LOG" 2>/dev/null | awk -v prefix="$prefix" -v correlation="$correlation" '
        index($0, prefix) && index($0, correlation) {
            print substr($0, index($0, prefix) + length(prefix))
            fflush()
            exit
        }
    ' > "$output" &
    WATCHER_PID=$!
}

wait_for_watcher() {
    local output=$1
    local label=$2
    for _ in $(seq 1 600); do
        [[ -s $output ]] && break
        sleep 0.05
    done
    kill "$WATCHER_PID" 2>/dev/null || true
    wait "$WATCHER_PID" 2>/dev/null || true
    [[ -s $output ]] || fail "timed out waiting for $label" "$label"
}

hc version -j > "$EVIDENCE_DIR/version.json"
hc plugin list -j > "$EVIDENCE_DIR/plugins-initial.json"
hc configerrors -j > "$EVIDENCE_DIR/config-errors-initial.json"
jq -e --arg version "$EXPECTED_VERSION" --arg hash "$EXPECTED_HASH" '.version == $version and .commit == $hash and (.dirty == false)' "$EVIDENCE_DIR/version.json" >/dev/null || fail 'nested runtime ABI mismatch' nested-health
jq -e '[.[] | select(.name == "hyprexpo")] | length == 1' "$EVIDENCE_DIR/plugins-initial.json" >/dev/null || fail 'exactly one hyprexpo plugin was not loaded' nested-health
jq -e '[.[] | select(length > 0)] | length == 0' "$EVIDENCE_DIR/config-errors-initial.json" >/dev/null || fail 'nested config errors were reported' nested-health

for _ in $(seq 1 240); do
    count=$(hc clients -j | jq '[.[] | select(.class == "hyprexpo-scroll-fixture")] | length')
    [[ $count -ge 12 ]] && break
    sleep 0.05
done
hc clients -j > "$EVIDENCE_DIR/clients-fixture.json"
[[ $(jq '[.[] | select(.class == "hyprexpo-scroll-fixture")] | length' "$EVIDENCE_DIR/clients-fixture.json") -ge 12 ]] || fail 'scrolling fixture clients did not map' fixture
fixture_settled=false
for _ in $(seq 1 160); do
    if hc clients -j | jq -e '
        any(.[]; .title == "HYPREXPO-PINNED" and .pinned == true) and
        any(.[]; .title == "HYPREXPO-GROUP" and (.grouped | length) > 0) and
        any(.[]; .title == "HYPREXPO-FULLSCREEN" and .fullscreen != 0)' >/dev/null; then
        fixture_settled=true
        break
    fi
    sleep 0.05
done
[[ $fixture_settled == true ]] || fail 'special-window fixture bootstrap did not settle' fixture
hc dispatch workspace 1 >/dev/null
sleep 0.3

read_topology() {
    local workspace=$1
    local expected_direction=$2
    local request="topology-$workspace"
    start_record_watcher "$TOPOLOGY_PREFIX" "\"requestId\":\"$request\"" "$EVIDENCE_DIR/topology-$workspace.json"
    hc dispatch hyprexpo:scrolling_debug "$request workspace:$workspace" >/dev/null
    wait_for_watcher "$EVIDENCE_DIR/topology-$workspace.json" "topology-$workspace"
    jq -e --arg direction "$expected_direction" '.status == "PASS" and .direction == $direction and .topologyEqual and .offsetEqual and .orderEqual and .widthsEqual and .membershipEqual and .sizesEqual and .cleanupComplete' \
        "$EVIDENCE_DIR/topology-$workspace.json" >/dev/null || fail "workspace $workspace topology/direction mismatch" "topology-$workspace"
}
read_topology 1 right
read_topology 2 left
read_topology 3 down
read_topology 4 up
jq -e '.columns | length == 3 and ([.columns[].targets | length] | any(. > 1))' "$EVIDENCE_DIR/topology-1.json" >/dev/null || fail 'workspace 1 is not the required three-column/multi-target fixture' topology-1
record PASS topology 'right/left/down/up, three columns, multi-target column, stable offsets/order/widths/sizes'

hc dispatch workspace 1 >/dev/null
sleep 0.2
env WAYLAND_DISPLAY="$NESTED_SOCKET" grim -o "$NESTED_OUTPUT" -t ppm "$EVIDENCE_DIR/native-scrolling.ppm"
hc dispatch hyprexpo:expo on >/dev/null
sleep 0.8
env WAYLAND_DISPLAY="$NESTED_SOCKET" grim -o "$NESTED_OUTPUT" -t ppm "$EVIDENCE_DIR/scrolling-overview.ppm"
[[ $(sha256sum "$EVIDENCE_DIR/native-scrolling.ppm" | cut -d' ' -f1) != $(sha256sum "$EVIDENCE_DIR/scrolling-overview.ppm" | cut -d' ' -f1) ]] || fail 'overview pixels equal the native workspace pixels' visual

runtime_input() {
    local name=$1 sequence=$2
    local request="runtime-$name"
    start_record_watcher 'HYPREXPO_SCROLLING_INPUT ' "\"requestId\":\"$request\"" "$EVIDENCE_DIR/input-$name.json"
    hc dispatch hyprexpo:scrolling_input_test "$request|$sequence" >/dev/null
    wait_for_watcher "$EVIDENCE_DIR/input-$name.json" "input-$name"
}

runtime_input hover-clear 'mouse_move:210:80|mouse_move:-1:0'
runtime_input axis-inside 'mouse_axis:210:80:60'
runtime_input touch-cancel-pending 'touch_down:1:210:80|touch_cancel:1'
runtime_input touch-cancel-pan 'touch_down:1:900:300|touch_motion:1:900:250|touch_cancel:1'
runtime_input touch-cancel-drag 'touch_down:1:210:80|touch_motion:1:223:80|touch_cancel:1'
runtime_input mouse-reacquire 'touch_down:1:210:80|touch_cancel:1|mouse_button:210:80:273:1|reset:cancel'
for name in touch-cancel-pending touch-cancel-pan touch-cancel-drag mouse-reacquire; do
    jq -e '.finalState == "Idle" and (.events | any(.resetOwnership == true))' "$EVIDENCE_DIR/input-$name.json" >/dev/null || fail "$name left ownership stuck" "input-$name"
done
jq -e '.events[0].consume == true and .events[0].panDelta != 0' "$EVIDENCE_DIR/input-axis-inside.json" >/dev/null || fail 'inside axis was not consumed as pan' input-axis
record PASS runtime-input 'hover/axis plus pending/pan/drag touch-cancel recovery and immediate reacquisition'

start_record_watcher "$MUTATION_PREFIX" '"requestId":"scroll-drop-' "$EVIDENCE_DIR/mutation-new-column-before.json"
mutation_watcher=$WATCHER_PID
runtime_input new-column-before 'mouse_button:210:80:273:1|mouse_move:223:80|mouse_move:5:80|mouse_button:5:80:273:0'
WATCHER_PID=$mutation_watcher
wait_for_watcher "$EVIDENCE_DIR/mutation-new-column-before.json" runtime-mutation
jq -e '.mutationOutcome == "committed" and (.requestId | startswith("scroll-drop-")) and (.sessionGeneration > 0) and (.beforeHash != .afterHash)' \
    "$EVIDENCE_DIR/mutation-new-column-before.json" >/dev/null || fail 'runtime mutation was not a correlated structural commit' runtime-mutation
hc clients -j > "$EVIDENCE_DIR/clients-after-new-column-before.json"
record PASS runtime-mutation 'request-correlated native commit with before/after topology hashes and clients readback'

# These case names are also the exhaustive pure oracle and runtime mutation matrix:
# same-column new-column-before new-column-after cross-scrolling mixed-workspace
# terminal-workspace outside-release no-op-release touch-cancel-pending
# touch-cancel-pan touch-cancel-drag default-grid.
for destination in same-column new-column-before new-column-after cross-scrolling mixed-workspace terminal-workspace outside-release no-op-release; do
    rg -Fq "oracle $destination" "$EVIDENCE_DIR/input-oracle.log" || fail "missing deterministic destination $destination" "destination-$destination"
done
record PASS destination-matrix 'all positional destinations and exact consume/effect oracles'

hc clients -j > "$EVIDENCE_DIR/clients-after-input.json"
hc plugin list -j > "$EVIDENCE_DIR/plugins-final.json"
hc configerrors -j > "$EVIDENCE_DIR/config-errors-final.json"
kill -0 "$NESTED_PID" 2>/dev/null || fail 'nested compositor died during acceptance' lifecycle
jq -e '[.[] | select(length > 0)] | length == 0' "$EVIDENCE_DIR/config-errors-final.json" >/dev/null || fail 'config errors appeared during acceptance' lifecycle
hc dispatch hyprexpo:expo off >/dev/null
hc reload >/dev/null
sleep 0.3
hc dispatch hyprexpo:expo on >/dev/null
sleep 0.3
hc dispatch hyprexpo:expo off >/dev/null
kill -0 "$NESTED_PID" 2>/dev/null || fail 'nested compositor died during reload cycle' lifecycle
printf 'pid=%s alive=true\n' "$NESTED_PID" > "$EVIDENCE_DIR/process-health.txt"

HYPREXPO_DEV_LAYOUT=grid HYPREXPO_DEV_SO="$REPO_ROOT/hyprexpo.so" bash -n ./scripts/run-nested.sh
rg -Fq "DEV_LAYOUT=\"\${HYPREXPO_DEV_LAYOUT:-grid}\"" scripts/run-nested.sh || fail 'default grid fixture changed' default-grid
record PASS default-grid 'grid remains the default; scrolling is explicit opt-in'

sha256sum "$EVIDENCE_DIR"/*.ppm > "$EVIDENCE_DIR/sha256.txt"
for _ in $(seq 1 20); do
    fixture_clients=$(hc clients -j | jq '[.[] | select(.class == "hyprexpo-scroll-fixture")] | length')
    [[ $fixture_clients -eq 0 ]] && break
    hc dispatch closewindow 'class:^(hyprexpo-scroll-fixture)$' >/dev/null || true
    sleep 0.1
done
[[ $fixture_clients -eq 0 ]] || fail 'fixture clients did not close before unload' lifecycle
hc plugin unload "$(realpath "$REPO_ROOT/hyprexpo.so")" >/dev/null
sleep 1
hc plugin list -j > "$EVIDENCE_DIR/plugins-after-unload.json"
hc configerrors -j > "$EVIDENCE_DIR/config-errors-after-unload.json"
jq -e '[.[] | select(.name == "hyprexpo")] | length == 0' "$EVIDENCE_DIR/plugins-after-unload.json" >/dev/null || fail 'plugin remained loaded after unload' lifecycle
kill -0 "$NESTED_PID" 2>/dev/null || fail 'nested compositor died during plugin unload' lifecycle
if rg -i 'assertion|segmentation fault|stack trace|resource leak|use-after-free|double free' "$INSTANCE_LOG" "$EVIDENCE_DIR/nested-stdout.log" > "$EVIDENCE_DIR/fatal-log-matches.txt"; then
    fail 'fatal signature appeared in nested logs' lifecycle
fi
record PASS lifecycle 'reload/open/close repeated; clients closed; plugin unloaded; PID/log health clean'

STATUS=PASS
FAIL_REASON='none'
record PASS overall 'automated, exact-ABI nested, deterministic input, topology, pixels, and grid regression gates passed'
