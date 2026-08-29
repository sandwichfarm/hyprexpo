#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

declare -A cases=(
    [hover-clear]='mouse_move:100:100|mouse_move:-1:-1'
    [non-primary-passthrough]='mouse_button:100:100:272:1'
    [mouse-click]='mouse_move:100:100|mouse_button:100:100:273:1|mouse_move:104:100|mouse_button:104:100:273:0'
    [same-column]='mouse_button:100:100:273:1|mouse_move:115:100|mouse_move:120:140|mouse_button:120:140:273:0'
    [new-column-before]='mouse_button:100:100:273:1|mouse_move:115:100|mouse_move:5:100|mouse_button:5:100:273:0'
    [new-column-after]='mouse_button:100:100:273:1|mouse_move:115:100|mouse_move:995:100|mouse_button:995:100:273:0'
    [cross-scrolling]='mouse_button:100:100:273:1|mouse_move:115:100|mouse_move:100:400|mouse_button:100:400:273:0'
    [mixed-workspace]='mouse_button:100:100:273:1|mouse_move:115:100|mouse_move:500:400|mouse_button:500:400:273:0'
    [terminal-workspace]='mouse_button:100:100:273:1|mouse_move:115:100|mouse_move:900:490|mouse_button:900:490:273:0'
    [axis-owned]='mouse_button:100:100:273:1|mouse_axis:100:100:120|mouse_button:100:100:273:0'
    [touch-pan]='touch_down:1:900:300|touch_motion:1:900:180|touch_up:1'
    [touch-tap]='touch_down:2:100:100|touch_motion:2:104:100|touch_up:2'
    [touch-drag]='touch_down:3:100:100|touch_motion:3:115:100|touch_motion:3:500:300|touch_up:3'
    [touch-cancel]='touch_down:4:100:100|touch_cancel:4'
    [mismatched-cancel]='touch_down:5:100:100|touch_cancel:6|touch_cancel:5'
    [post-cancel-reacquire]='touch_down:7:100:100|touch_cancel:7|mouse_button:100:100:273:1|reset:cancel'
    [stale-target]='touch_down:8:100:100|reset:refresh|touch_motion:8:120:100'
    [refresh-reset]='mouse_button:100:100:273:1|reset:refresh'
    [teardown-reset]='touch_down:9:900:300|reset:teardown|reset:teardown'
)

source_contract() {
    cd "$repo_root"
    local token
    for token in \
        'm_events.input.mouse.move.listen' \
        'm_events.input.mouse.button.listen' \
        'm_events.input.mouse.axis.listen' \
        'm_events.input.touch.down.listen' \
        'm_events.input.touch.motion.listen' \
        'm_events.input.touch.up.listen' \
        'm_events.input.touch.cancel.listen' \
        'info.cancelled = effects.consume' \
        'transitionInput(' \
        'touchToGlobalLogical('; do
        rg -Fq "$token" ScrollingOverview.cpp
    done
    rg -Fq 'plugin:hyprexpo:scrolling_input_debug' PluginConfig.cpp Dispatchers.cpp
    rg -Fq 'hyprexpo:scrolling_input_test' Dispatchers.cpp
    rg -Fq 'HYPREXPO_SCROLLING_INPUT {}' Dispatchers.cpp
    for token in "${!cases[@]}"; do
        rg -Fq "[$token]" scripts/inject-scrolling-input.sh
    done
    make test
    printf '%s\n' 'scrolling input source contract PASS'
}

if [[ ${1:-} == --source-contract ]]; then
    source_contract
    exit 0
fi

if [[ -z ${HYPRLAND_INSTANCE_SIGNATURE:-} ]]; then
    printf '%s\n' 'HYPRLAND_INSTANCE_SIGNATURE is required for runtime injection' >&2
    exit 1
fi

debug_value=$(hyprctl getoption plugin:hyprexpo:scrolling_input_debug -j | jq -r '.int // 0')
if [[ $debug_value != 1 ]]; then
    printf '%s\n' 'plugin:hyprexpo:scrolling_input_debug must be 1' >&2
    exit 1
fi

case_name=${1:-}
if [[ -z $case_name || -z ${cases[$case_name]+x} ]]; then
    printf 'usage: %s <%s>\n' "$0" "$(IFS='|'; printf '%s' "${!cases[*]}")" >&2
    exit 1
fi

request_id="${case_name}-requestId-$$"
sequence="$request_id|${cases[$case_name]}"
log_file="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/hypr/$HYPRLAND_INSTANCE_SIGNATURE/hyprland.log"
hyprctl dispatch hyprexpo:scrolling_input_test "$sequence" >/dev/null

for _ in {1..50}; do
    record=$(rg -F "HYPREXPO_SCROLLING_INPUT {\"requestId\":\"$request_id\"" "$log_file" | tail -1 || true)
    if [[ -n $record ]]; then
        json=${record#*HYPREXPO_SCROLLING_INPUT }
        jq -e --arg requestId "$request_id" '.requestId == $requestId and (.events | length) > 0' <<<"$json" >/dev/null
        printf '%s\n' "$json"
        exit 0
    fi
    sleep 0.02
done

printf 'no request-correlated input diagnostic found for %s\n' "$request_id" >&2
exit 1
