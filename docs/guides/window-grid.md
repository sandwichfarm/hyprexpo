# Window Grid

The window grid is an overview of one scrolling workspace's windows. Where the
[scrolling overview](./scrolling-overview) shows every workspace on the monitor
as a tape row, the window grid shows only the windows of the workspace the
overview opened on, laid out in a fixed grid that fits the screen. It exists for
`general.layout = scrolling` users whose tape holds more columns than fit on
screen and who want to jump to one window without panning.

It opens only on a workspace whose resolved layout is `scrolling`. Any other
workspace opens the classic workspace grid, whatever `overview_mode` says.

## Configuration

```lua
hl.config({
    plugin = {
        hyprexpo = {
            overview_mode = "windows",
            windows_columns = 0, -- 0: ceil(sqrt(window count)); 1..16: fixed column count
        },
    },
})
```

```ini
plugin {
    hyprexpo {
        overview_mode = windows
        windows_columns = 0
    }
}
```

| key | effect |
| --- | --- |
| `overview_mode = windows` | scrolling workspaces open the window grid; `auto` opens the tape; `grid` opens the workspace grid |
| `windows_columns` | columns per row; `0` picks `ceil(sqrt(n))` for `n` windows, and a requested value is clamped to `n` |
| `tile_rounding`, `tile_rounding_focus`, `tile_rounding_hover`, `tile_rounding_power` | preview corner radii, shared with the workspace grid and the tape |
| `border_width`, `border_color_focus` | focus frame around the keyboard-focused preview; `border_width = 0` disables it |
| `show_pinned_windows` | include pinned windows in the grid |

## What the grid shows

- Tiled windows in native tape order (left to right, then column rows), then
  floating windows. Each preview keeps the window's aspect ratio inside its cell
  and a partial last row is centered.
- A `1`–`9` badge on the first nine previews and `0` on the tenth. Windows past
  the tenth have no badge; reach them with the arrow keys, the pointer, or
  `hyprexpo:kb_selecti N` with their position `N` in the same order.
- The preview of the window that already had focus is focused when the grid
  opens, framed with the focus border.
- An empty scrolling workspace opens an empty grid: no previews, nothing to
  select, and `expo next` still escalates it.

## Selecting a window

| input | result |
| --- | --- |
| pointer hover | focuses the preview under the pointer; a stationary pointer never overrides keyboard focus |
| left click | focuses and selects the clicked preview |
| `hyprexpo:kb_focus left/right/up/down` | spatial focus between previews |
| `hyprexpo:kb_confirm` | selects the focused preview |
| raw digit keys `1`–`9`, `0` | select the badged preview, unless `number_key_mode = passthrough` |
| `hyprexpo:kb_selecti N` | selects the `N`-th preview in badge order (`10` is the `0` badge; larger `N` reaches unbadged windows) |
| `cancel_key`, `hyprexpo:expo cancel` | close without selecting |

Selecting a window keeps its workspace active and gives that window keyboard
focus. The grid is select-only: pressing and moving on a preview never starts a
window drag, and the mouse wheel does nothing because the grid never pans.

## Two-step key: windows first, then workspaces

`hyprexpo:expo next` opens the overview when none is open, replaces an open
window grid with the workspace grid on the same monitor, and does nothing once
the workspace grid is showing. Bind it twice: once normally and once inside the
`hyprexpo` submap, because the overview activates that submap while open and
your normal binds stop matching.

```lua
hl.bind("SUPER + Tab", function() hl.plugin.hyprexpo.expo("next") end)
hl.define_submap("hyprexpo", function()
    hl.bind("SUPER + Tab", function() hl.plugin.hyprexpo.expo("next") end)
    hl.bind("escape", function() hl.plugin.hyprexpo.expo("cancel") end)
    hl.bind("return", function() hl.plugin.hyprexpo.kb_confirm() end)
    hl.bind("left",   function() hl.plugin.hyprexpo.kb_focus("left") end)
    hl.bind("right",  function() hl.plugin.hyprexpo.kb_focus("right") end)
    hl.bind("up",     function() hl.plugin.hyprexpo.kb_focus("up") end)
    hl.bind("down",   function() hl.plugin.hyprexpo.kb_focus("down") end)
end)
```

```ini
bind = SUPER, Tab, hyprexpo:expo, next
submap = hyprexpo
    bind = SUPER, Tab, hyprexpo:expo, next
    bind = , escape, hyprexpo:expo, cancel
    bind = , return, hyprexpo:kb_confirm
    bind = , left,   hyprexpo:kb_focus, left
    bind = , right,  hyprexpo:kb_focus, right
    bind = , up,     hyprexpo:kb_focus, up
    bind = , down,   hyprexpo:kb_focus, down
submap = reset
```

With overviews open on several monitors, a second `expo next` (with or without
`all`) escalates every open window grid at once; monitors already showing a
workspace grid or the scrolling tape stay as they are. Keyboard ownership stays
on the monitor that had it. If the workspace grid cannot be created on an
escalating monitor, every overview closes rather than leaving that monitor
without one.

`hyprexpo:expo toggle` still opens whichever overview `overview_mode` selects
and closes any open overview, so a plain toggle bind keeps working alongside
`next`.

## Limits

- No positional drag and no "next empty workspace" row; use the tape
  (`overview_mode = auto`) or the workspace grid for moving windows.
- No sibling workspaces: the grid never shows other workspaces on the monitor.
- Window previews are captured at native size and fitted into their cells, so
  very large windows on a very large monitor can exceed the
  `scrolling_thumbnail_budget` and render as placeholders.

## How it is wired

The window grid is not a third overview implementation. The session factory
resolves one session kind from the configured `overview_mode`, the detected
workspace layout and an explicit grid override; dispatchers act on that kind.
Inside the scrolling session the grid is a *presentation* whose traits (which
rows exist, whether presses can drag, whether previews are numbered, what
digits select, whether floating windows are selectable targets) are declared in
one table. See [Session kinds](./scrolling-overview#session-kinds) for how a
future layout or presentation plugs into the same seams.
