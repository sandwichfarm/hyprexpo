# Installing the Development Track

Use the development track only with its matching Hyprland source revision and
dependency environment. A plugin built for a tagged release is not a substitute
for a development build, and a development branch name is not proof that an
arbitrary upstream tip is compatible.

The development candidate targets Hyprland
`34eb03bd8da01024596c367fba66485a8c9b8ca7`. Before installing, check the candidate's
flake lock and the running compositor's `hyprctl version` output against the
[validation receipt](../reference/workflow-validation.md). Test in a disposable
compositor before changing a desktop.

Commands below using `hyprland-git` assume that branch contains the validated
candidate and matching lock; branch creation alone is insufficient. Test an
unmerged candidate from its checkout in a disposable compositor. Do not save
its PR hash or temporary branch in a desktop's hyprpm registration: a later
squash merge or branch deletion can break updates.

## hyprpm Revision Selection

Hyprpm accepts an optional Git revision after the repository URL. Its
implementation clones the repository and resets to that revision. For a
non-default branch, use its remote-tracking ref in the fresh clone. From a
HyprExpo checkout, use the guarded registration wrapper:

```sh
hyprpm update
./scripts/hyprpm-add.sh https://github.com/sandwichfarm/hyprexpo origin/hyprland-git
hyprpm enable hyprexpo
hyprpm reload
```

The explicit revision bypasses `commit_pins`. This is intentional for the
chase track; do not use it on released Hyprland to bypass a failed compatibility
check. A bare `hyprland-git` name does not resolve in a fresh clone where only
`master` exists locally. `origin/hyprland-git` does.

For an immutable plugin candidate already retained by maintained history, use
its full commit hash in place of `origin/hyprland-git`. The guard accepts hashes
reachable from `master`, `hyprland-git`, declared `release/<version>` branches,
or versioned release tags in a fresh clone. Temporary branch names and commits
reachable only through PR history are rejected before registration. Keep the
corresponding compositor revision and dependency environment together.

For unmerged PR tests, use `make dev-reload` or `./scripts/run-nested.sh` in the
test checkout without changing managed state. Direct revision-specific hyprpm
tests belong in a disposable environment whose registration is discarded with
the test. They do not establish a persistent installation or future availability.

Hyprpm cannot add the same repository twice. To change the selected revision,
first disable and remove the existing registration, then add the intended
revision and enable it. Save the previous plugin/compositor revisions before
switching. Do not overwrite a loaded shared object in place.

### Update and Verify

Run `hyprpm update` to rebuild/update against the installed compositor, then
`hyprpm reload` in the matching running session. An explicit branch revision
follows that branch; a full commit hash remains fixed. Both initial addition
and revision-specific updating must succeed before calling a consumer path
validated. If an update fails, keep the verbose log and return to the previous
tested plugin/compositor pair rather than bypassing compatibility checks.

Check all of the following:

- `hyprctl version` identifies the expected running upstream commit.
- `hyprpm list` shows the intended repository/revision without a failed build.
- The installed repository state and verbose build log identify the selected
  plugin commit and matching headers/dependency environment.
- `hyprctl plugin list` confirms loading, and the compositor reports no new
  configuration errors. Exercise overview opening, selection, cancellation,
  and unloading in the disposable session.

Hyprpm's progress messages alone are insufficient: inspect individual plugin
failure state and the resulting artifact. For failures, rerun the command with
`--verbose` and retain the log.

See the [official plugin installation guide](https://wiki.hypr.land/Plugins/Using-Plugins/)
and the installed `hyprpm --help` for the supported command surface. The revision
selection and pin-bypass behavior are implemented in
[Hyprland's plugin manager](https://github.com/hyprwm/Hyprland/blob/34eb03bd8da01024596c367fba66485a8c9b8ca7/hyprpm/src/core/PluginManager.cpp).

The completed development manager proof used an isolated Ubuntu VM with an
exact Nix dependency shell, genuine generated headers, and a transparent
pkg-config query adapter. See the [validation environment recipe](../reference/hyprpm-validation-environment.md)
for its privileged-tool availability and cleanup-ACL setup. This is evidence
for that Nix-assisted hyprpm path, not a claim that an unprepared generic Arch
installation was tested. The direct Nix consumer below needs no hyprpm adapter.

## Nix Consumer

Select compatible plugin source and make its Hyprland input follow the same
locked input as your compositor. For example, these consumer inputs select the
pinned development target:

```nix
inputs = {
  hyprland.url = "github:hyprwm/Hyprland/34eb03bd8da01024596c367fba66485a8c9b8ca7";
  hyprexpo = {
    url = "github:sandwichfarm/hyprexpo/hyprland-git";
    inputs.hyprland.follows = "hyprland";
  };
};
```

Generate and retain the consumer's `flake.lock`. Record the resolved plugin
commit as well as the Hyprland commit; the branch URL becomes reproducible only
through its lock. During candidate validation, substitute the published
candidate commit or branch for `hyprland-git`.

Use `inputs.hyprexpo.packages.${system}.hyprexpo` as the plugin and the matching
Hyprland package set as the compositor. The repository also exposes its matching
`hyprland` package to support an explicit plugin/compositor build pair. Following
a system input aligns dependencies only when the selected plugin source supports
that input's API; `follows` is not a compatibility adapter.

For a direct build of a recorded repository candidate, replace the value below
with its full tested plugin commit:

```sh
plugin_commit=REPLACE_WITH_TESTED_PLUGIN_COMMIT
plugin_ref="github:sandwichfarm/hyprexpo/$plugin_commit"
nix build "$plugin_ref#hyprexpo" --out-link result-plugin --json > plugin-build.json
nix build "$plugin_ref#hyprland" --out-link result-hyprland --json > compositor-build.json
```

These direct builds use the candidate's committed lock. Start
`result-hyprland/bin/Hyprland` with a disposable configuration and private runtime
directory. Identify that test session's instance signature, verify its version,
and load `result-plugin/lib/libhyprexpo.so` using the matching `hyprctl`:

```sh
test_signature=REPLACE_WITH_DISPOSABLE_INSTANCE_SIGNATURE
result-hyprland/bin/hyprctl -i "$test_signature" version
result-hyprland/bin/hyprctl -i "$test_signature" plugin load \
  "$(readlink -f result-plugin/lib/libhyprexpo.so)"
```

Run these commands within the disposable session's environment, including its
private `XDG_RUNTIME_DIR`. A signature from another runtime directory will not
identify it. Use the consumer's locked package outputs instead when testing an
input override, and record that consumer lock separately.

Preserve build JSON, derivation paths, dependency closure, lock-file checksum,
and runtime evidence. Successful flake evaluation alone does not validate
installation. Do not distribute a Nix-linked shared object as a generic Arch
plugin binary.

Update plugin and compositor inputs deliberately, inspect their new locked
commits, and repeat the build/runtime checks before switching the running
session. Preserve the old consumer lock and system generation for rollback.

## Troubleshooting and Rollback

| Symptom | Check and recovery |
| --- | --- |
| Git revision cannot be checked out | Check the saved override and follow [saved-revision recovery](../troubleshooting.md#hyprpm-cannot-check-out-a-saved-revision). Verify any replacement against maintained history in a fresh clone. |
| Missing or changed C++ APIs | Compare the source revision with the exact compositor target. Select a compatible candidate before retrying. |
| Header or plugin ABI mismatch | Confirm the running session matches the installed compositor, then regenerate headers/rebuild with its dependency environment. Restart into the intended compositor when disk and running versions differ. |
| Nix input follows the system but build fails | Check source compatibility and the complete lock, not only the top-level Hyprland input. Retain the failing build log. |
| Plugin builds but interaction fails | Keep the evidence and return to the previously tested plugin/compositor pair; do not call the build runtime support. |

To return to released Hyprland with hyprpm, disable/remove the chase
registration, restore a supported released compositor, and start a matching
session. Then run `hyprpm update`, add the repository **without a revision
argument**, enable it, and reload. Omitting the revision restores release-pin
selection. Verify the selected plugin commit and run a smoke check again.

For Nix, restore the previous consumer lock/system generation containing the
matching compositor and plugin, rebuild or activate that generation as
appropriate, and start the matching session. Reverting only the plugin while
keeping an incompatible compositor is not a complete rollback.
