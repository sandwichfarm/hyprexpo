# Open pull request visual proofs — 2026-09-02

This branch is a dedicated evidence surface. The media is intentionally separate
from every pull request head so adding reviewer-visible proof does not change any
reviewed code diff. Public links should pin the final commit containing this
manifest and these assets, not this mutable branch name.

All captures ran in nested Hyprland **0.56.1** with runtime/client ABI commit
[`5c9377c15f85c50648f35ca5a213754f95b93ca0`](https://github.com/hyprwm/Hyprland/commit/5c9377c15f85c50648f35ca5a213754f95b93ca0).

## Attribution

| PR | Captured head | Demonstrated behavior | Screenshot | WebM |
|---|---|---|---|---|
| [#103 — feat(expo): add an all-monitors mode](https://github.com/sandwichfarm/hyprexpo/pull/103) | [`707012ac5f15b3090054faec26b08423a0356cc7`](https://github.com/sandwichfarm/hyprexpo/commit/707012ac5f15b3090054faec26b08423a0356cc7) | One toggle opens simultaneous workspace overviews on two monitors. | [PNG](./pr-103-all-monitors.png) | [WebM](./pr-103-all-monitors.webm) |
| [#104 — Fix center-current grid bounds](https://github.com/sandwichfarm/hyprexpo/pull/104) | [`5c6778a5aaaa3e95b70561e7dfc847ed0c12db94`](https://github.com/sandwichfarm/hyprexpo/commit/5c6778a5aaaa3e95b70561e7dfc847ed0c12db94) | Center-current selection stays in bounds at workspaces 1, 5, and 9. | [PNG](./pr-104-center-current-bounds.png) | [WebM](./pr-104-center-current-bounds.webm) |
| [#105 — Keep pinned windows from cancelling close animation](https://github.com/sandwichfarm/hyprexpo/pull/105) | [`ab5fd0a976021de6336831007e440b1bde8ee296`](https://github.com/sandwichfarm/hyprexpo/commit/ab5fd0a976021de6336831007e440b1bde8ee296) | The pinned window persists while the workspace close transition completes. | [PNG](./pr-105-pinned-close-animation.png) | [WebM](./pr-105-pinned-close-animation.webm) |
| [#107 — Add a native scrolling-layout overview](https://github.com/sandwichfarm/hyprexpo/pull/107) | [`4d4311797ebdd5184b62dfee2478490a8c9a9073`](https://github.com/sandwichfarm/hyprexpo/commit/4d4311797ebdd5184b62dfee2478490a8c9a9073) | The native scrolling-layout overview opens and presents window-level previews. | [PNG](./pr-107-scrolling-overview.png) | [WebM](./pr-107-scrolling-overview.webm) |

## Media integrity

| File | Format / codec | Dimensions | Duration | Bytes | SHA-256 |
|---|---|---:|---:|---:|---|
| `pr-103-all-monitors.png` | PNG | 905×507 | still | 383381 | `00b3ca09ea7385caa23c6259e88d337ed43fef612d38e5193e565188e55f3305` |
| `pr-103-all-monitors.webm` | WebM / VP9 | 926×554 | 9.000 s | 1211734 | `e22d02bc57a5b9f234a74caddb4c06b52ad5b41574dd85704583bd452c0b886e` |
| `pr-104-center-current-bounds.png` | PNG | 1800×328 | still | 114543 | `af00cc984886066ef06f08933d66fef5839594616dd11c97c52d5e56b3f4a9ed` |
| `pr-104-center-current-bounds.webm` | WebM / VP9 | 926×506 | 20.051 s | 8282445 | `db3657a673b49476262f380bc3a8e44db60ccb8eb3435a88c9883c7f842c7506` |
| `pr-105-pinned-close-animation.png` | PNG | 1800×164 | still | 63406 | `423cd737182d6d86fc3a96df392403bf76c09226c1e377a9e932c7fdaceb98a0` |
| `pr-105-pinned-close-animation.webm` | WebM / VP9 | 926×554 | 9.000 s | 452706 | `185bd89b17d277ff63d2515f5c2380e66c4edf0a9fc9334db603e094f8579473` |
| `pr-107-scrolling-overview.png` | PNG | 927×507 | still | 26553 | `d256b67cc620cffe5bd60e16363d4c126576f315c148efb51db823e660ec2b3c` |
| `pr-107-scrolling-overview.webm` | WebM / VP9 | 926×554 | 10.000 s | 212459 | `60b5954f113662d73fd20d3d341af1708ad8a586e5f6b3838e8c316c7722f191` |

The selected files were decoded before publication. The four screenshots and
sampled frames across all four videos were also inspected for accidental
credential or private-data disclosure.
