---
name: hyprexpo-chase
description: Observe Hyprland upstream, prepare one bounded development compatibility candidate, or prepare a release packet without publishing.
---

# HyprExpo chase

Run `python3 scripts/watch-hyprland.py --format markdown` first. Read the
report and [chasing Hyprland](../../../docs/guides/chasing-hyprland.md) before
changing source, branches, or CI state.

## Modes

- `observe`: Read-only. Report exact target, upstream tip, release candidates,
  probe evidence, duplicate candidates, and missing information.
- `chase`: One exact upstream commit and one owned candidate branch. Work only
  in an isolated checkout. Keep `hyprland-git` protected until reviewed merge.
- `release`: Prepare a local packet and promotion PR for one exact upstream
  release. Do not create tags, releases, or prereleases.
- `status`: Report current observer output and existing candidate state.

## Required rules

- Existing Actions are read-only observers. One designated manual runner owns
  candidate writes. Do not run concurrent repair agents across machines.
- Preserve pins, managed-installation guards, contributor history, and live
  desktop state. Test PR artifacts with disposable sessions or `make dev-reload`;
  never save a temporary revision to managed hyprpm state.
- Bind all evidence to exact plugin tree, upstream commit, lock, and test scope.
  A compile or old artifact is not runtime acceptance for a new target.
- Use at most three repair hypotheses and two hours of active work. On failure,
  leave a resumable draft with diagnosis and missing gates.
- Release preparation is not publication. Follow the existing promotion guide
  and require explicit authorization before tags, assets, or release changes.

Hermes is not a first-version runner. It may reuse this procedure only after a
separate identity, credential, writer-lock, scheduler, and pilot validation.
