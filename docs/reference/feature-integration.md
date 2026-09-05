# Feature interoperability validation

PR #116 combined the feature branches for testing on top of the release-compatible
recovery at `eecace3`. The verified integration was squash-merged to master at
`fe502d0`. The checkpoints below retain the original integration commit identities.

## Merge checkpoints

| Order | Feature PR | Scope | Combined validation |
| --- | --- | --- | --- |
| 1 | #104 | Monitor-local centered workspace bounds | Merged at `c20e08e` |
| 2 | #115 | Capped grids preserve anchors and reject padding selection | `89b2285`: tests, sanitizers, both release builds, 54 two-output selections and padding/empty-workspace input checks pass |
| 3 | #105 | Pinned windows do not interrupt overview close | `361abbd`: tests, sanitizers, both release builds, repeated range/input checks and pinned-window selection follow-through pass |
| 4 | #114 | All-monitor overview, input ownership, drag/drop and recapture | `5eae39b` + `b042565`: tests/sanitizers and both builds pass; 108 selections, cross-monitor drag, surviving-output teardown and exact submap restoration pass |
| 5 | #107 | Native scrolling overview and mixed-layout sessions | `d41f75c` through `c905dcf`: tests/sanitizers, both release builds, CMake/Meson suites, 39 input-oracle cases and native acceptance on both releases pass; mixed-session regressions recorded below |

From the second merge onward, each combined tree must pass regression tests,
separate builds for Hyprland v0.56.1 and v0.56.2, and the applicable disposable
runtime checks before the next feature is merged. The final checks cover the
features together, especially workspace selection, exact submap restoration,
multi-monitor ownership, preview recapture, pinned windows and scrolling input.

## Building the combined features

Use master or the release tag for the landed features. The v0.56.1 and v0.56.2
HyprPM pins select combined source on master at
`7c5e2ac2524ffa75d077b5785b760ec81edb0cc6`; older release pins are unchanged.
Compile against the exact Hyprland revision and dependency ABI of the disposable
compositor. An enabled HyprPM entry is not evidence that the combined artifact
is loaded: confirm the plugin version and configuration errors in the target
compositor. Do not overwrite an artifact while it is loaded.

The ordinary source gates are:

```sh
make -B test
make check-pins REF=HEAD
bash scripts/inject-scrolling-input.sh --source-contract
```

Use `make -B all` with separate output paths and exact include paths for each
release. A previously built binary is not proof for a different header set.

The integration PR was merged after combined testing and inline review.
Compile checks alone do not establish runtime interoperability.

The fourth checkpoint found that a disconnected output could leave its
overview registered. The integration branch unregisters it on monitor removal;
the focused runtime regression now passes.

## Combined behavior and repairs

Grid and native scrolling sessions share the per-monitor registry, generation
checks, keyboard traversal and exact entry-submap restoration. Selecting an
already-active workspace on another output now transfers monitor focus too.
Relative grid selectors use their own monitor context; an empty tile cannot
select a foreign-owned empty workspace or create its workspace on another output.

Testing with repainting clients exposed a native input race: scene refresh
cleared ownership between press and release. Content refresh now waits for the
interaction to finish, and released selection is retained until deferred close.
Native drops revalidate ordered column/target identities and geometry before
mutation. Config reload, workspace moves and teardown still invalidate input;
workspace-move invalidation is conservative across native sessions.

The final pointer regression holds ownership through 350 ms of 50 ms client
repaints, commits a real two-column-to-one-column drop while retaining both
targets, then changes topology with `expel` during another held drop. The stale
release preserves the changed geometry and emits no additional mutation.
Grid-to-scrolling, scrolling-to-grid and scrolling-to-scrolling cross-output
drops all release ownership without moving windows or closing peer sessions.

## Integration checkpoint evidence

The following separate binaries were built from `c905dcf` and loaded into
matching disposable compositors. Both passed native topology/direction checks,
deterministic mouse/touch cancellation and reacquisition, same/new/existing
column and cross/mixed/terminal workspace mutations, rollback, real Wayland
keyboard/pointer input, reload/close/unload and process-health checks.

| Hyprland | Source hash | Plugin SHA-256 |
| --- | --- | --- |
| v0.56.1 | `5c9377c15f85c50648f35ca5a213754f95b93ca0` | `aef9db328fe6ea3e67d9a3ad8653ac81df9f14049dc5e6e48be504066f1167b1` |
| v0.56.2 | `efb50993780079460b0cbed1363e2166a2de1d9f` | `54f07e26a758e2b25c576de11ebca9f079b632d0937e4ecd6f9a44c7b6276841` |

Pinned-preview opt-in/off pixel assertions and selection through a hidden
pinned overlay pass on both releases. CMake and Meson each pass all three suites.
Local logs, scripts, binaries, captures and checksums are retained under
`/var/tmp/hyprexpo-integration.oc6IUs`, with the final source checkpoint in
`merge-5-input-safe-c905dcfb0e1050c19021f2cf4b8f2d71d04763ce` and native runs in
`native/v0.56.{1,2}-input-safe*`.

## v0.56.2 release revalidation

After the squash merge, master commit
`7c5e2ac2524ffa75d077b5785b760ec81edb0cc6` was rebuilt separately for both supported
Hyprland releases and passed native acceptance in both matching disposable
compositors. Its runtime code is identical to the reviewed integration head
`86c6783`; only `VERSION` changed to `v0.56.2`.

| Hyprland | Pinned-source build SHA-256 |
| --- | --- |
| v0.56.1 | `e86ca41c0d3982cfb8768a6a3fe632c9cd09f1176a3faa44d336865b38bc96c1` |
| v0.56.2 | `5044f240905fa47c7f905ea21d3fdd80a95390554ff0201a1581b64c043616b3` |

These pre-tag pin builds report `v0.56.2-dev+7c5e2ac`. The release workflow builds
the tagged tree afresh and must report clean `v0.56.2`; its downloadable artifact
and provenance are separate from the above binaries. Fresh source tests and
ASan/UBSan suites passed. Logs/builds are under
`/var/tmp/hyprexpo-release-0562.6RsFp3`; native acceptance evidence is under
`/var/tmp/hyprexpo-integration.oc6IUs/native/v0.56.{1,2}-release-0562-pin`.

## Test boundaries

- Grid-to-grid cross-output dragging is supported. Cross-output drops involving
  native scrolling cancel safely; native scrolling mutations remain within the
  initiating monitor's workspace rows. This integration does not introduce a
  cross-output native-column transfer mechanism.
- Deterministic touch events and Wayland virtual input do not replace physical
  touchpad/touchscreen testing. Physical devices and Nix builds remain untested.
- PR #117 is a separate development-Hyprland compatibility draft superseding
  reverted #109, not part of this tagged-release feature set. It remains separate
  to preserve the #113 recovery and explicit release reference.
