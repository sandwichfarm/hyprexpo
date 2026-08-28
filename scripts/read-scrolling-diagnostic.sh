#!/usr/bin/env bash
set -euo pipefail

readonly PREFIX='HYPREXPO_SCROLLING_DIAGNOSTIC '

valid_request_id() {
    [[ ${1:-} =~ ^[A-Za-z0-9._-]{1,64}$ ]]
}

extract_record() {
    local log_file=$1
    local request_id=$2

    valid_request_id "$request_id" || {
        printf 'invalid request ID\n' >&2
        return 2
    }
    [[ -f $log_file ]] || {
        printf 'diagnostic log does not exist: %s\n' "$log_file" >&2
        return 2
    }

    local matches
    matches=$(awk -v prefix="$PREFIX" -v id="\"requestId\":\"$request_id\"" '
        index($0, prefix) && index($0, id) {
            record = substr($0, index($0, prefix) + length(prefix))
            print record
        }
    ' "$log_file")

    local count
    count=$(printf '%s\n' "$matches" | awk 'NF { count++ } END { print count + 0 }')
    [[ $count -eq 1 ]] || {
        printf 'expected exactly one diagnostic record for %s, found %s\n' "$request_id" "$count" >&2
        return 1
    }
    printf '%s\n' "$matches"
}

source_contract() {
    local fixture_dir
    fixture_dir=$(mktemp -d)
    trap 'rm -rf -- "$fixture_dir"' RETURN
    local fixture="$fixture_dir/nested.log"
    printf '%s\n' \
        'noise' \
        'HYPREXPO_SCROLLING_DIAGNOSTIC {"requestId":"other","status":"PASS"}' \
        'HYPREXPO_SCROLLING_DIAGNOSTIC {"requestId":"wanted-1","status":"PASS"}' >"$fixture"
    [[ $(extract_record "$fixture" wanted-1) == '{"requestId":"wanted-1","status":"PASS"}' ]]
    ! extract_record "$fixture" 'bad id' >/dev/null 2>&1
    printf 'read-scrolling-diagnostic source contract PASS\n'
}

if [[ ${1:-} == --source-contract ]]; then
    source_contract
    exit 0
fi

[[ $# -eq 2 ]] || {
    printf 'usage: %s LOG_FILE REQUEST_ID\n' "$0" >&2
    exit 2
}
extract_record "$1" "$2"
