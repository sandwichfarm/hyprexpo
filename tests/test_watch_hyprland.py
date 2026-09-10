import json
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
WATCHER = ROOT / "scripts/watch-hyprland.py"


def fixture(main="b" * 40, target="a" * 40, lock=None, probe="success", releases=None, tags=None, prs=None):
    return {
        "master": "m" * 40,
        "development_branch": "d" * 40,
        "upstream_main": main,
        "upstream_main_date": "2026-09-10T00:00:00Z",
        "contract": {
            "development_revision": target,
            "lock_revision": target if lock is None else lock,
            "lock_matches_target": lock is None or lock == target,
            "release_targets": [{"name": "v0.56.2", "rev": "r" * 40}],
        },
        "releases": [] if releases is None else releases,
        "tags": {} if tags is None else tags,
        "open_prs": [] if prs is None else prs,
        "probe_runs": [{"databaseId": 1, "status": "completed", "conclusion": probe, "headSha": "m" * 40, "createdAt": "2026-09-10T00:00:00Z"}],
    }


def run(data, form="json"):
    with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as handle:
        json.dump(data, handle)
        path = handle.name
    result = subprocess.run(["python3", str(WATCHER), "--fixture", path, "--format", form], text=True, capture_output=True)
    Path(path).unlink()
    return result


class WatchHyprlandTests(unittest.TestCase):
    def test_up_to_date_has_no_candidate_work(self):
        result = run(fixture(main="a" * 40))
        report = json.loads(result.stdout)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(report["status"], "up_to_date")
        self.assertEqual(report["eligible_releases"], [])

    def test_new_target_requests_one_candidate(self):
        result = run(fixture())
        report = json.loads(result.stdout)
        self.assertEqual(report["status"], "new_upstream_target")
        self.assertEqual(report["next_action"], "prepare one development candidate")

    def test_existing_exact_candidate_deduplicates(self):
        main = "b" * 40
        result = run(fixture(prs=[{"number": 7, "title": "chase", "headRefName": f"chase/hyprland-{main}", "baseRefName": "hyprland-git", "isDraft": True}]))
        report = json.loads(result.stdout)
        self.assertEqual(report["status"], "candidate_active")
        self.assertEqual(report["candidate_prs"][0]["number"], 7)

    def test_existing_candidate_wins_over_prior_failed_probe(self):
        main = "b" * 40
        report = json.loads(run(fixture(probe="failure", prs=[{"number": 7, "title": "chase", "headRefName": f"chase/hyprland-{main}", "baseRefName": "hyprland-git", "isDraft": True}])).stdout)
        self.assertEqual(report["status"], "candidate_active")
        self.assertIn("probe_failed", report["failure_stages"])

    def test_failed_probe_requires_diagnosis(self):
        result = run(fixture(probe="failure"))
        report = json.loads(result.stdout)
        self.assertEqual(report["status"], "needs_diagnosis")
        self.assertIn("probe_failed", report["failure_stages"])

    def test_lock_mismatch_is_never_compatible(self):
        result = run(fixture(lock="c" * 40))
        report = json.loads(result.stdout)
        self.assertEqual(report["status"], "needs_diagnosis")
        self.assertIn("development_lock_mismatch", report["failure_stages"])

    def test_new_stable_release_is_reported_but_drafts_are_ignored(self):
        releases = [
            {"id": 1, "tag_name": "v0.56.2", "draft": False, "prerelease": False},
            {"id": 2, "tag_name": "v0.57.0", "draft": False, "prerelease": False, "target_commitish": "z" * 40},
            {"id": 3, "tag_name": "v0.58.0", "draft": True, "prerelease": False},
            {"id": 4, "tag_name": "v0.59.0-rc.1", "draft": False, "prerelease": True},
        ]
        report = json.loads(run(fixture(releases=releases, tags={"v0.57.0": "p" * 40})).stdout)
        self.assertEqual(report["eligible_releases"], [{"id": 2, "tag": "v0.57.0", "tag_commit": "p" * 40, "target": "z" * 40, "published_at": None}])

    def test_older_unsupported_releases_are_not_new_work(self):
        releases = [{"id": 1, "tag_name": "v0.55.4", "draft": False, "prerelease": False}]
        report = json.loads(run(fixture(main="a" * 40, releases=releases)).stdout)
        self.assertEqual(report["eligible_releases"], [])

    def test_markdown_exposes_failure_and_release_work(self):
        result = run(fixture(probe="failure", releases=[{"id": 2, "tag_name": "v0.57.0", "draft": False, "prerelease": False}]), "markdown")
        self.assertEqual(result.returncode, 0)
        self.assertIn("probe_failed", result.stdout)
        self.assertIn("v0.57.0", result.stdout)


if __name__ == "__main__":
    unittest.main()
