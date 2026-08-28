# Runtime Smoke Checklist

Run these checks in a nested Hyprland session or another disposable compositor session before publishing a release.

`scripts/run-nested.sh` launches a disposable nested session with a fresh user-owned build under `${XDG_CACHE_HOME:-$HOME/.cache}/hyprexpo`.

`scripts/dev-watch.sh` rebuilds and relaunches that session on source changes.

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
12. Complete the expo gesture and confirm its existing open/select behavior is unchanged.
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

::: warning
The public site and docs should not claim full release readiness until this runtime smoke gate has been completed for the intended release artifact.
:::
