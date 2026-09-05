# Native scrolling API probe

This isolated Hyprland 0.56.1 plugin is the blocking empirical gate for issue #85. It does not link to or modify `hyprexpo.so`.

Build it only against the exact installed ABI:

```sh
test "$(pkg-config --modversion hyprland)" = 0.56.1
cmake -S spikes/scrolling-api -B /tmp/w7w-probe-build
cmake --build /tmp/w7w-probe-build
```

The nested harness loads the module and invokes:

```text
hyprexpo-scroll-probe:inspect REQUEST_ID|/tmp/hyprexpo-scroll-probe-REQUEST_ID.ppm
```

Each request writes one tight target PPM and one `HYPREXPO_SCROLL_PROBE` JSON log record. The record contains exact ABI identity, native direction/offset/column/row/window topology before and after capture, pixel dimensions/bounds/SHA-256, renderer restoration, and an explicit no-mutation outcome.

Run the retained fixture through `scripts/run-scrolling-probe.sh --non-interactive`. Never load this experimental module into the production compositor session.
