#!/usr/bin/env python3
"""Reject hyprpm registrations and replacements tied to temporary history."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import tomllib


RELEASE_TAG = r"v[0-9]+\.[0-9]+\.[0-9]+(?:\+[0-9]+)?"
RETAINED_BRANCH = r"(?:master|hyprland-git|release/[0-9]+(?:\.[0-9]+)*)"


def retained_ref(ref):
    return bool(
        re.fullmatch(r"refs/remotes/origin/" + RETAINED_BRANCH, ref)
        or re.fullmatch(r"refs/tags/" + RELEASE_TAG, ref)
    )


def check_state(state):
    try:
        with state.open("rb") as stream:
            data = tomllib.load(stream)
    except FileNotFoundError:
        if state.is_symlink():
            raise ValueError("state.toml is a dangling symlink")
        return "no managed state at this destination"

    return check_repository(data.get("repository"))


def check_repository(repository):
    if not isinstance(repository, dict):
        raise ValueError("state.toml has no repository table")
    rev = repository.get("rev", "")
    if not isinstance(rev, str):
        raise ValueError("repository.rev must be a string")
    if not rev:
        return "no explicit revision override; hyprpm can select compatibility pins"

    # A temporary branch can disappear even when its current commit is on master.
    if not (
        re.fullmatch(r"[0-9a-fA-F]{40}", rev)
        or rev in ("master", "refs/heads/master")
        or retained_ref(rev)
        or retained_ref("refs/remotes/" + rev)
        or re.fullmatch(RELEASE_TAG, rev)
    ):
        raise ValueError("repository.rev names temporary or unsupported history: " + rev)

    url = repository.get("url")
    if not isinstance(url, str) or not url.strip():
        raise ValueError("an explicit revision requires repository.url")

    # Do not inherit another checkout's objects, refs, or working tree. In
    # particular, an old PR object in a local clone must not make this pass.
    environment = os.environ.copy()
    for name in (
        "GIT_DIR", "GIT_COMMON_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE",
        "GIT_OBJECT_DIRECTORY", "GIT_ALTERNATE_OBJECT_DIRECTORIES",
        "GIT_SHALLOW_FILE", "GIT_NAMESPACE",
    ):
        environment.pop(name, None)
    environment["GIT_TERMINAL_PROMPT"] = "0"

    def git(*args):
        try:
            return subprocess.run(
                ["git", *args], capture_output=True, text=True,
                env=environment, timeout=60,
            )
        except subprocess.TimeoutExpired:
            # TimeoutExpired includes the command, which may contain URL credentials.
            raise ValueError("Git verification timed out; installation refused") from None

    with tempfile.TemporaryDirectory(prefix="hyprexpo-revision-check-") as directory:
        clone = str(Path(directory) / "repo")
        # --no-local is essential for local URLs: copying the object directory
        # would retain unreachable commits which a normal remote clone omits.
        # Empty templates prevent injected local refs and alternate object stores.
        result = git("clone", "--quiet", "--no-checkout", "--no-local", "--template=", "--", url, clone)
        if result.returncode:
            raise ValueError("cannot verify repository.url with a fresh clone; installation refused")
        result = git("-C", clone, "rev-parse", "--verify", "--end-of-options", rev + "^{commit}")
        if result.returncode:
            raise ValueError("saved revision is unavailable in a fresh clone: " + rev)
        commit = result.stdout.strip()
        result = git("-C", clone, "for-each-ref", "--format=%(refname)", "refs/remotes/origin/", "refs/tags/")
        if result.returncode:
            raise ValueError("cannot enumerate retained upstream history")
        for ref in result.stdout.splitlines():
            if not retained_ref(ref):
                continue
            result = git("-C", clone, "merge-base", "--is-ancestor", commit, ref + "^{commit}")
            if result.returncode == 0:
                return f"saved revision {rev} is retained by {ref}"
            if result.returncode != 1:
                raise ValueError("cannot verify retained upstream history: " + ref)
        raise ValueError("saved revision is not retained by a release tag or maintained branch: " + rev)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("state", nargs="?", type=Path, help="state.toml beside the managed destination (do not resolve the .so symlink)")
    parser.add_argument("--url", help="repository URL for a proposed registration")
    parser.add_argument("--revision", help="proposed revision, or an empty string for compatibility pins")
    args = parser.parse_args()
    proposal = args.url is not None or args.revision is not None
    if proposal and (args.state is not None or args.url is None or args.revision is None):
        parser.error("use either a state path or both --url and --revision")
    if not proposal and args.state is None:
        parser.error("a state path or both --url and --revision are required")
    try:
        if proposal:
            if not args.url.strip():
                raise ValueError("repository URL must not be empty")
            message = check_repository({"url": args.url, "rev": args.revision})
        else:
            message = check_state(args.state)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        location = "proposed registration" if proposal else str(args.state)
        print(f"error: unsafe hyprpm state at {location}: {error}", file=sys.stderr)
        print(
            "No plugin or state was changed by this check. Use make dev-reload or a nested session for PR testing.\n"
            "For recovery, see docs/troubleshooting.md; do not replace the saved revision with another PR hash.",
            file=sys.stderr,
        )
        return 1
    print("hyprpm state verified: " + message)
    return 0


if __name__ == "__main__":
    sys.exit(main())
