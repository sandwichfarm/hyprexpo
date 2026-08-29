# Scrolling Overview

HyprExpo detects Hyprland's native `scrolling` layout on the workspace where
the overview opens. A valid native snapshot selects the window-level scrolling
session; non-scrolling, empty, expired, or incompatible state falls back to the
existing workspace grid. The grid implementation itself has no scrolling mode
branches.

This is a focused Hyprland-native overview, not full Niri parity. It implements
the scrolling tape, individual window previews, input, panning, selection, and
safe positional moves. It does not implement hot corners, dwell activation,
layer-shell or wallpaper composition, or arbitrary insertion of workspaces.

## Scene and captures

Every ordinary workspace on the monitor becomes a vertical row. Native
scrolling rows preserve direction (`right`, `left`, `down`, or `up`), column
order, per-column width, target order and proportions, current native offset,
and targets beyond the visible viewport. Non-scrolling workspaces are mixed
rows rendered through the same workspace-capture boundary as the default grid.
Floating, grouped, fullscreen, and optionally pinned targets remain classified
in diagnostic state.

One terminal row represents only the next empty ordinary workspace. Dropping
there creates that workspace transactionally. There is no arbitrary workspace
insertion before, after, or between existing workspace IDs.

`plugin:hyprexpo:scrolling_thumbnail_budget` is the integer multiplier `m`
(`1..16`, default `4`). For monitor dimensions `W x H`, total requested target
pixels are bounded by `m * W * H`. Each target first receives a per-target cap;
when the total still exceeds the budget, all targets share the same
square-root scale, with a 16-pixel minimum fallback. The budget limits capture
resolution, not which offscreen targets exist in the scene.

## Pointer and touch contract

The runtime listens to the exact Hyprland signals `input.mouse.move`,
`input.mouse.button`, `input.mouse.axis`, `input.touch.down`,
`input.touch.motion`, `input.touch.up`, and `input.touch.cancel`.

- Mouse move updates exact-window hover enter/change/clear and damage without
  consuming ordinary hover motion or hiding the cursor.
- A primary press on a window is owned. Release below the 12-logical-pixel
  threshold is an exact click; crossing the threshold starts one drag proxy and
  release produces at most one drop.
- Primary press on the canvas owns a pan. Mouse axis inside the overview pans
  and is consumed; outside axis passes through. Axis during an owned drag is
  consumed without moving the native tape or applying an extra pan.
- Touch follows the same tap/threshold-drag model. A matching
  `input.touch.cancel` clears pending, pan, or drag ownership, selection, drop
  intent, hover, and proxy state. A mismatched ID passes through. Mouse or touch
  can reacquire immediately after cancel.
- Unowned buttons, unrelated touch IDs, and events outside the overview pass
  through. The effect returned by the pure input transition is the sole source
  of Hyprland event consumption.

Keyboard focus is spatial across windows and workspace rows. Confirm selects
the exact focused target/row. Cancel, close, refresh, reload, and teardown clear
pointer/touch ownership before replacing captures or destroying listeners, so
a later release cannot apply stale work.

## Positional drag and rollback

Within a native scrolling row, the center of a target is an existing-column
row insertion zone; the direction-aware low and high edges create a new column
before or after. The same-column reorder, existing-column row, new column
before, and new column after outcomes preserve normalized row proportions.
Dropping on another scrolling workspace performs a cross-workspace native
move; a mixed row uses the safe workspace fallback; the terminal row creates
only the next empty workspace. Outside release cancels, and same-position
release is a no-op.

Each release snapshots source and destination before removal, keeps strong
call-scoped ownership, re-resolves after controller moves, restores exact
column widths and normalized row sizes, recalculates, then checks global
exact-once membership and order. Any injected or host failure enters rollback.
Successful rollback must structurally equal the pre-state; rollback failure
safe-closes the overview and emits a high-severity diagnostic rather than
continuing with uncertain compositor state.

## Diagnostics

The read-only topology dispatcher:

```bash
hyprctl dispatch hyprexpo:scrolling_debug smoke-1 active
hyprctl dispatch hyprexpo:scrolling_debug smoke-2 workspace:3
```

emits one `HYPREXPO_SCROLLING_DIAGNOSTIC` JSON record with request/session IDs,
Hyprland and client ABI hashes, direction/offset, columns, targets, sizes,
special-window classification, and before/after equality. It never moves tape,
focus, cursor, or windows and performs no CPU pixel readback or filesystem
output.

Deterministic acceptance input is disabled by default. In a disposable nested
config only, set `scrolling_input_debug = 1`, then use
`hyprexpo:scrolling_input_test`. Its `HYPREXPO_SCROLLING_INPUT` record exposes
per-event state, consumption, pan, selection, drag, reset, and drop effects.
Native releases emit `HYPREXPO_SCROLLING_MUTATION` with request correlation,
outcome, rollback status, violated invariants, and secret-free topology hashes.

## Exact ABI and runtime procedure

Scrolling access uses Hyprland internals and is pinned to Hyprland `0.56.1`,
commit `5c9377c15f85c50648f35ca5a213754f95b93ca0`. Rebuild and revalidate for any
other Hyprland version or ABI hash.

```bash
test "$(pkg-config --modversion hyprland)" = 0.56.1
HYPREXPO_DEV_LAYOUT=scrolling ./scripts/run-nested.sh
./scripts/validate-scrolling-overview.sh --all \
  --evidence .planning/quick/260828-w7w-implement-issue-85-provide-a-niri-like-o/260828-w7w-05-RUNTIME-EVIDENCE.md
```

That default command is reusable after merge and does not assume an issue
branch or remote state. Optional pre-publication fence for issue #85:

```bash
./scripts/validate-scrolling-overview.sh --all \
  --evidence /tmp/issue-85-runtime-evidence.md \
  --issue-85-publication-check
```

The validator keeps images, checksums, client JSON, native topology, input and
mutation records, config/plugin state, process health, and attributable logs in
a per-run `/tmp/hyprexpo-scrolling-acceptance-*` bundle. Physical touch is
reported separately when no bound device exists; deterministic touch cancel
and reacquisition coverage remains mandatory.
