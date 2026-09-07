# Workflow Validation: 2026-09-07

This receipt separates release rehearsal, development runtime acceptance, and
CI evidence. It does not announce a new Hyprland release or authorize release
publication. The infrastructure and development candidate are reviewed in
[PR #121](https://github.com/sandwichfarm/hyprexpo/pull/121) and
[PR #122](https://github.com/sandwichfarm/hyprexpo/pull/122).

## Exact Targets and Source

| Track | Hyprland commit | Plugin source checkpoint |
| --- | --- | --- |
| Released v0.56.1 | `5c9377c15f85c50648f35ca5a213754f95b93ca0` | Native rehearsal: `b14e5833b8fe00dbb83c39b10d53ed8eb5278782`. |
| Released v0.56.2 | `efb50993780079460b0cbed1363e2166a2de1d9f` | Same native rehearsal candidate. |
| Development | `34eb03bd8da01024596c367fba66485a8c9b8ca7` | Development runtime source includes the API port and Lua caller fixes through `bb869b1abba13576313936a2ec088580e7c78fb0`. |

The development history retains original contributor commit
`fbdfba47cc7163578271ba7b342fd8674cd2e7af`. Release pins remain unchanged; the two
currently supported releases still select plugin source
`5891014c611e1bd56d0121143f0221d46b5c0967`.

The PR checks identify their own final tested head/merge commits. Each
compatibility artifact contains the plugin commit/tree, upstream commit,
resolved full dependency lock and checksum, Nix derivation and closure, build
logs, generated-header checksums, and shared-object checksum. Runtime evidence
applies to the artifacts below; a later different artifact needs its own
acceptance evidence.

## Release Rehearsal

Both release environments were freshly audited against their exact Git tags:
400 source headers, 11 downloaded package digests, and 99 package members
covering generated headers and compositor tools were checked per target.
Forced plugin builds included 352 Hyprland headers per target with no foreign
Hyprland headers or unresolved runtime libraries.

All three standalone regression suites and all 16 pin ancestry checks passed.
Each disposable release runtime passed 38 checks covering six rectangular grid
shapes, keyboard confirmation/cancellation, exact original-submap restoration,
pointer selection, active-overview unloading, reloading, and final unloading.
Each produced seven screenshots; all owned compositor, parent-compositor, and
fixture processes were stopped afterward.

| Native rehearsal artifact | SHA256 |
| --- | --- |
| v0.56.1 | `d5cc6b1c6c88b2b87333e5268764e155d91b773fefcdc202fa17d20efb422d47` |
| v0.56.2 | `d50d7dd5abf56bf67d3d16efc0b398b524c104df02f45c8a195f08e653b12f9e` |

The candidate's compiled source and Makefile were compared with the selected
release-pin source and found equal. Separately, unmodified tagged hyprpm
processes cloned the public repository without a revision argument, selected
`5891014...`, verified matching headers, and built artifacts that loaded and
unloaded in their respective disposable compositors. Their final privileged
installation step was blocked by the host-isolation sandbox, so this is
source-selection/build/load proof, not a claim of full release installation.
The matching header/global ABI state for that bounded test was prepared from
the audited release prefixes.

## Nix Development Consumer and Runtime

A fresh QEMU/KVM guest booted from a new child disk overlay with the older
stopped disposable VM as its read-only backing. Existing dependency cache
entries were reused. Nix 2.35.2 ran inside Ubuntu 24.04 with sandboxing enabled.
The final graphics path used guest VirtIO/VirGL, a headless Wayfire parent, and
the exact development Hyprland compositor. The live desktop installation was
not used as a test target.

An external consumer flake made the plugin's Hyprland input follow its exact
upstream input and retained its complete lock. Actual builds of both plugin
and compositor resolved the package outputs loaded in the VM; this was not
only flake evaluation. The compositor and plugin agree on the full ABI:

```text
34eb03bd8da01024596c367fba66485a8c9b8ca7_aq_0.15_hu_0.14_hg_0.5_hc_0.1_hlg_0.6
```

The accepted Nix shared object's SHA256 is:

```text
cdbf4077d624746e0db110dce9eb9828bba36cecbe3d9a2f41745b54a083e330
```

Acceptance covered mapped grid and scrolling previews, actual Wayland virtual
pointer and keyboard selection, original-submap restoration, pinned-window
exclusion and preservation, native touch ownership release, a no-op mutation,
a committed native move, and an injected post-add failure that rolled back to
the exact prior state with zero invariant violations. Unload, empty plugin
list, compositor liveness, reload, and empty configuration errors were checked.

![Mapped workspace grid in the disposable Nix compositor](/validation/2026-09-07/nix-grid.png)

![Mapped scrolling overview in the disposable Nix compositor](/validation/2026-09-07/nix-scrolling.png)

The development port also exposes the existing movement and scrolling
inspection/test handlers through Lua. On this upstream commit, legacy
`addDispatcherV2` registration returns failure; the Lua wrappers and updated
input helper preserve access to those existing operations. Regression tests
cover the wrappers and argument validation, and native runtime records exercise
both commit and rollback paths.

## Development hyprpm Consumer

The unmodified manager selected public branch ref
`origin/integration/chase-validation` at
`330e071a19798b6202cdebab76ae5a4f201c8a72`, generated the matching headers,
built and installed the plugin with `failed=false`, and successfully updated
the explicit revision. Its cached artifact was explicitly loaded after removing
a separate auto-loaded test artifact from the private VM configuration. The
loaded development version and process memory mapping identify the actual
hyprpm cache artifact.
Its SHA256 was
`9ba87450b60a7fefb91d9821fadcf918e67641fd1db0501f8caea1f39b9be3dd`.
An additional disable/remove/re-add cycle selected the full immutable commit,
installed with `failed=false`, and passed the same five real-input smoke cases.


Pointer selection, keyboard confirmation, scrolling cancellation, exact
original-submap restoration, and pinned-window preservation passed with that
managed artifact. The [reproduction recipe](./hyprpm-validation-environment.md)
records the Nix-assisted dependency environment and scoped preparation required
by this disposable Ubuntu VM. Upstream manager source, generated version hashes, and ABI checks
were not patched or bypassed.

## Branch Protection Readback

GitHub rulesets [Release compatibility](https://github.com/sandwichfarm/hyprexpo/rules/22443374)
and [Development compatibility](https://github.com/sandwichfarm/hyprexpo/rules/22443409)
are active. Effective-rule API readback confirmed:

| Branch | Required checks | Additional protection |
| --- | --- | --- |
| `master` | `Release gate`, `Site check`, from GitHub Actions app 15368. | Existing default PR, deletion, and non-fast-forward protections retained unchanged. |
| `hyprland-git` | `Development gate`, `Site check`, from GitHub Actions app 15368. | PRs required; deletion and non-fast-forward updates blocked; merge commits preserve contributor ancestry. |

Both require checks against the latest base. The existing repository-admin
bypass remains available; no claim is made that administrators cannot bypass
the rules. The prior default ruleset was compared before and after and was
unchanged. The new required checks already exist on the implementation PRs;
future PRs need the workflows present in their evaluated trees.

## CI and Promotion Boundaries

The reproducible Nix helper was executed for both releases and the development
target, including an initial build and a forced deterministic rebuild. Negative
checks rejected moving refs, empty/duplicate target matrices, mismatched
upstream revisions, missing dependency hashes, and a release lock presented as
a development lock. The aggregate gate was checked against all 64 combinations
of success, failure, cancellation, and skipped dependency results; only complete
success passes. Only the appropriate track's gate is emitted.

Generated-header provenance includes the actual scanner-produced protocol
headers as well as the version header. All-PR regression and site checks have
no path filters, so documentation-only PRs can satisfy their required checks.
The upstream-tip probe is separate and never changes the development pin;
its scheduled activation requires the workflow to be merged onto the default
branch and its helper to be available on the development branch.

The latest upstream release and newest stable semver tag checked during the
rehearsal were both `v0.56.2`. There was no newer eligible release to promote.
The [promotion checklist](../guides/release-promotion.md) is ready for the next
release; no duplicate tag or release was published during this rehearsal.

Physical touchpad feel, every GPU combination, physical multi-monitor hotplug,
and the full compositor interaction matrix remain outside this bounded proof.
A future release still needs its own candidate, integrated-tree validation,
matching runtime acceptance, and explicit maintainer approval.
