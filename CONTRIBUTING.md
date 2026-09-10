# Contributing

Follow the [branch policy](docs/reference/branch-policy.md) and validate against
the Hyprland revisions affected by your change.

For periodic development compatibility work, follow
[chasing Hyprland](docs/guides/chasing-hyprland.md). It keeps upstream
observation, candidate repair, and release preparation separate.

## Local Installation Contract

These rules apply to maintainers, contributors, and coding agents.

- Test PR builds with `make dev-reload` or `./scripts/run-nested.sh`. Keep the
  checkout and built artifact available for the test session.
- Do not write a PR commit or temporary branch into the desktop's hyprpm
  `repository.rev`, or set that override to keep a test build installed. A
  squash merge and branch deletion can make the original commit unavailable
  to `hyprpm update`, even when it still exists in your local Git database.
- From a checkout, register a managed installation through
  `./scripts/hyprpm-add.sh <repository-url> [revision]`. Released Hyprland
  normally uses no revision argument so hyprpm can select `commit_pins`.
  The wrapper validates an explicit revision before invoking hyprpm.
- Run `make check-hyprpm-state` before any manual managed replacement.
  `make install` and `scripts/dev-link.sh` enforce this check themselves.
  Do not bypass a failed check by editing `rev` to another test hash.
- Keep supported branch and release-tag history available upstream. Persistent
  hashes must remain reachable from that history; temporary PR refs are not
  retention anchors. Follow the [recovery procedure](docs/troubleshooting.md#hyprpm-cannot-check-out-a-saved-revision)
  when a saved override is already stale.
- Back up the existing state and artifact before intentional replacements;
  never overwrite the inode of a loaded shared object. Verify the matching
  ABI, loaded plugin, and config errors after loading.

The guard uses Git and Python 3.11+ standard-library tooling. Unpinned and
unmanaged destinations pass without network access. Explicit revisions require
a fresh upstream clone; failed verification stops the operation without
changing the plugin or saved state. This verifies revision retention, not ABI
compatibility, and does not intercept direct hyprpm commands or manual edits.

## Guard Regression Tests

Run `make test-tooling` for the Git/installer fixtures and `make test` for the
standalone C++ suites. Tooling tests use disposable repositories and fake plugin
artifacts, never the running desktop or a real publication remote. Both suites
are required by compatibility CI.

The release commands must stop if `check-commit-pins.sh` fails. Never suppress
that failure or publish a PR commit merely because the local object exists.
