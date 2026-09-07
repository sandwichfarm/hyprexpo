# Repository Layout

The plugin keeps its private C++ implementation and headers together in `src/`.
Build and package entry points remain at the root so existing source, hyprpm,
and Nix installation commands retain their interfaces.

| Location | Contents |
| --- | --- |
| `src/` | Production C++ sources and private headers. There is no separately installed public C++ header API. |
| `scripts/` | Build, development, validation, release, and configuration-migration helpers; the compatibility target JSON used by CI scripts. |
| `tests/` | C++ regression suites and Python CI contract tests. |
| `.github/workflows/` | GitHub Actions workflow definitions. |
| `docs/` and `site/` | Documentation and website sources. |
| Root | README, license, version, build definitions, package metadata, and dependency lock. |

`make all` still produces `hyprexpo.so` at the root, as required by hyprpm.
CMake and Meson retain their out-of-tree build and package installation paths.
Internal C++ includes remain local to `src/`; tests explicitly reference those
private headers and read production source from its new location.

The configuration migration helper is now
`scripts/hyprexpo-migrate-config.py`; its arguments and behavior are unchanged.
The CI target data lives beside its reader as `scripts/hyprland-targets.json`.
Run its Python checks with:

```sh
python3 -m unittest discover -s tests -p 'test_ci_*.py' -v
```

Disposable experiments belong in the ignored local `spikes/` directory. The
retired scrolling API probe, its dedicated historical runner, and its probe-only
source test are available in Git history. Production validation retains its
regression suites, sanitizer build, diagnostic reader, and input oracle; it no
longer depends on ignored artifacts from that completed experiment.

## Carrying the Layout into hyprland-git

Use a reviewed forward-merge PR into `hyprland-git`. Preserve development API
adaptations rather than copying the release branch's C++ files over them.

1. Start an isolated integration branch from the current `origin/hyprland-git`.
   Record both branch tips and keep the development Hyprland pin and lock.
2. If the branches have not yet reconciled the shared CI/docs baseline, merge
   the pre-cleanup `master` checkpoint first. Shared work arrived on `master`
   through a squash merge, so Git history alone can make equivalent content
   look independently added. Resolve target-specific README and Nix differences
   while retaining the development upstream revision and dependency graph.
3. Merge the `master` commit containing this cleanup. With the shared baseline
   in ancestry, Git can carry development edits through the source renames and
   recognize the retired directories as deletions. Use the actual landed
   cleanup commit after a squash merge, not an obsolete PR-branch hash.
4. Resolve remaining path conflicts explicitly. Keep development C++ content
   under `src/`, source-test assertions for its API, and the input helper's Lua
   transport and independent shell-variable initialization. Retain the restored
   README demonstration video and the shared build/script layout.
5. Check that no root C++ files or retired directories reappear, then compare
   every moved production file with the pre-merge development source. Explain
   any content change separately from the relocation. Verify release pins and
   the development flake/lock remain intact.
6. Run the development branch's regression, script, documentation, pin, and
   exact-upstream build checks before opening the reconciliation PR. Merge it
   with a merge commit to preserve contributor and branch history.

The cleanup PR records a disposable rehearsal against the branch tips available
when it was prepared. Repeat these checks against current tips before performing
the real reconciliation; the rehearsal does not modify the published branches.

### Rehearsal checkpoint

The cleanup was rehearsed with pre-cleanup `master` at
`d4e3c6ba207da56f6bc47ecb3eb88091d2a2a742` and `hyprland-git` at
`e20d9ad7fe29aaee3bab4b5ecaaa523aae1b7e92`:

- The baseline merge had one conflict: the independently added `flake.lock`.
  Retaining the development lock preserved its exact upstream and dependencies.
- The subsequent layout merge applied cleanly. All 36 development C++ source
  and header files retained identical contents under `src/`; development Lua
  transport, helper fixes, release pins, and the restored demo video survived.
- The three C++ suites, four Python checks, and 39-case input oracle passed.
  An actual Nix build against the retained development commit produced the same
  plugin bytes as before cleanup. Both released targets also retained identical
  Nix plugin bytes after relocation.

These results validate the strategy at those checkpoints. Recheck conflicts and
verification whenever either published branch has advanced.
