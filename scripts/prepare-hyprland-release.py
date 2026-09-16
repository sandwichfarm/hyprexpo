#!/usr/bin/env python3
"""Create a local, non-publishing release preparation packet from an observation."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
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
    tag_commit = release.get("tag_commit")
    if not isinstance(tag_commit, str) or not re.fullmatch(r"[0-9a-f]{40}", tag_commit):
        print(f"release {args.tag!r} lacks one immutable peeled tag commit", file=sys.stderr)
        return 2
    plugin_base = report.get("plugin_base")
    development_target = report.get("development_target")
    if not all(isinstance(value, str) and re.fullmatch(r"[0-9a-f]{40}", value) for value in (plugin_base, development_target)):
        print("observation lacks immutable plugin base and development target", file=sys.stderr)
        return 2
    plugin_tree = report.get("plugin_tree")
    development_lock = report.get("development_lock")
    missing_gates = []
    if not isinstance(plugin_tree, str) or not re.fullmatch(r"[0-9a-f]{40}", plugin_tree):
        missing_gates.append("candidate plugin tree identity")
    if development_lock != development_target or report.get("lock_matches_target") is not True:
        missing_gates.append("candidate lock identity")
    if not report.get("selected_validation"):
        missing_gates.append("exact build/rebuild provenance")
    missing_gates.extend(["disposable runtime receipt", "pin ancestry and final integrated-tree validation", "explicit maintainer release approval"])
    packet = {
        "schema_version": 1,
        "status": "prepared_not_published",
        "candidate_version": args.tag,
        "release": {
            "id": release.get("id"),
            "tag": args.tag,
            "tag_commit": tag_commit,
            "target_commitish": release.get("target"),
            "published_at": release.get("published_at"),
        },
        "plugin": {"base_commit": plugin_base, "candidate_commit": report.get("candidate_commit"), "tree": plugin_tree},
        "lock": {"development_target": development_target, "development_lock": development_lock, "matches_target": report.get("lock_matches_target") is True},
        "support_decision": {"proposed_release_targets": report.get("release_targets", []), "existing_candidate": report.get("candidate_prs", [])},
        "artifact_evidence": report.get("selected_validation"),
        "test_evidence": report.get("test_evidence", []),
        "missing_gates": missing_gates,
        "required_gates": [
            "exact upstream tag/peeled commit",
            "compatible plugin candidate/tree and lock",
            "proposed support and maintenance decision",
            "exact build/rebuild provenance",
            "disposable runtime receipt",
            "pin ancestry and final integrated-tree validation",
            "explicit maintainer release approval",
        ],
        "notes": report.get("notes", []),
        "publication": "forbidden by this preparation command",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(packet, indent=2, sort_keys=True) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
