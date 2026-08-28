#!/usr/bin/env bash
set -Eeuo pipefail

EXPECTED_VERSION="0.56.1"
EXPECTED_HASH="5c9377c15f85c50648f35ca5a213754f95b93ca0"
EXPECTED_BASE="f3ed01d3b024e404563e7ce18efdf1583aaa8cba"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ARTIFACT_ROOT="$REPO_ROOT/.planning/quick/260828-w7w-implement-issue-85-provide-a-niri-like-o"
RESULT="$ARTIFACT_ROOT/260828-w7w-00R-RESULT.md"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)-$$"
EVIDENCE_DIR="/tmp/hyprexpo-scroll-probe-recovery-$RUN_ID"
BUILD_DIR="$EVIDENCE_DIR/build"
CONFIG="$EVIDENCE_DIR/hyprland.conf"
STDOUT_LOG="$EVIDENCE_DIR/hyprland-stdout.log"
GUARD_LOG="$EVIDENCE_DIR/recovery-guards.log"
INSTANCE_LOG=""
INSTANCE=""
NESTED_SOCKET=""
NESTED_OUTPUT=""
NESTED_PID=""
PENDING_ID=""
PENDING_GENERATION=""
LAST_COMMAND="initialization"
CLIENT_PIDS=()
declare -A CLIENT_FIFOS=()

usage() {
    echo "usage: $0 --non-interactive --gpu-present-recovery" >&2
    exit 2
}

[[ "${1:-}" == "--non-interactive" && "${2:-}" == "--gpu-present-recovery" && $# -eq 2 ]] || usage
cd "$REPO_ROOT"

full_guard() {
    local stage="$1"
    {
        printf 'stage=%s utc=%s head=%s\n' "$stage" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$(git rev-parse HEAD)"
        test "$(git branch --show-current)" = feat/scrolling-overview-85
        test "$(git rev-parse master)" = "$EXPECTED_BASE"
        test "$(git rev-parse origin/master)" = "$EXPECTED_BASE"
        test "$(git merge-base origin/master HEAD)" = "$EXPECTED_BASE"
        test "$(sha256sum "$ARTIFACT_ROOT/260828-w7w-00-SPIKE-RESULT.md" | cut -d' ' -f1)" = db9d1004a5a3537c3684a2ec9662b2a1dc67fa1668d1c0765974a4a400f45791
        test "$(sha256sum "$ARTIFACT_ROOT/260828-w7w-00-SUMMARY.md" | cut -d' ' -f1)" = 4d5ba80d7bfcfd834b64bffcab6a74c0b3d678ba43f26aa264f59f035fe8fca3
        test "$(sha256sum "$ARTIFACT_ROOT/260828-w7w-00-RECOVERY.md" | cut -d' ' -f1)" = f3511a276287317de2a44234fa12b406f943f0f7157c5f161673e00f258213dc
        git merge-base --is-ancestor a890a0d e9c5acc
        git merge-base --is-ancestor e9c5acc 5d56b13
        git merge-base --is-ancestor 5d56b13 HEAD
        grep -q '^status: FAIL$' "$ARTIFACT_ROOT/260828-w7w-00-SPIKE-RESULT.md"
        printf 'result=PASS\n'
    } >> "${GUARD_LOG:-/dev/null}" 2>&1
}

# A changed pinned artifact is an integrity stop, not a new empirical result.
mkdir -p "$EVIDENCE_DIR"
if ! full_guard initial; then
    echo "[scrolling-probe] GUARD_MISMATCH before launch; 00R result was not rewritten" >&2
    exit 1
fi

write_fail() {
    local reason="$1"
    mkdir -p "$(dirname "$RESULT")"
    {
        printf '%s\n' '---'
        printf 'status: FAIL\n'
        printf 'plan: 260828-w7w-00R\n'
        printf 'run_id: %s\n' "$RUN_ID"
        printf 'evidence_dir: %s\n' "$EVIDENCE_DIR"
        printf '%s\n\n' '---'
        printf '# Scrolling GPU Presentation Recovery Result\n\n'
        printf '**Gate:** FAIL — Plans 01-05 remain blocked.\n\n'
        printf '**Failed command/stage:** `%s`\n\n' "$LAST_COMMAND"
        printf '**Evidence:** %s\n\n' "$reason"
        printf 'The immutable Plan 00 result remains FAIL. No downstream plan is authorized.\n'
    } > "$RESULT"
}

die() {
    trap - ERR
    write_fail "$1"
    echo "[scrolling-probe] FAIL: $1" >&2
    exit 1
}

die_primary() {
    trap - ERR
    write_fail "PRIMARY_GPU_COMPOSITION_FAIL: $1"
    echo "[scrolling-probe] PRIMARY_GPU_COMPOSITION_FAIL: $1" >&2
    exit 1
}

guard_or_abort() {
    if ! full_guard "$1"; then
        trap - ERR
        echo "[scrolling-probe] GUARD_MISMATCH at $1; 00R result was not rewritten" >&2
        exit 1
    fi
}

hc() {
    HYPRLAND_INSTANCE_SIGNATURE="$INSTANCE" hyprctl "$@"
}

attempt_pending_ack() {
    if [[ -n "$PENDING_ID" && -n "$PENDING_GENERATION" && -n "$INSTANCE" ]]; then
        hc dispatch hyprexpo-scroll-probe:ack "ack|$PENDING_ID|$PENDING_GENERATION" >/dev/null 2>&1 || true
        PENDING_ID=""
        PENDING_GENERATION=""
    fi
}

cleanup() {
    set +e
    attempt_pending_ack
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
trap 'die "unexpected command failure at script line $LINENO"' ERR

for command in jq kitty grim python sha256sum cmake; do
    command -v "$command" >/dev/null || die "$command is required for recovery evidence"
done
[[ "$(pkg-config --modversion hyprland)" == "$EXPECTED_VERSION" ]] || die "pkg-config hyprland version is not $EXPECTED_VERSION"

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

guard_or_abort before-nested-launch
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
for _ in $(seq 1 200); do
    hc monitors -j >/dev/null 2>&1 && break
    kill -0 "$NESTED_PID" 2>/dev/null || die "nested Hyprland exited before control socket readiness"
    sleep 0.05
done
hc monitors -j >/dev/null 2>&1 || die "nested Hyprland control socket did not become ready"

hc version -j > "$EVIDENCE_DIR/version.json"
hc plugin list -j > "$EVIDENCE_DIR/plugins-initial.json"
hc configerrors -j > "$EVIDENCE_DIR/config-errors-initial.json"
jq -e --arg version "$EXPECTED_VERSION" --arg hash "$EXPECTED_HASH" '.version == $version and .commit == $hash and (.dirty == false)' "$EVIDENCE_DIR/version.json" >/dev/null ||
    die "nested runtime version/hash did not match the exact probe ABI"
jq -e 'length == 1 and .[0].name == "hyprexpo-scroll-probe"' "$EVIDENCE_DIR/plugins-initial.json" >/dev/null || die "nested session did not load only the probe plugin"
jq -e '[.[] | select(length > 0)] | length == 0' "$EVIDENCE_DIR/config-errors-initial.json" >/dev/null || die "nested configuration reported errors"
NESTED_SOCKET="$(hyprctl instances -j | jq -r --argjson pid "$NESTED_PID" '.[] | select(.pid == $pid) | .wl_socket')"
NESTED_OUTPUT="$(hc monitors -j | jq -r '.[0].name')"
[[ -n "$NESTED_SOCKET" && -n "$NESTED_OUTPUT" ]] || die "nested Wayland socket/output was not discoverable"

launch_pattern_client() {
    local index="$1"
    local title="HYPREXPO-PROBE-$index"
    local fifo="$EVIDENCE_DIR/client-$index.fifo"
    mkfifo "$fifo"
    CLIENT_FIFOS["$title"]="$fifo"
    env WAYLAND_DISPLAY="$NESTED_SOCKET" PROBE_FIFO="$fifo" PROBE_INDEX="$index" kitty --class "hyprexpo-probe-$index" --title "$title" \
        --override cursor_blink_interval=0 sh -lc '
            case "$PROBE_INDEX" in
                1) bg="16;24;48"; fg="20;220;100" ;;
                2) bg="70;18;96"; fg="240;190;20" ;;
                3) bg="8;78;116"; fg="240;80;60" ;;
                *) bg="92;38;12"; fg="80;210;240" ;;
            esac
            printf "\033[2J\033[H\033[?25l\033[48;2;%sm  GPU TARGET %s  \033[0m\n\n\033[48;2;%sm UNIQUE BLOCK %s UNIQUE BLOCK \033[0m\n\nalpha-%s beta-%s gamma-%s\n" "$bg" "$PROBE_INDEX" "$fg" "$PROBE_INDEX" "$PROBE_INDEX" "$PROBE_INDEX" "$PROBE_INDEX"
            while IFS= read -r command; do
                if [ "$command" = change ]; then
                    printf "\033[2J\033[H\033[?25l\033[48;2;150;20;40m  CHANGED GPU TARGET %s  \033[0m\n\n\033[48;2;20;80;220m BLUE CHANGED BLOCK %s BLUE CHANGED \033[0m\n\ndelta-%s epsilon-%s zeta-%s\n" "$PROBE_INDEX" "$PROBE_INDEX" "$PROBE_INDEX" "$PROBE_INDEX" "$PROBE_INDEX"
                fi
            done < "$PROBE_FIFO"' &
    CLIENT_PIDS+=("$!")
}

LAST_COMMAND="controlled scrolling fixture creation"
for index in 1 2 3 4; do
    launch_pattern_client "$index"
    for _ in $(seq 1 100); do
        [[ "$(hc clients -j | jq 'length')" -ge "$index" ]] && break
        sleep 0.05
    done
done
for _ in $(seq 1 200); do
    [[ "$(hc clients -j | jq '[.[] | select(.title | startswith("HYPREXPO-PROBE-"))] | length')" == 4 ]] && break
    sleep 0.05
done
[[ "$(hc clients -j | jq 'length')" -ge 4 ]] || die "four controlled fixture windows did not map"
hc dispatch focuswindow 'title:^(HYPREXPO-PROBE-1)$' >/dev/null
hc dispatch layoutmsg consume >/dev/null
hc dispatch focuswindow 'title:^(HYPREXPO-PROBE-1)$' >/dev/null
sleep 0.5
hc clients -j > "$EVIDENCE_DIR/clients-fixture.json"

capture_state() {
    local path="$1"
    hc clients -j > "$path.clients"
    hc monitors -j > "$path.monitors"
    hc activeworkspace -j > "$path.workspace"
    hc activewindow -j > "$path.window"
    jq -n --slurpfile clients "$path.clients" --slurpfile monitors "$path.monitors" --slurpfile workspace "$path.workspace" --slurpfile window "$path.window" '
        {clients: ($clients[0] | map({address,title,class,workspace,at,size,floating,pinned}) | sort_by(.address)),
         monitors: ($monitors[0] | map({id,name,activeWorkspace}) | sort_by(.id)),
         activeWorkspace: ($workspace[0] | {id,name}), activeWindow: ($window[0] | {address,title,class})}' > "$path"
}

wait_for_record() {
    local request_id="$1"
    local stage="$2"
    local output_file="$3"
    local records=""
    for _ in $(seq 1 240); do
        records="$(
            { hc rollinglog 2>/dev/null || true; tail -n 4000 "$INSTANCE_LOG" 2>/dev/null || true; } |
                sed -n 's/^.*HYPREXPO_SCROLL_PROBE //p' | sort -u |
                jq -c --arg id "$request_id" --arg stage "$stage" 'select(.requestId == $id and .stage == $stage)' 2>/dev/null || true
        )"
        [[ -n "$records" ]] && break
        sleep 0.05
    done
    [[ "$(printf '%s\n' "$records" | sed '/^$/d' | wc -l)" -eq 1 ]] || die "request $request_id did not emit exactly one $stage record"
    printf '%s\n' "$records" > "$output_file"
}

ppm_analyze() {
    local ppm="$1"
    local output="$2"
    local sample_x="${3:--1}"
    local sample_y="${4:--1}"
    python - "$ppm" "$output" "$sample_x" "$sample_y" <<'PY'
import collections, json, sys
src, dst, sx, sy = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
data = open(src, 'rb').read()
pos = 0
def token():
    global pos
    while pos < len(data):
        if data[pos:pos+1] == b'#':
            pos = data.index(b'\n', pos) + 1
        elif data[pos] in b' \t\r\n': pos += 1
        else: break
    start = pos
    while pos < len(data) and data[pos] not in b' \t\r\n': pos += 1
    return data[start:pos]
if token() != b'P6': raise SystemExit('not binary PPM')
w, h, maximum = int(token()), int(token()), int(token())
while pos < len(data) and data[pos] in b' \t\r\n': pos += 1
pixels = data[pos:]
if maximum != 255 or len(pixels) != w*h*3: raise SystemExit('invalid PPM payload')
colors = [tuple(pixels[i:i+3]) for i in range(0, len(pixels), 3)]
counts = collections.Counter(colors)
dominant, dominant_count = counts.most_common(1)[0]
different = [(i % w, i // w) for i, color in enumerate(colors) if color != dominant]
bounds = None if not different else {'x': min(x for x,_ in different), 'y': min(y for _,y in different),
    'w': max(x for x,_ in different)-min(x for x,_ in different)+1,
    'h': max(y for _,y in different)-min(y for _,y in different)+1}
sample = None if sx < 0 or sy < 0 or sx >= w or sy >= h else list(colors[sy*w+sx])
json.dump({'width':w,'height':h,'uniqueColors':len(counts),'dominant':list(dominant),'dominantCount':dominant_count,
           'nonDominantCount':len(different),'nonDominantBounds':bounds,'sample':sample}, open(dst,'w'))
PY
}

validate_marker() {
    local ready="$1"
    local analysis="$2"
    python - "$ready" "$analysis" <<'PY'
import json, sys
r, a = json.load(open(sys.argv[1])), json.load(open(sys.argv[2]))
expected = tuple(bytes.fromhex(r['marker']['color'][1:]))
sample = tuple(a['sample'])
if any(abs(x-y) > 3 for x,y in zip(expected, sample)):
    raise SystemExit(f'marker mismatch expected={expected} sample={sample}')
PY
}

validate_tight() {
    local ready="$1"
    local analysis="$2"
    python - "$ready" "$analysis" <<'PY'
import json, sys
r, a = json.load(open(sys.argv[1])), json.load(open(sys.argv[2]))
if a['width'] != r['capture']['width'] or a['height'] != r['capture']['height']:
    raise SystemExit('tight PPM dimensions do not match retained FBO')
if a['uniqueColors'] < 3 or a['nonDominantCount'] <= 0 or not a['nonDominantBounds']:
    raise SystemExit('tight PPM is blank or uniform')
b = a['nonDominantBounds']
if b['x'] < 0 or b['y'] < 0 or b['x'] + b['w'] > a['width'] or b['y'] + b['h'] > a['height']:
    raise SystemExit('tight PPM content bounds escape the crop')
PY
}

declare -A READY_FILES=() DONE_FILES=() TIGHT_FILES=() FULL_FILES=() RESTORED_FILES=() STATE_BEFORE=() STATE_AFTER=() TIGHT_SHAS=()

run_request() {
    local label="$1"
    local selector="$2"
    local request_id="$RUN_ID-$label"
    local ready="$EVIDENCE_DIR/$label-ready.json"
    local done="$EVIDENCE_DIR/$label-done.json"
    local before="$EVIDENCE_DIR/$label-state-before.json"
    local after="$EVIDENCE_DIR/$label-state-after.json"
    local full="$EVIDENCE_DIR/$label-full.ppm"
    local tight="$EVIDENCE_DIR/$label-tight.ppm"
    local restored="$EVIDENCE_DIR/$label-restored.ppm"
    local full_analysis="$EVIDENCE_DIR/$label-full-analysis.json"
    local tight_analysis="$EVIDENCE_DIR/$label-tight-analysis.json"
    local restored_analysis="$EVIDENCE_DIR/$label-restored-analysis.json"

    guard_or_abort "before-request-$label"
    capture_state "$before"
    LAST_COMMAND="present $label"
    hc dispatch hyprexpo-scroll-probe:inspect "present|$request_id|$selector" >/dev/null || die "present dispatcher failed for $label"
    wait_for_record "$request_id" READY "$ready"
    PENDING_ID="$request_id"
    PENDING_GENERATION="$(jq -r '.sessionGeneration' "$ready")"
    jq -e --arg id "$request_id" --arg selector "$selector" '
        .status == "READY" and .requestId == $id and .capture.rendererStateRestored == true and
        .pendingFramebuffer == true and .pendingTexture == true and
        (.target.visibility == "visible" or .target.visibility == "offscreen") and
        (.capture.width > 0 and .capture.height > 0) and
        (if $selector == "offscreen" then .target.visibility == "offscreen" else true end)
    ' "$ready" >/dev/null || die "READY contract failed for $label"
    jq -e --slurpfile clients "$EVIDENCE_DIR/clients-fixture.json" '.target.windowIdentity as $id | $clients[0] | any(.[]; .address == $id and .title != "")' "$ready" >/dev/null ||
        die "READY target identity did not correlate to fixture clients for $label"

    local crop_x crop_y crop_w crop_h marker_x marker_y
    crop_x="$(jq -r '.capture.logicalCropBox.x | round' "$ready")"
    crop_y="$(jq -r '.capture.logicalCropBox.y | round' "$ready")"
    crop_w="$(jq -r '.capture.logicalCropBox.w | round' "$ready")"
    crop_h="$(jq -r '.capture.logicalCropBox.h | round' "$ready")"
    marker_x="$(jq -r '.marker.box.x + (.marker.box.w / 2) | floor' "$ready")"
    marker_y="$(jq -r '.marker.box.y + (.marker.box.h / 2) | floor' "$ready")"
    LAST_COMMAND="nested grim capture $label"
    local marker_ok=false
    for _ in $(seq 1 20); do
        env WAYLAND_DISPLAY="$NESTED_SOCKET" grim -o "$NESTED_OUTPUT" -t ppm "$full"
        ppm_analyze "$full" "$full_analysis" "$marker_x" "$marker_y"
        if validate_marker "$ready" "$full_analysis" 2>/dev/null; then marker_ok=true; break; fi
        sleep 0.05
    done
    [[ "$marker_ok" == true ]] || die "generation marker was not observable for $label"
    env WAYLAND_DISPLAY="$NESTED_SOCKET" grim -g "$crop_x,$crop_y ${crop_w}x${crop_h}" -t ppm "$tight"
    ppm_analyze "$tight" "$tight_analysis"
    validate_tight "$ready" "$tight_analysis" || die_primary "retained target texture was blank, uniform, corrupt, or wrongly sized for $label"

    LAST_COMMAND="ack and restore $label"
    hc dispatch hyprexpo-scroll-probe:ack "ack|$request_id|$PENDING_GENERATION" >/dev/null || die "matching ack failed for $label"
    wait_for_record "$request_id" DONE "$done"
    PENDING_ID=""
    PENDING_GENERATION=""
    jq -e '.status == "PASS" and .topologyUnchanged == true and .activeWorkspaceBefore == .activeWorkspaceAfter and
        .focusedWindowBefore == .focusedWindowAfter and .pendingGeneration == null and .pendingFramebuffer == false and
        .pendingTexture == false and .pendingOverlay == false and .pendingPassCount == 0 and .markerAbsentFromQueuedPass == true' "$done" >/dev/null ||
        die "DONE cleanup/state contract failed for $label"
    guard_or_abort "after-ack-$label"
    sleep 0.15
    capture_state "$after"
    cmp <(jq -S . "$before") <(jq -S . "$after") >/dev/null || die "hyprctl state changed across $label"
    env WAYLAND_DISPLAY="$NESTED_SOCKET" grim -o "$NESTED_OUTPUT" -t ppm "$restored"
    ppm_analyze "$restored" "$restored_analysis" "$marker_x" "$marker_y"
    if validate_marker "$ready" "$restored_analysis" 2>/dev/null; then die "request marker remained after ack for $label"; fi
    kill -0 "$NESTED_PID" 2>/dev/null || die "nested compositor exited during $label"

    READY_FILES["$label"]="$ready"
    DONE_FILES["$label"]="$done"
    FULL_FILES["$label"]="$full"
    TIGHT_FILES["$label"]="$tight"
    RESTORED_FILES["$label"]="$restored"
    STATE_BEFORE["$label"]="$before"
    STATE_AFTER["$label"]="$after"
    TIGHT_SHAS["$label"]="$(sha256sum "$tight" | awk '{print $1}')"
}

LAST_COMMAND="visible stable generations"
run_request visible-a1 visible
VISIBLE_ID="$(jq -r '.target.windowIdentity' "${READY_FILES[visible-a1]}")"
run_request visible-a2 "$VISIBLE_ID"
[[ "${TIGHT_SHAS[visible-a1]}" == "${TIGHT_SHAS[visible-a2]}" ]] || die "unchanged visible target SHA-256 was unstable"

LAST_COMMAND="offscreen stable generations"
run_request offscreen-b1 offscreen
OFFSCREEN_ID="$(jq -r '.target.windowIdentity' "${READY_FILES[offscreen-b1]}")"
OFFSCREEN_TITLE="$(jq -r '.target.title' "${READY_FILES[offscreen-b1]}")"
[[ "$OFFSCREEN_ID" != "$VISIBLE_ID" ]] || die "visible and offscreen selectors resolved the same target"
run_request offscreen-b2 "$OFFSCREEN_ID"
[[ "${TIGHT_SHAS[offscreen-b1]}" == "${TIGHT_SHAS[offscreen-b2]}" ]] || die "unchanged offscreen target SHA-256 was unstable"

LAST_COMMAND="controlled offscreen content change"
[[ -n "${CLIENT_FIFOS[$OFFSCREEN_TITLE]:-}" ]] || die "offscreen target title did not map to a controlled FIFO"
printf 'change\n' > "${CLIENT_FIFOS[$OFFSCREEN_TITLE]}"
sleep 0.75
run_request offscreen-b-changed "$OFFSCREEN_ID"
[[ "${TIGHT_SHAS[offscreen-b1]}" != "${TIGHT_SHAS[offscreen-b-changed]}" ]] || die "controlled offscreen content change did not alter tight SHA-256"

LAST_COMMAND="final health and unload"
hc plugin list -j > "$EVIDENCE_DIR/plugins-before-unload.json"
hc configerrors -j > "$EVIDENCE_DIR/config-errors-final.json"
jq -e 'length == 1 and .[0].name == "hyprexpo-scroll-probe"' "$EVIDENCE_DIR/plugins-before-unload.json" >/dev/null || die "probe plugin health changed after generations"
jq -e '[.[] | select(length > 0)] | length == 0' "$EVIDENCE_DIR/config-errors-final.json" >/dev/null || die "nested config errors appeared"
if rg -n 'ASSERT|SIG(SEGV|ABRT)|stack trace|terminate called|std::exception|resource leak' "$INSTANCE_LOG" "$STDOUT_LOG" > "$EVIDENCE_DIR/fatal-log-scan.txt"; then
    die "nested logs contain a fatal/assertion/resource-leak signature"
fi
hc plugin unload "$PROBE_SO" >/dev/null || die "probe plugin unload failed"
sleep 0.15
hc plugin list -j > "$EVIDENCE_DIR/plugins-after-unload.json"
jq -e 'length == 0' "$EVIDENCE_DIR/plugins-after-unload.json" >/dev/null || die "probe plugin remained loaded after unload"
kill -0 "$NESTED_PID" 2>/dev/null || die "nested compositor exited during unload"
guard_or_abort after-final-unload
cp "$INSTANCE_LOG" "$EVIDENCE_DIR/hyprland-instance.log"

LAST_COMMAND="PASS result write"
{
    printf '%s\n' '---'
    printf 'status: PASS\n'
    printf 'plan: 260828-w7w-00R\n'
    printf 'run_id: %s\n' "$RUN_ID"
    printf 'evidence_dir: %s\n' "$EVIDENCE_DIR"
    printf '%s\n\n' '---'
    printf '# Scrolling GPU Presentation Recovery Result\n\n'
    printf '**Gate:** PASS — pending exact human approval; Plans 01-05 remain blocked until approval.\n\n'
    printf '## Runtime Contract\n\n'
    printf -- '- Hyprland `%s` / `%s`; nested PID `%s` survived capture and unload.\n' "$EXPECTED_VERSION" "$EXPECTED_HASH" "$NESTED_PID"
    printf -- '- Probe used only retained GPU framebuffer textures; grim used nested `WAYLAND_DISPLAY=%s` and output `%s`.\n' "$NESTED_SOCKET" "$NESTED_OUTPUT"
    printf -- '- Five request generations reached READY -> marker/full+tight PPM -> ack -> marker-free DONE/restored PPM.\n'
    printf -- '- Every generation preserved canonical direction/offset/column widths/target membership and exact active-workspace/focus/client state.\n'
    printf -- '- Probe unload left zero plugins, an alive compositor, empty config errors, and a clean fatal-log scan.\n\n'
    printf '## Pixel Evidence\n\n'
    for label in visible-a1 visible-a2 offscreen-b1 offscreen-b2 offscreen-b-changed; do
        printf -- '- `%s`: target `%s` / `%s` (%s), generation `%s`, marker `%s`, tight `%s`, SHA `%s`, full `%s`, restored `%s`.\n' \
            "$label" "$(jq -r '.target.windowIdentity' "${READY_FILES[$label]}")" "$(jq -r '.target.title' "${READY_FILES[$label]}")" \
            "$(jq -r '.target.visibility' "${READY_FILES[$label]}")" "$(jq -r '.sessionGeneration' "${READY_FILES[$label]}")" \
            "$(jq -r '.marker.color' "${READY_FILES[$label]}")" "${TIGHT_FILES[$label]}" "${TIGHT_SHAS[$label]}" "${FULL_FILES[$label]}" "${RESTORED_FILES[$label]}"
    done
    printf '\n- Visible unchanged SHA equality: `%s`.\n' "${TIGHT_SHAS[visible-a1]}"
    printf -- '- Offscreen unchanged SHA equality: `%s`.\n' "${TIGHT_SHAS[offscreen-b1]}"
    printf -- '- Offscreen changed SHA: `%s` (different: PASS).\n\n' "${TIGHT_SHAS[offscreen-b-changed]}"
    printf '## Correlated Records\n\n'
    for label in visible-a1 visible-a2 offscreen-b1 offscreen-b2 offscreen-b-changed; do
        printf -- '- `%s`: READY `%s`; DONE `%s`; state before/after `%s`, `%s`.\n' "$label" "${READY_FILES[$label]}" "${DONE_FILES[$label]}" "${STATE_BEFORE[$label]}" "${STATE_AFTER[$label]}"
    done
    printf '\n## Guards and Health\n\n'
    printf -- '- Recovery guard log: `%s`\n' "$GUARD_LOG"
    printf -- '- Runtime/plugin/config/log evidence: `%s`\n\n' "$EVIDENCE_DIR"
    printf 'The immutable Plan 00 result remains FAIL. Plan 01 may begin only after the exact signal `00R GPU capture approved`.\n'
} > "$RESULT"

echo "[scrolling-probe] PASS"
echo "[scrolling-probe] result: $RESULT"
echo "[scrolling-probe] evidence: $EVIDENCE_DIR"
