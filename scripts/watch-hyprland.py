#!/usr/bin/env python3
"""Report actionable Hyprland upstream changes without changing repository state."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SCHEMA_VERSION = 1
REPOSITORY = "sandwichfarm/hyprexpo"
UPSTREAM = "hyprwm/Hyprland"
SEMVER = re.compile(r"^v(\d+)\.(\d+)\.(\d+)$")


class ObservationError(RuntimeError):
    pass


def command(*args: str) -> str:
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True)
    if result.returncode:
        raise ObservationError(f"{' '.join(args[:3])}: {result.stderr.strip() or result.stdout.strip()}")
    return result.stdout


def json_command(*args: str) -> Any:
    try:
        return json.loads(command(*args))
    except json.JSONDecodeError as error:
        raise ObservationError(f"invalid JSON from {' '.join(args[:3])}") from error


def paginated_json(*args: str) -> list[dict[str, Any]]:
    pages = json_command(*args, "--paginate", "--slurp")
    if not isinstance(pages, list) or any(not isinstance(page, list) for page in pages):
        raise ObservationError("paginated API response is incomplete")
    return [item for page in pages for item in page]


def semantic_version(tag: str) -> tuple[int, int, int] | None:
    match = SEMVER.fullmatch(tag)
    return tuple(map(int, match.groups())) if match else None


def lock_revision(lock: dict[str, Any]) -> str | None:
    try:
        root = lock["nodes"][lock["root"]]
        node = lock["nodes"][root["inputs"]["hyprland"]]
        return node["locked"]["rev"]
    except (KeyError, TypeError):
        return None


def load_branch_contract(branch_sha: str) -> dict[str, Any]:
    targets = json.loads(command("git", "show", f"{branch_sha}:scripts/hyprland-targets.json"))
    lock = json.loads(command("git", "show", f"{branch_sha}:flake.lock"))
    development = targets.get("development", [])
    if len(development) != 1 or not isinstance(development[0].get("rev"), str):
        raise ObservationError("development target contract is invalid")
    revision = development[0]["rev"]
    locked = lock_revision(lock)
    return {
        "development_revision": revision,
        "lock_revision": locked,
        "lock_matches_target": locked == revision,
        "release_targets": targets.get("release", []),
    }


def discover_live() -> dict[str, Any]:
    command("git", "fetch", "--quiet", "origin", "master", "hyprland-git")
    master = json_command("gh", "api", f"repos/{REPOSITORY}/git/ref/heads/master")["object"]["sha"]
    development = json_command("gh", "api", f"repos/{REPOSITORY}/git/ref/heads/hyprland-git")["object"]["sha"]
    upstream = json_command("gh", "api", f"repos/{UPSTREAM}/commits/main")
    releases = paginated_json("gh", "api", f"repos/{UPSTREAM}/releases?per_page=100")
    prs = json_command("gh", "pr", "list", "-R", REPOSITORY, "--state", "open", "--json", "number,title,headRefName,baseRefName,isDraft")
    runs = json_command("gh", "run", "list", "-R", REPOSITORY, "--workflow", "upstream-tip.yml", "--limit", "10", "--json", "databaseId,status,conclusion,headSha,createdAt,event")
    return {
        "master": master,
        "development_branch": development,
        "upstream_main": upstream["sha"],
        "upstream_main_date": upstream["commit"]["committer"]["date"],
        "releases": releases,
        "open_prs": prs,
        "probe_runs": runs,
        "contract": load_branch_contract(development),
    }


def stable_releases(releases: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [release for release in releases if not release.get("draft") and not release.get("prerelease") and semantic_version(release.get("tag_name", ""))]


def release_candidates(releases: list[dict[str, Any]], supported: set[str]) -> list[dict[str, Any]]:
    supported_versions = [semantic_version(tag) for tag in supported if semantic_version(tag)]
    floor = max(supported_versions) if supported_versions else None
    candidates = []
    for release in stable_releases(releases):
        version = semantic_version(release["tag_name"])
        if release["tag_name"] not in supported and (floor is None or version > floor):
            candidates.append(release)
    return candidates


def observe(data: dict[str, Any]) -> dict[str, Any]:
    contract = data["contract"]
    target = contract["development_revision"]
    main = data["upstream_main"]
    candidate_prefix = f"chase/hyprland-{main}"
    candidates = [pr for pr in data.get("open_prs", []) if pr.get("headRefName") == candidate_prefix and pr.get("baseRefName") == "hyprland-git"]
    probe_runs = data.get("probe_runs", [])
    latest_probe = probe_runs[0] if probe_runs else None
    failures: list[str] = []
    if not contract["lock_matches_target"]:
        failures.append("development_lock_mismatch")
    if latest_probe and latest_probe.get("status") != "completed":
        failures.append("probe_incomplete")
    if latest_probe and latest_probe.get("status") == "completed" and latest_probe.get("conclusion") != "success":
        failures.append("probe_failed")
    supported = {row.get("name") for row in contract.get("release_targets", [])}
    eligible = release_candidates(data.get("releases", []), supported)
    if candidates:
        status = "candidate_active"
    elif failures:
        status = "incomplete" if "probe_incomplete" in failures else "needs_diagnosis"
    elif main == target:
        status = "up_to_date"
    else:
        status = "new_upstream_target"
    return {
        "schema_version": SCHEMA_VERSION,
        "observed_at": datetime.now(timezone.utc).isoformat(),
        "repository": REPOSITORY,
        "status": status,
        "master": data["master"],
        "development_branch": data["development_branch"],
        "plugin_base": data["development_branch"],
        "development_target": target,
        "development_lock": contract["lock_revision"],
        "upstream_main": main,
        "upstream_main_date": data.get("upstream_main_date"),
        "lock_matches_target": contract["lock_matches_target"],
        "latest_probe": latest_probe,
        "failure_stages": failures,
        "candidate_prs": candidates,
        "eligible_releases": [
            {"id": release.get("id"), "tag": release["tag_name"], "target": release.get("target_commitish"), "published_at": release.get("published_at")}
            for release in eligible
        ],
        "release_targets": contract.get("release_targets", []),
        "next_action": (
            "reuse active candidate and inspect its receipt" if candidates else
            "inspect failed probe evidence before repair" if failures else
            "prepare one development candidate" if main != target else
            "no development repair required"
        ),
    }


def markdown(report: dict[str, Any]) -> str:
    lines = [
        "# Hyprland chase observation",
        "",
        f"Status: **{report['status']}**",
        f"Development target: `{report['development_target']}`",
        f"Observed upstream main: `{report['upstream_main']}`",
        f"Plugin base: `{report['plugin_base']}`",
        f"Next action: {report['next_action']}",
        "",
    ]
    if report["failure_stages"]:
        lines.extend(["## Evidence gaps", "", *[f"- `{stage}`" for stage in report["failure_stages"]], ""])
    if report["candidate_prs"]:
        lines.extend(["## Existing candidates", "", *[f"- #{pr['number']} {pr['title']}" for pr in report["candidate_prs"]], ""])
    if report["eligible_releases"]:
        lines.extend(["## New released targets", "", *[f"- `{release['tag']}` (release ID `{release['id']}`)" for release in report["eligible_releases"]], ""])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--format", choices=("json", "markdown"), default="json")
    parser.add_argument("--fixture", type=Path, help="Read deterministic observation input instead of live services")
    args = parser.parse_args()
    try:
        data = json.loads(args.fixture.read_text()) if args.fixture else discover_live()
        report = observe(data)
    except (OSError, ObservationError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(json.dumps({"schema_version": SCHEMA_VERSION, "status": "incomplete", "error": str(error)}))
        return 2
    print(markdown(report) if args.format == "markdown" else json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
