#!/usr/bin/env python3
"""Create a local, non-publishing release preparation packet from an observation."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("observation", type=Path)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = json.loads(args.observation.read_text())
    matches = [release for release in report.get("eligible_releases", []) if release.get("tag") == args.tag]
    if len(matches) != 1:
        print(f"release {args.tag!r} is not one unique eligible observation target", file=sys.stderr)
        return 2
    release = matches[0]
    packet = {
        "schema_version": 1,
        "status": "prepared_not_published",
        "release": release,
        "plugin_base": report["plugin_base"],
        "development_target": report["development_target"],
        "release_targets": report["release_targets"],
        "required_gates": [
            "exact upstream tag/peeled commit",
            "compatible plugin candidate/tree and lock",
            "proposed support and maintenance decision",
            "exact build/rebuild provenance",
            "disposable runtime receipt",
            "pin ancestry and final integrated-tree validation",
            "explicit maintainer release approval",
        ],
        "publication": "forbidden by this preparation command",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(packet, indent=2, sort_keys=True) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
