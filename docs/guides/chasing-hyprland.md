# Chasing Hyprland

HyprExpo supports released Hyprland targets on `master` and one exact development
target on `hyprland-git`. This procedure observes upstream changes, prepares
reviewable compatibility work, and prepares release packets. It does not publish
or merge by itself.

## Observe

Run from an up-to-date checkout with GitHub CLI access:

```sh
python3 scripts/watch-hyprland.py --format markdown
```

The report is read-only. It records the development pin, upstream `main`,
release candidates, current probe evidence, and existing candidates. A failed
observation is incomplete evidence, never proof that no work exists.

The scheduled upstream-tip workflow runs this same observation before its
expensive build. Scheduled GitHub workflows are best effort and run from the
default branch; a delayed or missed schedule is not a compatibility result.

## Prepare one development target

Only the designated manual runner writes candidates. Actions observe. Do not run
another writer from Hermes or a second host until a shared writer lease exists.

1. Save the observer report. If a matching candidate or live job exists, inspect
   it and stop; do not duplicate it.
2. Create an isolated branch from current `hyprland-git` using the exact upstream
   commit from the report. Do not use the desktop checkout.
3. Update the development target, `flake.nix`, and resolved lock together.
   Preserve release targets and hyprpm pins.
4. Diagnose the exact build failure against the captured upstream source. Keep
   existing behavior tests meaningful; do not fake headers, change the target,
   disable sandboxing, or suppress features to obtain a build.
5. Run tooling, C++ suites, relevant input tests, documentation build, exact
   build, and affected disposable runtime proof. Record hardware gaps.
6. Push one owned candidate PR into `hyprland-git`. Keep incomplete work draft
   and include exact source/tree/lock/probe/runtime evidence plus the next gate.

The standing tracking PR uses the development contract only while it remains the
configured same-repository draft tracker. It is not a release-promotion PR.

## Prepare a released target

Run observation first. A new stable release is a separate work item identified
by release ID, tag, and peeled tag commit. Old unsupported releases, drafts,
prereleases, malformed tags, and a tag that moved need explicit review.

Choose a compatible historical candidate if `hyprland-git` has moved beyond the
release. Create a local packet and a reviewable promotion PR containing exact
upstream/plugin/tree/lock identities, support decisions, artifacts, runtime
receipt, pin ancestry, notes, and remaining gates. Follow the
[release promotion guide](./release-promotion.md).

Do not create a tag, GitHub Release, or prerelease packet in public state. The
current stable publisher accepts broad `v*` tags; release-candidate publication
requires separate version, trigger, collision, provenance, and authorization
work first.

## Resuming and Hermes

Bound one repair attempt to three hypotheses and two hours of active work. Keep
a receipt when a target remains broken. Before continuing, re-observe refs and
job handles; upstream may have moved or another contributor may own the work.

Hermes is optional later. It must use this procedure as approved context, have
separate read-only observation and constrained candidate-write credentials, an
isolated workspace, persistent receipts, single-writer ownership, capped jobs,
and a tested disable path. It must not receive stable-release publisher access.
