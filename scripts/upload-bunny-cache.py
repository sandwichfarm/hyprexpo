#!/usr/bin/env python3
"""Publish a Nix file cache to a dedicated Bunny Storage zone."""

from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import re
import sys
from urllib.error import HTTPError
from urllib.parse import urlsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener


class NoRedirects(HTTPRedirectHandler):
    def redirect_request(self, request, response, code, message, headers, url):
        # Storage API requests carry a zone password, including HEAD requests.
        raise HTTPError(request.full_url, code, "Storage redirects are forbidden", headers, response)


open_storage = build_opener(NoRedirects()).open


def publish(directory, environ=os.environ, open_url=open_storage):
    root = Path(directory)
    zone = environ.get("BUNNY_CACHE_STORAGE_ZONE", "")
    password = environ.get("BUNNY_CACHE_STORAGE_PASSWORD", "")
    endpoint = environ.get("BUNNY_CACHE_STORAGE_ENDPOINT") or "https://storage.bunnycdn.com"
    prefix = environ.get("BUNNY_CACHE_PREFIX") or "hyprland-nix"
    parsed = urlsplit(endpoint)
    if (parsed.scheme != "https" or not parsed.hostname or parsed.username
            or parsed.password or parsed.path not in ("", "/") or parsed.query or parsed.fragment):
        raise ValueError("Cache storage endpoint must be an HTTPS origin")
    if not re.fullmatch(r"[A-Za-z0-9_-]+", zone) or not password:
        raise ValueError("Dedicated cache storage zone and password are required")
    if any(not re.fullmatch(r"[A-Za-z0-9_-]+", part) for part in prefix.split("/")):
        raise ValueError("Invalid cache prefix")
    if not (root / "nix-cache-info").is_file():
        raise ValueError("Missing nix-cache-info")
    files = sorted(p for p in root.rglob("*") if p.is_file())
    for path in files:
        if path.is_symlink() or root.resolve() not in path.resolve().parents:
            raise ValueError("Cache files must stay inside the staging directory")
        if not re.fullmatch(r"[A-Za-z0-9._/-]+", path.relative_to(root).as_posix()):
            raise ValueError("Unexpected cache filename")
    blobs = [p for p in files if p.relative_to(root).parts[0] == "nar"]
    metadata = [p for p in files if p.suffix == ".narinfo"]

    def upload(path):
        relative = path.relative_to(root).as_posix()
        url = f"{endpoint.rstrip('/')}/{zone}/{prefix}/{relative}"
        # NAR filenames are content addressed. Metadata is always refreshed.
        if relative.startswith("nar/"):
            try:
                with open_url(Request(url, headers={"AccessKey": password}, method="HEAD"), timeout=60):
                    return
            except HTTPError as error:
                error.close()
                if error.code not in (404, 405):
                    raise RuntimeError(f"Bunny cache lookup failed (HTTP {error.code})") from None
        with path.open("rb") as content:
            request = Request(url, data=content, method="PUT", headers={
                "AccessKey": password,
                "Content-Type": "application/octet-stream",
                "Content-Length": str(path.stat().st_size),
            })
            try:
                with open_url(request, timeout=300) as response:
                    if not 200 <= response.status < 300:
                        raise RuntimeError("Bunny cache upload failed")
            except HTTPError as error:
                error.close()
                raise RuntimeError(f"Bunny cache upload failed (HTTP {error.code})") from None

    # No narinfo is published until every payload has successfully uploaded.
    with ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(upload, blobs))
        list(pool.map(upload, metadata))
    upload(root / "nix-cache-info")
    print(f"Published Bunny cache: {len(blobs)} payloads, {len(metadata)} path records")


if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise ValueError("usage: upload-bunny-cache.py CACHE_DIRECTORY")
        publish(sys.argv[1])
    except Exception as error:
        # Network exceptions can include authenticated request details.
        message = str(error) if isinstance(error, (ValueError, RuntimeError)) else type(error).__name__
        print(f"Bunny cache publication failed: {message}", file=sys.stderr)
        sys.exit(1)
