#!/usr/bin/env python3
"""Resolve the CI support contract without consulting moving remote refs."""

import argparse
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def targets(path=ROOT / "ci/hyprland-targets.json"):
    data = json.loads(path.read_text())
    if set(data) != {"release", "development"}:
        raise ValueError("target file must contain release and development")
    for track, rows in data.items():
        if not rows or (track == "development" and len(rows) != 1):
            raise ValueError(f"invalid number of {track} targets")
        names = set()
        revisions = set()
        for row in rows:
            if set(row) != {"name", "rev"}:
                raise ValueError("each target requires name and rev")
            if not re.fullmatch(r"[a-zA-Z0-9._-]+", row["name"]):
                raise ValueError("invalid target name")
            if not re.fullmatch(r"[0-9a-f]{40}", row["rev"]):
                raise ValueError("each target must use a full upstream commit")
            if row["name"] in names or row["rev"] in revisions:
                raise ValueError("duplicate target")
            names.add(row["name"])
            revisions.add(row["rev"])
    return data


def track_for_branch(branch):
    mapping = {"master": "release", "hyprland-git": "development"}
    if branch not in mapping:
        raise ValueError(f"unrecognized target branch: {branch}")
    return mapping[branch]


def verify_lock(metadata, expected):
    locks = metadata["locks"]
    upstream = locks["nodes"][locks["root"]]["inputs"]["hyprland"]
    locked = locks["nodes"][upstream]["locked"]
    if locked.get("rev") != expected or not locked.get("narHash"):
        raise ValueError("resolved Hyprland source differs from requested commit or lacks content hash")
    for name, node in locks["nodes"].items():
        if name == locks["root"]:
            continue
        if not node.get("locked", {}).get("narHash"):
            raise ValueError(f"dependency {name} lacks a locked content hash")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    matrix = commands.add_parser("matrix")
    matrix.add_argument("branch", choices=["master", "hyprland-git"])
    lock = commands.add_parser("verify-lock")
    lock.add_argument("metadata", type=Path)
    lock.add_argument("rev")
    branch_lock = commands.add_parser("verify-branch-lock")
    branch_lock.add_argument("metadata", type=Path)
    branch_lock.add_argument("branch", choices=["master", "hyprland-git"])
    args = parser.parse_args()
    if args.command == "matrix":
        print(json.dumps({"include": targets()[track_for_branch(args.branch)]}))
    elif args.command == "verify-lock":
        verify_lock(json.loads(args.metadata.read_text()), args.rev)
    else:
        metadata = json.loads(args.metadata.read_text())
        locks = metadata["locks"]
        upstream = locks["nodes"][locks["root"]]["inputs"]["hyprland"]
        rev = locks["nodes"][upstream]["locked"]["rev"]
        if rev not in {row["rev"] for row in targets()[track_for_branch(args.branch)]}:
            raise ValueError("branch flake lock differs from its compatibility contract")
        verify_lock(metadata, rev)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, TypeError) as error:
        sys.exit(str(error))
