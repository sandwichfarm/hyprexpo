# Hyprland build cache

Compatibility builds and the upstream-tip probe use a signed Nix binary cache
hosted on Bunny. Nix checks the exact output store path before compiling Hyprland.
A matching build is downloaded; a missing build is compiled. Trusted branch runs
publish its runtime and development-header closures before compiling the plugin,
so a plugin failure does not discard a successful Hyprland build.

The cache identity includes the Hyprland commit, resolved dependencies, target
platform, overlays and build recipe. A version label alone is insufficient.
Changing plugin source does not invalidate an otherwise identical Hyprland build.
The existing plugin `--rebuild` comparison and generated-header audit remain active.

## One-time repository configuration

Use a **dedicated Bunny Storage zone and pull zone**. Do not reuse the website's
storage zone: `deploy-site.yml` deletes files outside the website build output.
Point the pull zone at the cache storage zone and enable HTTPS. Configure:

| Kind | Name | Value |
| --- | --- | --- |
| Variable | `BUNNY_CACHE_URL` | Public HTTPS URL including prefix, e.g. `https://cache.example.net/hyprland-nix` |
| Variable | `BUNNY_CACHE_PUBLIC_KEY` | Nix signing public key, `name:base64` |
| Variable | `BUNNY_CACHE_PREFIX` | Optional storage prefix; defaults to `hyprland-nix` |
| Variable | `BUNNY_CACHE_STORAGE_ENDPOINT` | Optional regional HTTPS storage origin; defaults to `https://storage.bunnycdn.com` |
| Secret | `BUNNY_CACHE_STORAGE_ZONE` | Dedicated storage zone name |
| Secret | `BUNNY_CACHE_STORAGE_PASSWORD` | That zone's Storage API password, not the account API key |
| Secret | `BUNNY_CACHE_SIGNING_KEY` | Nix signing private key |

The public URL must map to the same storage prefix used for uploads. Disable
caching of 404 responses on the pull zone, so newly published paths are immediately
available. Store NAR payloads and narinfo metadata without content transformation.

Generate a signing pair on a trusted machine with Nix installed:

```bash
umask 077
NIX_REMOTE=dummy:// nix-store --generate-binary-cache-key hyprexpo-hyprland-1 cache-private.key cache-public.key
gh secret set BUNNY_CACHE_SIGNING_KEY --repo sandwichfarm/hyprexpo < cache-private.key
gh variable set BUNNY_CACHE_PUBLIC_KEY --repo sandwichfarm/hyprexpo < cache-public.key
```

Alternatively, run `./scripts/configure-bunny-cache.sh`. It prompts for all
five values, explains where to find each one (including signing-key generation),
keeps secret prompts hidden, and sends secret values to `gh` on stdin.
Pass `--repo OWNER/REPO` to target another repository.

`NIX_REMOTE=dummy://` allows key generation without initializing or writing to
the system Nix store, so it also works when `/nix/store` is unavailable.

Keep the private key outside the checkout and secure or remove the temporary key
file after recording it in the secret manager. Never put it in build artifacts.
Nix's [file binary cache](https://releases.nixos.org/nix/nix-2.32.2/manual/store/types/local-binary-cache-store.html)
signs exported paths. Consumers retain signature verification and the standard
Nix caches and trust keys.

## Run behavior

- Pull requests, including same-repository PRs, get public cache reads only.
  Missing dependencies still build, but PR code never receives write credentials.
- Pushes and manual runs on `master` or `hyprland-git`, plus the scheduled upstream
  probe, can populate the cache. A manual run from another branch is read-only.
- Payloads upload before path metadata. Existing content-addressed payloads can
  be skipped. A failed upload fails the job and leaves no new metadata referring
  to that failed payload. Already published valid entries remain reusable.
- With neither cache URL nor public key configured, builds use the existing Nix
  caches and local compilation. Partial read configuration fails clearly. A
  configured writer missing credentials also fails instead of silently losing
  the cache upload.
- Cache outages permit local compilation through Nix fallback. Publishing failures
  remain visible failures on writer runs.

## Validation

After landing the workflow and configuring the cache, dispatch Compatibility on
`master` once to populate release builds. Repeat on a fresh runner. Compare
`hyprland-build.log`: the second run should fetch Hyprland from the Bunny URL and
contain no Hyprland compilation. The plugin still compiles and passes its rebuild
comparison. Both runs must preserve the same exact lock and Hyprland derivation.

Evidence includes `hyprland-build.json`, `hyprland-build.log`, and, for writers,
`bunny-cache-paths.txt` / `bunny-cache-check.log`. The latter records the pre-upload
closure check and can show missing paths on a successful cold run.

Local checks:

```bash
make test-tooling
bash -n scripts/ci-build.sh scripts/ci-hyprland-cache.sh
```
