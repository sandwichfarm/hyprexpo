# Releasing hyprexpo

The plugin version has a **single source of truth: the `VERSION` file** (e.g.
`v0.55.2+2`). It is baked into the binary at build time and used to create the
git tag, so the version reported by the plugin, the `VERSION` file, and the git
tag can never drift apart.

## Cutting a release

```sh
make version v0.55.3   # 1. write + commit the VERSION file
make tag               # 2. create the matching annotated git tag
make publish           # 3. push branch + tag (triggers the release workflow)
```

That's it. The `Release` workflow then verifies the `VERSION` file matches the
pushed tag (`make check-version`), runs the tests, builds `hyprexpo.so`, and
publishes a GitHub release.

### Notes

- The version must look like `v1.2.3` or `v1.2.3+4`; `make version` rejects
  anything else.
- `make version` (no argument) prints the current baked version.
- `make tag` refuses to run if the `VERSION` file has uncommitted changes or the
  tag already exists, so a release always points at a committed version.
- **Development builds** (anything not built exactly on a release tag) report a
  `…-dev+<shorthash>` suffix, so crash reports can be told apart from real
  releases. Builds sitting on the tag report the clean version.
- The version is baked the same way by all three build paths
  (`Makefile`, `meson.build`, `CMakeLists.txt`) via `scripts/version.sh`.
