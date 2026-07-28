# Releasing hyprexpo

The plugin version has a **single source of truth: the `VERSION` file** (e.g.
`v0.55.2+2`). It is baked into the binary at build time and used to create the
git tag, so the version reported by the plugin, the `VERSION` file, and the git
tag can never drift apart.

## Cutting a release

Run this from updated `master`, after all compatibility work has landed:

```sh
make check-pins        # 1. verify every hyprpm plugin pin is on master's history
make version v0.56.2   # 2. write + commit the VERSION file
make tag               # 3. create the matching annotated git tag
make publish           # 4. push branch + tag (triggers the release workflow)
```

The second hash in each `hyprpm.toml` `commit_pins` entry is a hyprexpo commit.
It must be the commit that actually landed on `master`. In particular, do not
copy a commit hash from a PR branch when that PR will be squash-merged: the
squash commit has a different hash. `make check-pins` catches missing and
non-ancestor hashes before a release is tagged.

The `version`, `tag`, and `publish` targets repeat the relevant pin check, and
the `Release` workflow verifies both the pins and the `VERSION`/tag match before
it runs the tests, builds `hyprexpo.so`, and publishes a GitHub release.

### Notes

- The version must look like `v1.2.3` or `v1.2.3+4`; `make version` rejects
  anything else.
- `make version` (no argument) prints the current baked version.
- `make tag` refuses to run if the `VERSION` file has uncommitted changes or the
  tag already exists. It also refuses uncommitted `hyprpm.toml` changes, so the
  checked pins are the pins included in the release.
- `make check-pins REF=v1.2.3` can audit the pins stored in an existing tag or
  commit.
- **Development builds** (anything not built exactly on a release tag) report a
  `…-dev+<shorthash>` suffix, so crash reports can be told apart from real
  releases. Builds sitting on the tag report the clean version.
- The version is baked the same way by all three build paths
  (`Makefile`, `meson.build`, `CMakeLists.txt`) via `scripts/version.sh`.
