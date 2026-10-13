# Scrolling overview proof for issue #158

These unedited captures come from disposable, ABI-matched Hyprland 0.56.1 and 0.56.2 sessions. The test scene reproduces the report: workspace 2 uses Dwindle, workspace 3 does not exist, and workspaces 4 and 5 use Scrolling with one or two windows. The Lua configuration sets `workspace_method = "first 1"`.

With `overview_mode = auto`, Scrolling shows individual windows in vertical workspace rows. The opening viewport centers the active row; use the wheel, canvas drag, or keyboard navigation to reveal another row. Two wide windows shrink to fit their row, so the adjacent row may initially show less content than a row with one window. `overview_mode = grid` shows workspace thumbnails instead.

## Video

- [0.56.2 before: focus returns to the opening row after a repaint](scrolling-0562-before.mp4)
- [0.56.2 after: wheel, keyboard, selection, mixed row, grid override, fullscreen](scrolling-0562.mp4)
- [0.56.1 compatibility matrix and all four scrolling directions](scrolling-0561.mp4)
- [0.56.1 input, movement, and rollback](scrolling-0561-input.mp4)

## Images

- [0.56.2 before: lower row cannot stay focused](scrolling-0562-before-keyboard.png)
- [0.56.2 after: lower row reached by wheel](scrolling-0562-panned.png)
- [0.56.2 after: keyboard focus reaches the lower row](scrolling-0562-keyboard.png)
- [0.56.2 forced grid](scrolling-0562-grid.png)
- [0.56.2 fullscreen overview](scrolling-0562-fullscreen.png)
- [0.56.1 offscreen/shared columns](scrolling-0561-multirow.png)
- [0.56.1 existing empty destination populated](scrolling-0561-empty-drop.png)
- [0.56.1 new workspace following the Dwindle default](scrolling-0561-terminal-dwindle.png)

The corner-marked windows make missing edges visible. The videos retain their raw transitions; still captures were taken after test clients submitted their final-size buffers.

## Verification

| Hyprland | Exact revision | Local runtime checks | Tested plugin SHA-256 |
| --- | --- | ---: | --- |
| 0.56.1 | `5c9377c15f85c50648f35ca5a213754f95b93ca0` | 27 | `2603b0d655755e95ce78bf3d53533023af6718317b7799b453a4b4d46e867d3e` |
| 0.56.2 | `efb50993780079460b0cbed1363e2166a2de1d9f` | 35 | `4e75fb66cdeec7d0d647dc77bbf39fa4009c8fe1d3b5ff6c1b9afea9f904ecb1` |

The source tested at runtime was PR #159's original head `69cf7e5f8400d302ac6597f2f14c820c41231528` plus the local fix patch, SHA-256 `1c87afb2473afec8b5e9cd6e961adf75fe65d50c4f877ba7a1a8ff75b157fa59`. The code in that patch is committed in this PR; the test binaries still display the original head in their development version string. See [media checksums](sha256.json).

The runtime matrix covers complete previews, one/two windows, Dwindle and Scrolling rows, four directions, scrolling beyond the native viewport, fullscreen, Lua grid override, selection, panning, column and workspace moves, empty destinations, faulted rollback, config reload, and three plugin unload/reload cycles on each release. Four C++ suites, 84 tooling tests, 40 input-oracle cases, sanitizers, static analysis, and documentation build passed locally.

Mouse and keyboard events were delivered through Wayland virtual input. Deterministic touch cancellation was tested. Physical touch/trackpad gestures, DRM scanout/tearing, other GPUs, and development revisions were not tested by this packet.
