import json
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
WATCHER = ROOT / "scripts/watch-hyprland.py"


def evidence(tree, target, conclusion="success", status="completed", run_id=1, lock=None, **extra):
    return {
        "run_id": run_id,
        "run_url": f"https://example.test/runs/{run_id}",
        "workflow": "compatibility.yml",
        "run_status": status,
        "conclusion": conclusion,
        "workflow_head": "h" * 40,
        "created_at": f"2026-09-10T00:00:{run_id:02}Z",
        "attempt": 1,
        "job": "build",
        "plugin_commit": "c" * 40,
        "plugin_tree": tree,
        "hyprland_commit": target,
        "branch_lock_revision": target if lock is None else lock,
        "provenance": "structured",
        **extra,
    }


def fixture(main="a" * 40, target="a" * 40, lock=None, records=None, prs=None, release_targets=None, releases=None, tags=None):
    return {
        "master": "m" * 40,
        "development_branch": "d" * 40,
        "upstream_main": main,
        "upstream_main_date": "2026-09-10T00:00:00Z",
        "contract": {
            "development_revision": target,
            "lock_revision": target if lock is None else lock,
            "lock_matches_target": lock is None or lock == target,
            "branch_tree": "t" * 40,
            "release_targets": [{"name": "v0.56.2", "rev": "r" * 40}] if release_targets is None else release_targets,
        },
        "releases": [] if releases is None else releases,
        "tags": {} if tags is None else tags,
        "open_prs": [] if prs is None else prs,
        "evidence": [] if records is None else records,
    }


def run(data, form="json"):
    with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as handle:
        json.dump(data, handle)
        path = handle.name
    result = subprocess.run(["python3", str(WATCHER), "--fixture", path, "--format", form], text=True, capture_output=True)
    Path(path).unlink()
    return result


class WatchHyprlandTests(unittest.TestCase):
    def test_validated_merged_repair_makes_old_failed_probe_historical(self):
        target = "a" * 40
        current = evidence("t" * 40, target, run_id=2)
        old_failed = evidence("o" * 40, target, conclusion="failure", run_id=1, workflow="upstream-tip.yml")
        report = json.loads(run(fixture(records=[old_failed, current])).stdout)
        self.assertEqual(report["status"], "up_to_date")
        self.assertEqual(report["selected_validation"]["run_id"], 2)
        self.assertEqual(report["evidence"][0]["state"], "historical")

    def test_current_failure_beats_earlier_success(self):
        target = "a" * 40
        old_success = evidence("t" * 40, target, run_id=1)
        current_failure = evidence("t" * 40, target, conclusion="failure", run_id=2)
        report = json.loads(run(fixture(records=[old_success, current_failure])).stdout)
        self.assertEqual(report["status"], "needs_diagnosis")
        self.assertEqual(report["failure_stages"], ["validation_failed"])

    def test_upstream_advance_needs_new_candidate_even_with_prior_validation(self):
        target = "a" * 40
        report = json.loads(run(fixture(main="b" * 40, records=[evidence("t" * 40, target)])).stdout)
        self.assertEqual(report["status"], "new_upstream_target")

    def test_plugin_tree_change_rejects_same_target_success(self):
        target = "a" * 40
        report = json.loads(run(fixture(records=[evidence("o" * 40, target)])).stdout)
        self.assertEqual(report["status"], "incomplete")
        self.assertIn("validation_missing", report["failure_stages"])
        self.assertEqual(report["evidence"][0]["state"], "historical")

    def test_workflow_head_does_not_decide_identity(self):
        target = "a" * 40
        report = json.loads(run(fixture(records=[evidence("t" * 40, target, workflow_head="m" * 40)])).stdout)
        self.assertEqual(report["status"], "up_to_date")
        self.assertEqual(report["selected_validation"]["workflow_head"], "m" * 40)

    def test_missing_pending_and_cancelled_provenance_stay_explicit(self):
        target = "a" * 40
        missing = {"run_id": 1, "run_url": "https://example.test/runs/1", "workflow": "upstream-tip.yml", "state": "insufficient", "reason": "provenance_missing"}
        pending = evidence("t" * 40, target, status="in_progress", conclusion=None, run_id=2)
        report = json.loads(run(fixture(records=[missing, pending])).stdout)
        self.assertEqual(report["status"], "incomplete")
        self.assertIn("validation_pending", report["failure_stages"])
        self.assertEqual(report["evidence"][0]["state"], "insufficient")

    def test_cancelled_current_validation_is_incomplete(self):
        target = "a" * 40
        cancelled = evidence("t" * 40, target, status="completed", conclusion="cancelled")
        report = json.loads(run(fixture(records=[cancelled])).stdout)
        self.assertEqual(report["status"], "incomplete")
        self.assertEqual(report["failure_stages"], ["validation_cancelled"])

    def test_lock_mismatch_never_uses_matching_build(self):
        target = "a" * 40
        report = json.loads(run(fixture(lock="b" * 40, records=[evidence("t" * 40, target)])).stdout)
        self.assertEqual(report["status"], "needs_diagnosis")
        self.assertIn("development_lock_mismatch", report["failure_stages"])

    def test_candidate_detection_and_markdown_evidence(self):
        target = "a" * 40
        main = "b" * 40
        candidate = {"number": 7, "title": "chase", "headRefName": f"chase/hyprland-{main}", "baseRefName": "hyprland-git", "isDraft": True}
        result = run(fixture(main=main, records=[evidence("t" * 40, target)], prs=[candidate]), "markdown")
        self.assertEqual(result.returncode, 0)
        self.assertIn("candidate_active", result.stdout)
        self.assertIn("Selected validation", result.stdout)
        self.assertIn("run 1", result.stdout)

    def test_new_patch_in_maintained_older_line_is_eligible(self):
        release = {"id": 9, "tag_name": "v0.55.3", "target_commitish": "c" * 40}
        report = json.loads(run(fixture(
            release_targets=[{"name": "v0.55.2", "rev": "r" * 40}, {"name": "v0.56.2", "rev": "s" * 40}],
            releases=[release],
            tags={"v0.55.3": "c" * 40},
        )).stdout)
        self.assertEqual([item["tag"] for item in report["eligible_releases"]], ["v0.55.3"])
        self.assertEqual(report["release_reviews"], [])

    def test_tag_only_malformed_and_retargeted_releases_are_review_items(self):
        releases = [
            {"id": 1, "tag_name": "nightly", "target_commitish": "main"},
            {"id": 2, "tag_name": "v0.57.0", "target_commitish": "d" * 40},
        ]
        report = json.loads(run(fixture(
            releases=releases,
            tags={"nightly": "n" * 40, "v0.57.0": "c" * 40, "v0.57.1": "e" * 40},
        )).stdout)
        reviews = {(item.get("tag"), item["reason"]) for item in report["release_reviews"]}
        self.assertIn(("nightly", "unsupported_release_tag"), reviews)
        self.assertIn(("v0.57.0", "tag_identity_mismatch"), reviews)
        self.assertIn(("v0.57.1", "tag_without_release"), reviews)
        self.assertEqual(report["eligible_releases"], [])


if __name__ == "__main__":
    unittest.main()
