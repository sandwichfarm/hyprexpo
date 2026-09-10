# Promoting a Hyprland Release

Use this checklist to prepare a release-compatible candidate from the
Hyprland development track. It stops at a reviewable, validated promotion PR;
merging, tagging, and publishing require the maintainer's release decision.
See the [branch policy](../reference/branch-policy.md) for contribution flow and
[compatibility reference](../reference/compatibility.md) for current targets.
Use [chasing Hyprland](./chasing-hyprland.md) to discover and prepare a target;
this guide remains the promotion and publication gate.

## Select Immutable Inputs

Record these values in the promotion PR before starting builds:

| Input | Required evidence |
| --- | --- |
| Upstream release | Release tag and the exact commit it resolves to in hyprwm/Hyprland. |
| Plugin candidate | Full commit hash and its relationship to both published branches. |
| Integration base | Current published `master` commit. |
| Proposed support | Every retained or added upstream release; each retired target and its maintenance decision. |
| Dependencies | Locked Nix inputs or an equivalently recorded exact build environment, including generated headers. |

Fetch the plugin branches and upstream tags in isolated checkouts. Resolve tags
with `git rev-parse '<tag>^{commit}'`; record full hashes rather than a moving
branch name. Compare the latest upstream release with the supported matrix
before creating a new promotion: an already supported tag is a rehearsal
input, not a reason to publish a duplicate release.

Choose the plugin commit that supports the selected upstream release. If
`hyprland-git` already requires later APIs, branch the candidate from its
compatible earlier commit. Bring in missing fixes from current `master`
without importing those later API requirements. Do not merge the moving chase
tip merely because its own CI is green.

## Validate the Candidate and Integrated Tree

- [ ] Review all changes entering `master`, including conflict resolutions,
  default settings, compatibility declarations, Nix references, and user-facing
  behavior. Preserve contributor and maintenance history where the integration
  strategy requires it.
- [ ] Run `make test` and the applicable CI helper tests and documentation build.
- [ ] Force a plugin build for every release in the proposed support matrix.
  Use isolated source/build directories and matching generated headers; never
  reuse a binary just because its output path already exists.
- [ ] Capture the plugin/upstream hashes, lock-file hash, compiler, dependency
  closure or package versions, actual build command, and artifact checksum for
  each matrix entry.
- [ ] Load each artifact in a disposable compositor built for the same upstream
  commit and dependency ABI. Follow the
  [runtime smoke checklist](./runtime-smoke.md), exercising affected grid and
  scrolling overviews, input/selection, pinned windows, and teardown.
- [ ] Record screenshots and interaction results where rendering changed.
  Declare hardware-specific paths that were not exercised. Build success and
  successful plugin loading do not establish all interaction behavior.
- [ ] Validate the final tree produced by integrating with the recorded
  `master` base. If the base or candidate changes, reassess and repeat affected
  checks. Evidence for an earlier tree does not approve a conflict resolution.
- [ ] Run `make check-pins REF=HEAD` on the publication candidate. Compare its
  complete pin table with the previous published table; explain each deliberate
  change. Do not silently remove older mappings.

CI establishes reproducible build and regression evidence. Disposable runtime
acceptance remains an explicit release gate, with its evidence linked in the PR.

## Prepare the Promotion PR

Use this receipt in the PR description, replacing every entry with a result or
an explicitly pending gate:

```text
Upstream release tag / commit:
Plugin candidate commit:
Master base commit:
Integrated tree tested:
Retained / added / retired support targets:
Maintenance decision for retired targets:
CI runs and immutable dependency inputs:
Per-target artifact hashes and provenance:
Disposable load / interaction / teardown evidence:
Hyprpm and Nix consumer evidence:
Pin changes and ancestry verification:
Known verification gaps:
Release approval: pending
```

Existing release pins remain unchanged until the replacement source is both
validated and reachable from the publication branch. A squash merge changes the
landed commit hash: update the pin to the verified landed source, not an obsolete
PR-branch hash. For an older maintenance commit, follow the ancestry procedure
in the [branch policy](../reference/branch-policy.md#tags-pins-and-packages).

After explicit approval and landing, verify the published source identity and
repeat checks affected by any integration changes. Update compatibility docs,
release references, and validated pins before following the
[release ceremony](https://github.com/sandwichfarm/hyprexpo/blob/master/RELEASING.md). Verify the resulting tag, uploaded
artifact hashes, and provenance separately from workflow exit status. Merge
shared release changes forward into the development track afterward.

## Rehearsal Without Publication

Use an already supported upstream tag and a recorded plugin candidate. Perform
selection, regression/build validation, disposable runtime acceptance, and pin
checks as above. Save the receipt and logs, clearly labelled **rehearsal**.
Do not run version-changing, tagging, or publishing commands during a rehearsal.

A completed rehearsal demonstrates the preparation path. It does not announce
support for an unreleased version or replace validation when a new upstream
release and candidate become available.
