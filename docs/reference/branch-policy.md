# Branch and Release Policy

This policy helps maintainers and contributors choose a target branch, track
Hyprland development, and promote support for a new Hyprland release.

## Status and Branch Contracts

`master` remains the default branch for released Hyprland versions. The
`hyprland-git` branch and any older maintenance branches described here are
rollout targets; publishing this document does not create them or establish
compatibility with development Hyprland. Check the
[compatibility reference](./compatibility.md) for the supported release matrix
and recorded verification boundaries.

| Branch | Contract | Typical changes |
| --- | --- | --- |
| `master` (default) | Supports the latest adopted, validated Hyprland release and any additional releases in the support matrix. | Bug fixes, compatible features, and validated release promotions. |
| `hyprland-git` | Tracks upstream development through explicitly recorded Hyprland commits. Its tip is experimental. | Adaptations to unreleased APIs and integration of fixes from `master`. |
| `release/0.56` (example, optional) | Maintains a declared older Hyprland release line after `master` advances. | Selected compatible fixes and pin updates. |

Here, stable means a declared release compatibility target with recorded
validation. It does not promise a frozen feature set, support for every older
release, or interchangeable plugin binaries. Keep the existing `master` name;
renaming the default branch to `stable` is unnecessary for this separation.

Create an older maintenance branch only when someone intends to maintain it.
Otherwise, retain its existing tags and compatibility pins and explicitly mark
the release line as receiving no further fixes.

## How This Fits the Ecosystem

Arch's rolling distribution still packages versioned Hyprland releases. As of
2026-09-07, its Extra package is `0.56.2-2`; following development Git is a
separate compatibility choice. [Arch package](https://archlinux.org/packages/extra/x86_64/hyprland/)

Hyprland's official plugins serve released Hyprland versions through hyprpm
commit pins, while their latest plugin commits require `hyprland-git`.
Hyprpm maps a Hyprland commit to a plugin commit, and falls back to latest Git
when no pin matches. The upstream guidelines recommend a pin for each Hyprland
release. [Official plugins](https://github.com/hyprwm/hyprland-plugins),
[pin guidelines](https://wiki.hypr.land/hyprland-plugins/development/plugin-guidelines/)

HyprExpo keeps a release-compatible default as well as pins so that direct
source and Nix consumers have an explicit release target. The separate chase
branch provides a place to adapt to upstream development without advancing the
default branch's compatibility requirements prematurely.

## Contribution Flow

1. Target `master` for normal fixes and features that preserve its declared
   release matrix. Validate against every release that the change affects.
2. Merge those changes forward into `hyprland-git`. Resolve conflicts by
   preserving behavior while adapting the development API boundary.
3. Target `hyprland-git` for changes requiring unreleased Hyprland APIs. Record
   the exact upstream commit, dependency provenance, and checks performed in
   the PR. A branch named after upstream Git is not proof that today's upstream
   tip builds or runs.
4. If a fix originates on the chase branch and also applies to released
   Hyprland, backport it in a focused PR to `master`. Exclude unrelated API
   changes and repeat release validation.

Before a chase branch exists, keep development adaptations in topic branches
and identify them as awaiting that target. Their presence does not make them
eligible for a merge into the release-compatible default.

## Promoting a Hyprland Release

Promotion follows a specific upstream release and tested plugin commit. It is
not a periodic merge of the entire moving chase branch.

1. Choose the upstream release tag and resolve its exact commit. Identify the
   plugin commit that supports it. Record the proposed support matrix, including
   any older versions that will leave default-branch support.
2. If the chase tip already requires changes made after that release, select an
   earlier compatible commit or prepare a candidate branch from it. Bring over
   any missing default-branch fixes without importing later API requirements.
3. Prepare a promotion PR against `master`. A merge of the selected history is
   appropriate when all included changes belong in the new release target.
   Review the resulting tree, dependency references, and behavior preservation.
4. Run tests and forced builds against each release in the proposed matrix,
   with isolated headers and output paths. Record plugin/upstream commits,
   dependency and generated-header provenance, and artifact checksums. Use the
   [release verification procedure](./compatibility.md#release-verification).
5. Load each candidate artifact in a disposable matching compositor and run
   the [runtime smoke checklist](../guides/runtime-smoke.md). Exercise affected
   input, rendering, and teardown paths. Record untested hardware or package
   paths explicitly; a compile or load check does not prove those behaviors.
6. When retiring an older target from `master`, create a maintenance branch
   from its last validated source only if continued fixes are planned. State
   the support decision in the promotion PR and compatibility reference.
7. After review and landing, verify the final published source. Repeat affected
   checks if integration changed the tested tree. Point hyprpm pins at the
   validated, reachable source commit; run `make check-pins REF=HEAD` on the
   publication branch. Update release references and compatibility docs, then
   use the existing version/tag/publication workflow.
8. Merge the resulting default-branch history forward into `hyprland-git` and
   resume chasing the next upstream development target.

For example, if `master` supports Hyprland 0.56 and the chase branch has already
adopted post-0.57 changes, a 0.57 promotion must use the candidate that actually
supports 0.57. Merging the chase tip would advance the required API too far.
These version numbers illustrate the process, not additional support claims.

## Tags, Pins, and Packages

- **Branches organize development; tags and pins identify exact source.** Keep
  the project's Hyprland-aligned tag scheme: `v<hyprland-version>` for the first
  plugin release targeting that version, then `v<hyprland-version>+N` for plugin
  updates on the same target.
- **Preserve existing release pins on the chase branch.** A fix on a branch
  does not reach pinned users until the corresponding pin is deliberately
  updated. Validate the selected source against each mapped Hyprland release.
- **Keep maintained backports reachable.** Publish updated pin metadata on the
  branches users consume. For a new maintenance commit selected by a pin,
  integrate its history into `master` through a reviewed merge that preserves
  the default branch's current API and behavior, then merge forward into the
  chase branch. Cherry-picking alone does not make the original pinned commit
  an ancestor. Validate the maintenance source on its older target and the
  merged tree on the default's targets before updating pins. Never bypass the
  ancestry check or restore incompatible old API code to satisfy it.
- **Make chase consumption explicit.** A missing hyprpm pin on the default
  branch does not automatically select `hyprland-git`. Document and validate
  the selected installation path when rolling out the chase branch.
- **Align Nix source compatibility and the build input.** Keep the default
  flake on an adopted release and record an exact development commit on the
  chase branch. Following the system's Hyprland input aligns the build target;
  it cannot make incompatible plugin source compile.
- **Label development artifacts separately.** Record the plugin commit,
  Hyprland commit, and dependency provenance. Do not publish a chase artifact
  as a stable release asset. Build for the compositor that will load it.

## Rollout Checklist

These are follow-up actions after adopting this document, not completed setup:

1. Keep `master` as the default. Confirm its existing supported release matrix
   and published pins; branch-policy adoption alone does not change either.
2. Create `hyprland-git` from current, validated `master` in an isolated
   checkout. Select and record one upstream development commit and its
   dependency environment before applying compatibility changes.
3. Retarget or reconstruct development-only PRs onto that branch. Preserve
   contributor history where possible, review conflicts, and validate the
   combined result against the selected upstream commit.
4. Add branch-specific CI: declared release builds for `master`, and a
   reproducible pinned development build for `hyprland-git`. A separate probe
   of upstream tip can report drift without silently moving the tested target.
   Keep disposable runtime acceptance as a release gate.
5. Configure PR protections around those checks and publish tested installation
   instructions for the chase branch. Verify hyprpm and Nix consumption
   separately before claiming either path is supported.
6. At the next adopted upstream release, use the promotion procedure above.
   Create older maintenance branches only with an explicit support commitment.

After the chase branch and its consumer paths are validated, update this
document's status to describe the available branch and link to its evidence.
