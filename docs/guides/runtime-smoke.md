# Runtime Smoke Checklist

Run these checks in a nested Hyprland session or another disposable compositor session before publishing a release.

`scripts/run-nested.sh` launches a disposable nested session with a fresh user-owned build under `${XDG_CACHE_HOME:-$HOME/.cache}/hyprexpo`.

`scripts/dev-watch.sh` rebuilds and relaunches that session on source changes.

For the issue #85 fixture, run:

```bash
HYPREXPO_DEV_LAYOUT=scrolling ./scripts/run-nested.sh
```

This explicit mode creates right/left/down/up native scrolling workspaces, a
mixed-layout row, a three-column workspace with a multi-target column and an
offscreen target, plus floating, grouped, fullscreen, and pinned cases. The
default without `HYPREXPO_DEV_LAYOUT` remains the unchanged grid fixture.

The complete exact-ABI automated gate is:

```bash
./scripts/validate-scrolling-overview.sh --all \
  --evidence .planning/quick/260828-w7w-implement-issue-85-provide-a-niri-like-o/260828-w7w-05-RUNTIME-EVIDENCE.md
```

This command is reusable after merge. Before publishing issue #85 specifically,
append `--issue-85-publication-check` to also require the approved feature
branch, base, linear Lore history, and absent remote branch/PR.

Nested test binds:

- `F10` for overview
- `SUPER+Return` for a terminal
- `SUPER+1..9` for workspaces
- `SUPER+SHIFT+1..9` to move a window
- `SUPER+Q` to close a window
- `SUPER+SHIFT+Q` to exit

## Checklist

1. Load the locally built plugin and confirm no API/hash mismatch is reported.
2. Toggle overview on and off with `hyprexpo:expo, toggle`.
3. Cancel overview with the configured `cancel_key` and confirm the workspace does not change.
4. Move keyboard focus with `hyprexpo:kb_focus`, then select with `hyprexpo:kb_confirm`.
5. Select by pointer or touch near the outside edges of the monitor.
6. Confirm current, hover, focus, label, and border styling render as configured.
7. Open two or more windows on one workspace, open overview, press on a visible window preview, drag across the move threshold, and confirm the under-pointer drag proxy appears.
8. Drag that window preview over another valid workspace tile and confirm a positional landing proxy appears inside the hovered target tile.
9. Move the pointer within the target tile and between target tiles; confirm the landing proxy tracks the pointer position, preserves the original grab offset, and disappears when hovering the source or an invalid target.
10. Release on a target workspace and confirm the window still moves through the existing safe workspace move behavior. This release is not yet a positional layout insertion.
11. In Lua config, register distinct `expo` and `cancel` gestures (for example, four-finger up and four-finger down). With the overview closed, begin the cancel gesture and confirm it remains closed.
12. In grid mode, complete the expo gesture and confirm its existing open/select behavior is unchanged. In scrolling mode, confirm both expo-close and cancel swipes keep the opening workspace, then verify pointer selection and keyboard confirmation commit the chosen target separately.
13. From the workspace where the overview opened, hover a different tile and make a partial cancel swipe. Confirm the animation returns to the same still-open overview and the origin workspace has not changed.
14. Repeat with a completed cancel swipe. Confirm the overview closes onto the origin workspace rather than the hovered tile.
15. Open from the origin workspace, hover another tile, begin an expo close, and release below the completion threshold so the overview restores. Without changing the hovered tile, complete cancel and confirm its animation retargets the origin and closes there. Repeat open/completed-cancel once more to catch stale swipe state, then inspect the nested compositor log for crashes, assertions, API/hash mismatches, and stale-callback errors.
16. Open or create a Picture-in-Picture-like pinned window. In the nested session, one practical path is to focus a test window and run `hyprctl dispatch pin` from a terminal.
17. With the default `plugin:hyprexpo:show_pinned_windows = 0`, open overview and confirm the pinned window is not rendered into every workspace preview tile.
18. Set `plugin:hyprexpo:show_pinned_windows = 1`, reload or apply the keyword, reopen overview, and confirm pinned windows render in previews again for users who opt in.
19. Set `plugin:hyprexpo:show_pinned_windows = 0` again, close overview, and confirm the pinned window is still visible and pinned in the normal Hyprland session.
20. Unload or reload the plugin after closing overview and confirm no stale render pass callback crashes the session.

21. Enable `dynamic_grid = 1`, then test with 1, 2, 3, and 5 non-empty, non-special workspaces. Confirm the overview contains exactly those workspaces without padded empty tiles.
22. Confirm every preview preserves the monitor aspect ratio. For 3 and 5 workspaces, confirm the partial final row is independently centered.
23. Set `label_pos = top_right`, choose a visible `label_size`, and toggle `show_workspace_names`. Confirm every label remains inside its corresponding tile.
24. Toggle `wallpaper_bg` and reopen the overview. Confirm the wallpaper changes only the background and does not move or resize tiles.
25. Toggle `mru_sort` and confirm the current workspace moves to the first tile only when enabled. Toggle `fill_gaps` with non-contiguous workspace IDs and confirm missing IDs are added only when enabled.
26. With only one active workspace, complete and cancel a swipe. Confirm the overview exits cleanly without a crash or invalid animation state.
27. With `dynamic_grid = 1`, leave the current workspace empty and open a window on a neighboring workspace. Open and close with `hyprexpo:expo, toggle`; confirm the current workspace does not change.
28. With `dynamic_grid = 1` and `fill_gaps = 1`, create distant workspace IDs (for example 1 and 5000). Open the overview; confirm it rejects gap expansion, remains responsive, and shows only the sparse workspaces.
29. With `dynamic_grid = 1`, set `label_enable = 0`, then `label_show = never`, and set modern `border_color_current` / `border_color_hover` values. Confirm labels stay hidden and the modern border colors win over legacy highlight values.

30. In scrolling mode, prove hover enter/change/clear, click, threshold drag,
    real wheel pan, every positional drop zone, cross-workspace and mixed
    fallback moves, terminal next-empty creation, outside/no-op release, and
    rollback diagnostics.
31. Cancel touch pending, pan, and drag states. Confirm no selection/drop, no
    stuck consumption or proxy, and immediate mouse/touch reacquisition.
32. Capture topology before and after map/unmap/move/reload/close/unload cycles;
    confirm offset/order/width/size equality where no mutation was requested.
33. Repeat on rotated/fractional output settings, then launch the default grid
    fixture and confirm its appearance, selection, drag, cancel, and close paths.

::: warning
The public site and docs should not claim full release readiness until this runtime smoke gate has been completed for the intended release artifact.
:::

## Cross-monitor fixed-grid acceptance

Launch an isolated two-output session with
`HYPREXPO_DEV_OUTPUTS=2 ./scripts/run-nested.sh`. Address its instance explicitly
with `hyprctl -i <nested-instance>` for every command below. Never load a test
artifact in the desktop session.

1. Set `cross_monitor = 1`, `workspace_method = first 1`, `columns = 3`,
   `rows = 3`, `max_workspace = 9`, `skip_empty = 0`, `dynamic_grid = 0`, and
   `overview_mode = grid`. Bind workspaces 1–5 to WAYLAND-1 and 6–9 to WAYLAND-2.
   Put visibly different windows on workspace 1 and workspace 6.
2. Open `hyprexpo:expo on` on WAYLAND-1. Confirm tile 1 shows the local window
   and tile 6 shows the foreign window, with neither workspace moving outputs.
3. Set `animation = windowsMove,1,20,default`, `cursor:no_warps = 1`, and
   `input:follow_mouse = 1`. Select tile 6 with `hyprexpo:kb_selectn 6`.
   WAYLAND-1 must return directly to its local workspace without a foreign-tile
   zoom; focus and cursor must move to WAYLAND-2. Check `monitors -j`,
   `workspaces -j`, and `cursorpos -j` immediately and after pointer movement.
4. Repeat using `hyprexpo:expo on all`. Captures must still show windows rather
   than nested overviews, and foreign selection must remove all sessions.
5. Check local selection and cancel. Disable `cross_monitor`, reopen, and
   confirm the existing per-monitor enumeration is unchanged. Re-enable it and
   select an uncreated workspace bound to WAYLAND-2; it must be created there.
6. Confirm window dragging and `hyprexpo:move_window` are refused in the opt-in
   grid; close/reopen/reload/unload must leave the compositor responsive and
   `configerrors` empty. Test other monitor scales/transforms separately.
7. Keep the overview open on WAYLAND-1 while changing the visible content of a
   window on WAYLAND-2, for example play a video with mpv. Tile 6 must change
   without moving the cursor, switching workspaces, or reopening the overview.
   Repeat with `on all`, then with a different refresh rate on WAYLAND-2.
   Stop the content changes and check that the overview does not keep polling.
   Confirm normal local previews, selection, cancel and plugin unload still work.

### Deferred cross-monitor selection regressions

In the disposable two-output grid fixture above, set
`binds:hide_special_on_workspace_change = false`, leave a populated special
workspace open on WAYLAND-2, and select workspace 6 from WAYLAND-1. Check that
WAYLAND-2 has `specialWorkspace.id = 0`, workspace 6 is active, and focus/cursor
land there. Cancel and local selection must leave the foreign special open.

Exercise a workspace ownership change before the idle callback with one IPC
batch against the nested instance:

```sh
hyprctl -i <nested-instance> --batch 'dispatch hyprexpo:kb_selectn 6 ; dispatch moveworkspacetomonitor 6 WAYLAND-1'
```

The overview must already be open on WAYLAND-1 with workspace 6 on WAYLAND-2.
After the batch, workspace 6, focus and cursor must all be on WAYLAND-1; the
callback must not clear a special workspace or warp to the previous owner.
