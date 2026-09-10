#!/usr/bin/env python3
"""Report Hyprland chase work from immutable build evidence."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SCHEMA_VERSION = 2
REPOSITORY = "sandwichfarm/hyprexpo"
UPSTREAM = "hyprwm/Hyprland"
SEMVER = re.compile(r"^v(\d+)\.(\d+)\.(\d+)$")
ATTEMPT = re.compile(r"-(\d+)$")


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
        "branch_tree": command("git", "rev-parse", f"{branch_sha}^{{tree}}").strip(),
        "release_targets": targets.get("release", []),
    }


def parse_provenance(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text().splitlines():
        key, separator, value = line.partition("=")
        if separator:
            values[key] = value
    return values


def artifact_attempt(name: str) -> int | None:
    match = ATTEMPT.search(name)
    return int(match.group(1)) if match else None


def artifact_evidence(run: dict[str, Any], workflow: str) -> list[dict[str, Any]]:
    run_id = run["databaseId"]
    artifacts = json_command("gh", "api", f"repos/{REPOSITORY}/actions/runs/{run_id}/artifacts").get("artifacts", [])
    records: list[dict[str, Any]] = []
    for artifact in artifacts:
        name = artifact.get("name", "")
        if artifact.get("expired") or not (name.startswith("upstream-tip-") or name.startswith("compatibility-")):
            continue
        with tempfile.TemporaryDirectory(prefix="hyprexpo-observation-") as directory:
            try:
                result = subprocess.run(["gh", "run", "download", str(run_id), "-R", REPOSITORY, "--name", name, "--dir", directory], cwd=ROOT, text=True, capture_output=True, timeout=20)
            except subprocess.TimeoutExpired:
                records.append({"run_id": run_id, "run_url": run.get("url"), "workflow": workflow, "state": "insufficient", "reason": "artifact_download_timed_out"})
                continue
            if result.returncode:
                records.append({"run_id": run_id, "run_url": run.get("url"), "workflow": workflow, "state": "insufficient", "reason": "artifact_download_failed"})
                continue
            root = Path(directory)
            manifest = root / "evidence.json"
            provenance = root / "provenance.txt"
            lock = root / "flake.lock"
            if manifest.exists():
                values = json.loads(manifest.read_text())
                provenance_kind = "structured"
            elif provenance.exists() and lock.exists():
                values = parse_provenance(provenance)
                values["branch_lock_revision"] = lock_revision(json.loads(lock.read_text()))
                provenance_kind = "legacy"
            else:
                records.append({"run_id": run_id, "run_url": run.get("url"), "workflow": workflow, "state": "insufficient", "reason": "provenance_missing"})
                continue
            records.append({
                "run_id": run_id,
                "run_url": run.get("url"),
                "workflow": workflow,
                "run_status": run.get("status"),
                "conclusion": run.get("conclusion"),
                "workflow_head": run.get("headSha"),
                "created_at": run.get("createdAt"),
                "attempt": values.get("run_attempt", artifact_attempt(name)),
                "job": values.get("job"),
                "plugin_commit": values.get("plugin_commit"),
                "plugin_tree": values.get("plugin_tree"),
                "hyprland_commit": values.get("hyprland_commit"),
                "branch_lock_revision": values.get("branch_lock_revision"),
                "provenance": provenance_kind,
            })
    return records


def discover_live() -> dict[str, Any]:
    command("git", "fetch", "--quiet", "origin", "master", "hyprland-git")
    master = json_command("gh", "api", f"repos/{REPOSITORY}/git/ref/heads/master")["object"]["sha"]
    development = json_command("gh", "api", f"repos/{REPOSITORY}/git/ref/heads/hyprland-git")["object"]["sha"]
    upstream = json_command("gh", "api", f"repos/{UPSTREAM}/commits/main")
    releases = paginated_json("gh", "api", f"repos/{UPSTREAM}/releases?per_page=100")
    tags = paginated_json("gh", "api", f"repos/{UPSTREAM}/tags?per_page=100")
    prs = json_command("gh", "pr", "list", "-R", REPOSITORY, "--state", "open", "--json", "number,title,headRefName,baseRefName,isDraft")
    evidence: list[dict[str, Any]] = []
    for workflow in ("upstream-tip.yml", "compatibility.yml"):
        runs = json_command("gh", "run", "list", "-R", REPOSITORY, "--workflow", workflow, "--limit", "10", "--json", "databaseId,status,conclusion,headSha,createdAt,event,url")
        inspected = 0
        for run in runs:
            if run.get("status") == "completed":
                records = artifact_evidence(run, workflow)
                evidence.extend(records)
                if records:
                    inspected += 1
                    if inspected == 3:
                        break
            else:
                evidence.append({"run_id": run["databaseId"], "run_url": run.get("url"), "workflow": workflow, "run_status": run.get("status"), "conclusion": run.get("conclusion"), "state": "insufficient", "reason": "run_incomplete"})
    return {
        "master": master,
        "development_branch": development,
        "upstream_main": upstream["sha"],
        "upstream_main_date": upstream["commit"]["committer"]["date"],
        "releases": releases,
        "tags": {tag["name"]: tag["commit"]["sha"] for tag in tags if isinstance(tag.get("name"), str) and isinstance(tag.get("commit"), dict) and isinstance(tag["commit"].get("sha"), str)},
        "open_prs": prs,
        "evidence": evidence,
        "contract": load_branch_contract(development),
    }


def stable_releases(releases: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [release for release in releases if not release.get("draft") and not release.get("prerelease") and semantic_version(release.get("tag_name", ""))]


def release_candidates(releases: list[dict[str, Any]], tags: dict[str, str], supported: set[str]) -> list[dict[str, Any]]:
    supported_versions = [semantic_version(tag) for tag in supported if semantic_version(tag)]
    floor = max(supported_versions) if supported_versions else None
    candidates = []
    for release in stable_releases(releases):
        version = semantic_version(release["tag_name"])
        if release["tag_name"] not in supported and (floor is None or version > floor):
            candidate = dict(release)
            candidate["tag_commit"] = tags.get(release["tag_name"])
            candidates.append(candidate)
    return candidates


def classify_evidence(records: list[dict[str, Any]], contract: dict[str, Any], target: str) -> list[dict[str, Any]]:
    classified = []
    for record in records:
        current = all(record.get(key) == value for key, value in {
            "plugin_tree": contract["branch_tree"],
            "hyprland_commit": target,
            "branch_lock_revision": target,
        }.items())
        item = dict(record)
        if record.get("state") == "insufficient":
            item["reason"] = record["reason"]
        elif not all(record.get(key) for key in ("plugin_tree", "hyprland_commit", "branch_lock_revision")):
            item["state"] = "insufficient"
            item["reason"] = "identity_incomplete"
        elif current:
            item["state"] = "current"
            item["reason"] = "tree_target_and_lock_match"
        else:
            item["state"] = "historical"
            item["reason"] = "tree_target_or_lock_changed"
        classified.append(item)
    return classified


def observe(data: dict[str, Any]) -> dict[str, Any]:
    contract = data["contract"]
    target = contract["development_revision"]
    main = data["upstream_main"]
    candidates = [pr for pr in data.get("open_prs", []) if pr.get("headRefName") == f"chase/hyprland-{main}" and pr.get("baseRefName") == "hyprland-git"]
    evidence = classify_evidence(data.get("evidence", []), contract, target)
    current = sorted((row for row in evidence if row.get("state") == "current"), key=lambda row: (row.get("created_at") or "", row.get("run_id", 0)), reverse=True)
    selected = current[0] if current else None
    failures: list[str] = []
    if not contract["lock_matches_target"]:
        failures.append("development_lock_mismatch")
    if selected:
        if selected.get("run_status") != "completed":
            failures.append("validation_pending")
        elif selected.get("conclusion") == "cancelled":
            failures.append("validation_cancelled")
        elif selected.get("conclusion") != "success":
            failures.append("validation_failed")
    elif main == target:
        failures.append("validation_missing")
    supported = {row.get("name") for row in contract.get("release_targets", [])}
    eligible = release_candidates(data.get("releases", []), data.get("tags", {}), supported)
    if failures:
        status = "incomplete" if any(stage in failures for stage in ("validation_pending", "validation_cancelled", "validation_missing")) else "needs_diagnosis"
    elif candidates:
        status = "candidate_active"
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
        "selected_validation": selected,
        "evidence": evidence,
        "failure_stages": failures,
        "candidate_prs": candidates,
        "eligible_releases": [{"id": release.get("id"), "tag": release["tag_name"], "tag_commit": release.get("tag_commit"), "target": release.get("target_commitish"), "published_at": release.get("published_at")} for release in eligible],
        "release_targets": contract.get("release_targets", []),
        "next_action": "inspect current validation evidence" if failures else "reuse active candidate" if candidates else "prepare one development candidate" if main != target else "no development repair required",
    }


def markdown(report: dict[str, Any]) -> str:
    lines = ["# Hyprland chase observation", "", f"Status: **{report['status']}**", f"Development target: `{report['development_target']}`", f"Observed upstream main: `{report['upstream_main']}`", f"Plugin base: `{report['plugin_base']}`", f"Next action: {report['next_action']}", ""]
    if report["failure_stages"]:
        lines.extend(["## Evidence gaps", "", *[f"- `{stage}`" for stage in report["failure_stages"]], ""])
    if report["selected_validation"]:
        item = report["selected_validation"]
        lines.extend(["## Selected validation", "", f"- [{item['workflow']} run {item['run_id']}]({item['run_url']})", f"- `{item['reason']}`; attempt `{item.get('attempt')}`, job `{item.get('job') or 'legacy artifact'}`", ""])
    historical = [item for item in report["evidence"] if item.get("state") == "historical"]
    if historical:
        lines.extend(["## Historical evidence", "", *[f"- [{item.get('workflow')} run {item.get('run_id')}]({item.get('run_url')}): `{item.get('reason')}`" for item in historical], ""])
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
