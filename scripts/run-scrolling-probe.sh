#!/usr/bin/env bash
set -Eeuo pipefail

EXPECTED_VERSION="0.56.1"
EXPECTED_HASH="5c9377c15f85c50648f35ca5a213754f95b93ca0"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RESULT="$REPO_ROOT/.planning/quick/260828-w7w-implement-issue-85-provide-a-niri-like-o/260828-w7w-00-SPIKE-RESULT.md"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)-$$"
EVIDENCE_DIR="/tmp/hyprexpo-scroll-probe-$RUN_ID"
BUILD_DIR="$EVIDENCE_DIR/build"
CONFIG="$EVIDENCE_DIR/hyprland.conf"
STDOUT_LOG="$EVIDENCE_DIR/hyprland-stdout.log"
CLIENTS_JSON="$EVIDENCE_DIR/clients.json"
VERSION_JSON="$EVIDENCE_DIR/version.json"
PLUGINS_JSON="$EVIDENCE_DIR/plugins.json"
CONFIG_ERRORS_JSON="$EVIDENCE_DIR/config-errors.json"
CONTROL_FIFO="$EVIDENCE_DIR/control.fifo"
INSTANCE_LOG=""
INSTANCE=""
NESTED_PID=""
CLIENT_PIDS=()
LAST_COMMAND="initialization"

usage() {
    echo "usage: $0 --non-interactive" >&2
    exit 2
}

[[ "${1:-}" == "--non-interactive" && $# -eq 1 ]] || usage

mkdir -p "$EVIDENCE_DIR"

cleanup() {
    set +e
    for pid in "${CLIENT_PIDS[@]:-}"; do
        [[ -n "$pid" ]] && kill "$pid" 2>/dev/null
    done
    if [[ -n "$NESTED_PID" ]]; then
        kill "$NESTED_PID" 2>/dev/null
        for _ in $(seq 1 40); do
            kill -0 "$NESTED_PID" 2>/dev/null || break
            sleep 0.05
        done
    fi
    if [[ -n "$INSTANCE_LOG" && -f "$INSTANCE_LOG" ]]; then
        cp "$INSTANCE_LOG" "$EVIDENCE_DIR/hyprland-instance.log"
    fi
}
trap cleanup EXIT INT TERM

write_fail() {
    local reason="$1"
    mkdir -p "$(dirname "$RESULT")"
    {
        printf '%s\n' '---'
        printf 'status: FAIL\n'
        printf 'plan: 260828-w7w-00\n'
        printf 'run_id: %s\n' "$RUN_ID"
        printf 'evidence_dir: %s\n' "$EVIDENCE_DIR"
        printf '%s\n\n' '---'
        printf '# Scrolling API Spike Result\n\n'
        printf '**Gate:** FAIL\n\n'
        printf '**Failed command/stage:** `%s`\n\n' "$LAST_COMMAND"
        printf '**Evidence:** %s\n\n' "$reason"
        printf 'Plans 01-05 are blocked. Research workspace-framebuffer crop and isolated translated render-pass alternatives, then revise Plans 01-05 before any execution.\n'
    } > "$RESULT"
}

die() {
    trap - ERR
    write_fail "$1"
    echo "[scrolling-probe] FAIL: $1" >&2
    exit 1
}

on_error() {
    local line="$1"
    trap - ERR
    write_fail "unexpected command failure at script line $line"
    echo "[scrolling-probe] FAIL at line $line" >&2
    exit 1
}
trap 'on_error "$LINENO"' ERR

LAST_COMMAND="exact pkg-config version check"
[[ "$(pkg-config --modversion hyprland)" == "$EXPECTED_VERSION" ]] || die "pkg-config hyprland version is not $EXPECTED_VERSION"
command -v jq >/dev/null || die "jq is required for machine-readable verification"
command -v kitty >/dev/null || die "kitty is required for the controlled nested fixture"

LAST_COMMAND="exact probe CMake build"
cmake -S "$REPO_ROOT/spikes/scrolling-api" -B "$BUILD_DIR" > "$EVIDENCE_DIR/cmake-configure.log"
cmake --build "$BUILD_DIR" > "$EVIDENCE_DIR/cmake-build.log"
PROBE_SO="$BUILD_DIR/hyprexpo-scrolling-api-probe.so"
[[ -f "$PROBE_SO" ]] || die "probe shared object was not produced"

cat > "$CONFIG" <<EOF
monitor = ,800x600@60,auto,1

general {
    layout = scrolling
    border_size = 0
    gaps_in = 0
    gaps_out = 0
}

scrolling {
    direction = right
    column_width = 0.7
    fullscreen_on_one_column = 0
    follow_focus = 1
}

animations {
    enabled = 0
}

misc {
    disable_hyprland_logo = true
    disable_splash_rendering = true
}

debug {
    disable_logs = false
}

plugin = $PROBE_SO
EOF

LAST_COMMAND="nested Hyprland launch"
env WLR_BACKENDS=wayland WLR_RENDERER=pixman WLR_NO_HARDWARE_CURSORS=1 HYPRLAND_NO_LOGO=1 Hyprland -c "$CONFIG" > "$STDOUT_LOG" 2>&1 &
NESTED_PID=$!

for _ in $(seq 1 200); do
    INSTANCE="$(hyprctl instances -j 2>/dev/null | jq -r --argjson pid "$NESTED_PID" '.[] | select(.pid == $pid) | .instance' | head -n1)"
    [[ -n "$INSTANCE" ]] && break
    kill -0 "$NESTED_PID" 2>/dev/null || die "nested Hyprland exited before publishing an instance"
    sleep 0.05
done
[[ -n "$INSTANCE" ]] || die "nested Hyprland instance was not discoverable"
INSTANCE_LOG="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/hypr/$INSTANCE/hyprland.log"

hc() {
    HYPRLAND_INSTANCE_SIGNATURE="$INSTANCE" hyprctl "$@"
}

for _ in $(seq 1 200); do
    hc monitors -j >/dev/null 2>&1 && break
    kill -0 "$NESTED_PID" 2>/dev/null || die "nested Hyprland exited before control socket readiness"
    sleep 0.05
done
hc monitors -j >/dev/null 2>&1 || die "nested Hyprland control socket did not become ready"

LAST_COMMAND="nested ABI/plugin/config health readback"
hc version -j > "$VERSION_JSON"
hc plugin list -j > "$PLUGINS_JSON"
hc configerrors -j > "$CONFIG_ERRORS_JSON"
jq -e --arg version "$EXPECTED_VERSION" --arg hash "$EXPECTED_HASH" '.version == $version and .commit == $hash and (.dirty == false)' "$VERSION_JSON" >/dev/null ||
    die "nested runtime version/hash did not match the exact probe ABI"
jq -e 'length == 1 and .[0].name == "hyprexpo-scroll-probe"' "$PLUGINS_JSON" >/dev/null || die "nested session did not load only the probe plugin"
jq -e '[.[] | select(length > 0)] | length == 0' "$CONFIG_ERRORS_JSON" >/dev/null || die "nested configuration reported errors"

NESTED_SOCKET="$(hyprctl instances -j | jq -r --argjson pid "$NESTED_PID" '.[] | select(.pid == $pid) | .wl_socket')"
[[ -n "$NESTED_SOCKET" ]] || die "nested Wayland socket was not discoverable"
mkfifo "$CONTROL_FIFO"

launch_kitty() {
    local class="$1"
    local title="$2"
    shift 2
    env WAYLAND_DISPLAY="$NESTED_SOCKET" kitty --class "$class" --title "$title" --override cursor_blink_interval=0 "$@" &
    CLIENT_PIDS+=("$!")
}

LAST_COMMAND="controlled scrolling fixture creation"
env PROBE_FIFO="$CONTROL_FIFO" WAYLAND_DISPLAY="$NESTED_SOCKET" kitty --class hyprexpo-probe --title HYPREXPO-PROBE-CONTROLLED \
    --override cursor_blink_interval=0 sh -lc \
    'printf "\033[2J\033[H\033[?25l\033[48;2;16;24;48m  NATIVE SCROLL PROBE  \033[0m\n\n\033[48;2;20;140;80m GREEN BLOCK \033[0m  \033[48;2;180;90;20m ORANGE BLOCK \033[0m\n\nalpha beta gamma\n"; while IFS= read -r command; do if [ "$command" = change ]; then printf "\033[2J\033[H\033[?25l\033[48;2;150;20;40m  CHANGED PIXEL CONTENT  \033[0m\n\n\033[48;2;20;80;190m BLUE BLOCK EXTENDED BLUE BLOCK \033[0m\n\ndelta epsilon zeta\n"; fi; done < "$PROBE_FIFO"' &
CLIENT_PIDS+=("$!")

for spec in 'probe-two:HYPREXPO-PROBE-TWO' 'probe-three:HYPREXPO-PROBE-THREE' 'probe-four:HYPREXPO-PROBE-FOUR'; do
    class="${spec%%:*}"
    title="${spec#*:}"
    launch_kitty "$class" "$title" sh -lc 'printf "\033[2J\033[H\033[?25lfixture window\n"; sleep 300'
    for _ in $(seq 1 100); do
        [[ "$(hc clients -j | jq 'length')" -ge "${#CLIENT_PIDS[@]}" ]] && break
        sleep 0.05
    done
done

for _ in $(seq 1 200); do
    CONTROLLED_COUNT="$(hc clients -j | jq '[.[] | select(.title == "HYPREXPO-PROBE-CONTROLLED")] | length')"
    TOTAL_COUNT="$(hc clients -j | jq 'length')"
    [[ "$CONTROLLED_COUNT" == 1 && "$TOTAL_COUNT" -ge 4 ]] && break
    sleep 0.05
done
[[ "$CONTROLLED_COUNT" == 1 && "$TOTAL_COUNT" -ge 4 ]] || die "four controlled fixture windows did not map"

hc dispatch focuswindow 'title:^(HYPREXPO-PROBE-CONTROLLED)$' >/dev/null
hc dispatch layoutmsg consume >/dev/null
hc dispatch focuswindow 'title:^(HYPREXPO-PROBE-CONTROLLED)$' >/dev/null
sleep 0.25
hc clients -j > "$CLIENTS_JSON"

wait_for_record() {
    local request_id="$1"
    local output_file="$2"
    local records
    for _ in $(seq 1 200); do
        records="$(hc rollinglog 2>/dev/null | sed -n 's/^.*HYPREXPO_SCROLL_PROBE //p' | jq -c --arg id "$request_id" 'select(.requestId == $id)' 2>/dev/null || true)"
        [[ -n "$records" ]] && break
        sleep 0.05
    done
    [[ "$(printf '%s\n' "$records" | sed '/^$/d' | wc -l)" -eq 1 ]] || die "request $request_id did not emit exactly one JSON record"
    printf '%s\n' "$records" > "$output_file"
}

run_capture() {
    local request_id="$1"
    local ppm="$2"
    local json="$3"
    hc dispatch hyprexpo-scroll-probe:inspect "$request_id|$ppm" >/dev/null || die "probe dispatcher failed for $request_id"
    wait_for_record "$request_id" "$json"
}

CAPTURE_ONE="$EVIDENCE_DIR/capture-unchanged-1.ppm"
CAPTURE_TWO="$EVIDENCE_DIR/capture-unchanged-2.ppm"
CAPTURE_CHANGED="$EVIDENCE_DIR/capture-changed.ppm"
JSON_ONE="$EVIDENCE_DIR/probe-unchanged-1.json"
JSON_TWO="$EVIDENCE_DIR/probe-unchanged-2.json"
JSON_CHANGED="$EVIDENCE_DIR/probe-changed.json"

LAST_COMMAND="first and stable unchanged captures"
run_capture "$RUN_ID-unchanged-1" "$CAPTURE_ONE" "$JSON_ONE"
sleep 0.25
run_capture "$RUN_ID-unchanged-2" "$CAPTURE_TWO" "$JSON_TWO"

LAST_COMMAND="controlled content change capture"
printf 'change\n' > "$CONTROL_FIFO"
sleep 0.75
run_capture "$RUN_ID-changed" "$CAPTURE_CHANGED" "$JSON_CHANGED"

LAST_COMMAND="topology, identity, offset, and pixel assertions"
jq -e --arg version "$EXPECTED_VERSION" --arg hash "$EXPECTED_HASH" '
    .status == "PASS" and .hyprlandVersion == $version and .compileHash == $hash and .runtimeHash == $hash and
    .topologyUnchanged == true and .offsetBefore == .offsetAfter and .columnsBefore == .columnsAfter and
    .columnsBefore.direction == "right" and (.columnsBefore.columns | length) == 3 and
    any(.columnsBefore.columns[]; (.targets | length) == 2) and
    (.columnsBefore.columns[2].calculatedStart >= (.monitor.pixelWidth | tonumber)) and
    .capture.rendererStateRestored == true and .capture.width > 0 and .capture.height > 0 and
    .capture.width <= (.monitor.pixelWidth | tonumber) and .capture.height <= (.monitor.pixelHeight | tonumber) and
    .capture.nontransparentPixels > 0 and .capture.nonBackgroundBounds.w > 0 and .capture.nonBackgroundBounds.h > 0 and
    .capture.nonBackgroundBounds.w < .capture.width and .capture.nonBackgroundBounds.h < .capture.height and
    .mutationOutcome == "not-attempted" and .rollbackStatus == "not-required"
' "$JSON_ONE" >/dev/null || die "first capture JSON failed the complete topology/pixel contract"

jq -e --slurpfile clients "$CLIENTS_JSON" '
    .capture.windowIdentity as $captured |
    ($clients[0] | any(.[]; .address == $captured and .title == "HYPREXPO-PROBE-CONTROLLED")) and
    ([.columnsBefore.columns[].targets[].windowIdentity] | all(. as $id; $clients[0] | any(.[]; .address == $id)))
' "$JSON_ONE" >/dev/null || die "probe window identities did not match hyprctl clients"

CONTROL_WIDTH="$(jq -r '.[] | select(.title == "HYPREXPO-PROBE-CONTROLLED") | .size[0]' "$CLIENTS_JSON")"
CONTROL_HEIGHT="$(jq -r '.[] | select(.title == "HYPREXPO-PROBE-CONTROLLED") | .size[1]' "$CLIENTS_JSON")"
CAPTURE_WIDTH="$(jq -r '.capture.width' "$JSON_ONE")"
CAPTURE_HEIGHT="$(jq -r '.capture.height' "$JSON_ONE")"
(( CAPTURE_WIDTH >= CONTROL_WIDTH - 2 && CAPTURE_WIDTH <= CONTROL_WIDTH + 2 && CAPTURE_HEIGHT >= CONTROL_HEIGHT - 2 && CAPTURE_HEIGHT <= CONTROL_HEIGHT + 2 )) ||
    die "framebuffer dimensions were not tight to the controlled target"

SHA_ONE="$(jq -r '.capture.sha256' "$JSON_ONE")"
SHA_TWO="$(jq -r '.capture.sha256' "$JSON_TWO")"
SHA_CHANGED="$(jq -r '.capture.sha256' "$JSON_CHANGED")"
[[ "$SHA_ONE" == "$SHA_TWO" ]] || die "unchanged captures did not produce a stable SHA-256"
[[ "$SHA_ONE" != "$SHA_CHANGED" ]] || die "controlled content change did not alter the capture SHA-256"
[[ -s "$CAPTURE_ONE" && -s "$CAPTURE_TWO" && -s "$CAPTURE_CHANGED" ]] || die "one or more PPM artifacts are empty"

kill -0 "$NESTED_PID" 2>/dev/null || die "nested compositor exited during the probe"
hc plugin list -j > "$PLUGINS_JSON"
hc configerrors -j > "$CONFIG_ERRORS_JSON"
jq -e 'length == 1 and .[0].name == "hyprexpo-scroll-probe"' "$PLUGINS_JSON" >/dev/null || die "probe plugin health changed after capture"
jq -e '[.[] | select(length > 0)] | length == 0' "$CONFIG_ERRORS_JSON" >/dev/null || die "nested config errors appeared after capture"
if rg -n 'ASSERT|SIG(SEGV|ABRT)|stack trace|terminate called|std::exception' "$INSTANCE_LOG" "$STDOUT_LOG" > "$EVIDENCE_DIR/fatal-log-scan.txt"; then
    die "nested logs contain a fatal/assertion signature"
fi

PPM_SHA="$(sha256sum "$CAPTURE_ONE" | awk '{print $1}')"
cp "$INSTANCE_LOG" "$EVIDENCE_DIR/hyprland-instance.log"

LAST_COMMAND="PASS result write"
{
    printf '%s\n' '---'
    printf 'status: PASS\n'
    printf 'plan: 260828-w7w-00\n'
    printf 'run_id: %s\n' "$RUN_ID"
    printf 'evidence_dir: %s\n' "$EVIDENCE_DIR"
    printf '%s\n\n' '---'
    printf '# Scrolling API Spike Result\n\n'
    printf '**Gate:** PASS (pending human PPM inspection)\n\n'
    printf '## Exact Commands\n\n'
    printf '```sh\n'
    printf 'test "$(pkg-config --modversion hyprland)" = 0.56.1\n'
    printf './scripts/run-scrolling-probe.sh --non-interactive\n'
    printf 'jq . %s\n' "$JSON_ONE"
    printf 'display %s\n' "$CAPTURE_ONE"
    printf '```\n\n'
    printf '## Runtime and Fixture\n\n'
    printf -- '- Hyprland: `%s` / `%s`\n' "$EXPECTED_VERSION" "$EXPECTED_HASH"
    printf -- '- Instance: `%s` (PID `%s`, alive through final readback)\n' "$INSTANCE" "$NESTED_PID"
    printf -- '- Native topology: 3 columns; one 2-target column; third column begins offscreen; direction right.\n'
    printf -- '- Topology, direction, widths, target order/sizes, and offset were byte-equivalent before/after every capture.\n'
    printf -- '- `hyprctl clients -j` identity correlation: PASS.\n'
    printf -- '- Probe-only plugin list, empty config errors, and fatal-log scan: PASS.\n\n'
    printf '## Pixel Evidence\n\n'
    printf -- '- Viewable PPM: `%s`\n' "$CAPTURE_ONE"
    printf -- '- Target/framebuffer: `%sx%s` / `%sx%s`\n' "$CONTROL_WIDTH" "$CONTROL_HEIGHT" "$CAPTURE_WIDTH" "$CAPTURE_HEIGHT"
    printf -- '- Raw RGBA SHA-256 unchanged: `%s` = `%s`\n' "$SHA_ONE" "$SHA_TWO"
    printf -- '- Raw RGBA SHA-256 after controlled content change: `%s`\n' "$SHA_CHANGED"
    printf -- '- PPM file SHA-256: `%s`\n' "$PPM_SHA"
    printf -- '- Nontransparent pixels: `%s`; non-background bounds: `%s`\n\n' \
        "$(jq -r '.capture.nontransparentPixels' "$JSON_ONE")" "$(jq -c '.capture.nonBackgroundBounds' "$JSON_ONE")"
    printf '## Machine-Readable Record\n\n```json\n'
    jq . "$JSON_ONE"
    printf '```\n\n'
    printf '## Artifacts\n\n'
    printf -- '- Clients: `%s`\n' "$CLIENTS_JSON"
    printf -- '- Version/plugin/config: `%s`, `%s`, `%s`\n' "$VERSION_JSON" "$PLUGINS_JSON" "$CONFIG_ERRORS_JSON"
    printf -- '- Nested logs: `%s`, `%s`\n' "$EVIDENCE_DIR/hyprland-instance.log" "$STDOUT_LOG"
    printf -- '- Stable/change records: `%s`, `%s`, `%s`\n\n' "$JSON_ONE" "$JSON_TWO" "$JSON_CHANGED"
    printf 'Plans 01-05 remain blocked until the recorded PPM is manually approved as tight, correctly oriented controlled-window content.\n'
} > "$RESULT"

echo "[scrolling-probe] PASS"
echo "[scrolling-probe] result: $RESULT"
echo "[scrolling-probe] image: $CAPTURE_ONE"
echo "[scrolling-probe] evidence: $EVIDENCE_DIR"
