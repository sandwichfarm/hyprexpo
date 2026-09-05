# Feature interoperability candidate

This branch combines the open features for testing before a proposed merge to
master. Its base is the release-compatible recovery at `eecace3`.

## Merge checkpoints

| Order | Feature PR | Scope | Combined validation |
| --- | --- | --- | --- |
| 1 | #104 | Monitor-local centered workspace bounds | Merged at `c20e08e` |
| 2 | #115 | Capped grids preserve anchors and reject padding selection | `89b2285`: tests, sanitizers, both release builds, 54 two-output selections and padding/empty-workspace input checks pass |
| 3 | #105 | Pinned windows do not interrupt overview close | Pending |
| 4 | #114 | All-monitor overview, input ownership, drag/drop and recapture | Pending |
| 5 | #107 | Native scrolling overview and mixed-layout sessions | Pending |

From the second merge onward, each combined tree must pass regression tests,
separate builds for Hyprland v0.56.1 and v0.56.2, and the applicable disposable
runtime checks before the next feature is merged. The final checks cover the
features together, especially workspace selection, exact submap restoration,
multi-monitor ownership, preview recapture, pinned windows and scrolling input.

## Testing this branch

Use a plugin compiled from the recorded integration commit against the exact
Hyprland revision and dependency ABI of the disposable compositor. The release
pins in `hyprpm.toml` can select older source; an enabled HyprPM entry is not
evidence that the combined artifact is loaded. Confirm the plugin version and
configuration errors in the target compositor.

This integration PR remains separate from master until combined testing is
reviewed. Compile checks alone do not establish runtime interoperability.
