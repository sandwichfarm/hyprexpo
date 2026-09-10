import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ci_targets", ROOT / "scripts/ci-targets.py")
ci = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ci)


class TargetContractTests(unittest.TestCase):
    def test_only_known_branches_select_a_contract(self):
        self.assertEqual(ci.track_for_branch("master"), "release")
        self.assertEqual(ci.track_for_branch("hyprland-git"), "development")
        with self.assertRaises(ValueError):
            ci.track_for_branch("feature/doc-fix")

    def test_current_contract_has_exact_commits(self):
        ci.targets()

    def test_only_configured_same_repository_draft_tracker_uses_development_contract(self):
        result = ci.contract("sandwichfarm/hyprexpo", 123, "master", "sandwichfarm/hyprexpo", "hyprland-git", True)
        self.assertEqual(result, {"track": "hyprland-git", "gate": "Tracking gate", "kind": "tracking"})
        cases = (
            (124, "master", "sandwichfarm/hyprexpo", "hyprland-git", True),
            (123, "hyprland-git", "sandwichfarm/hyprexpo", "hyprland-git", True),
            (123, "master", "fork/hyprexpo", "hyprland-git", True),
            (123, "master", "sandwichfarm/hyprexpo", "hyprland-git", False),
        )
        for number, base, repository, head, draft in cases:
            self.assertEqual(ci.contract("sandwichfarm/hyprexpo", number, base, repository, head, draft)["kind"], "promotion_or_development")

    def test_reject_moving_ref_or_empty_matrix(self):
        for change in ("moving", "empty", "duplicate"):
            data = ci.targets()
            if change == "moving":
                data["development"][0]["rev"] = "main"
            elif change == "empty":
                data["release"] = []
            else:
                data["release"].append(data["release"][0])
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "targets.json"
                path.write_text(json.dumps(data))
                with self.assertRaises(ValueError):
                    ci.targets(path)

    def test_resolved_source_must_match_and_all_dependencies_must_be_hashed(self):
        rev = "a" * 40
        metadata = {"locks": {"root": "root", "nodes": {
            "root": {"inputs": {"hyprland": "hyprland_2"}},
            "hyprland_2": {"locked": {"rev": rev, "narHash": "sha256-source"}},
            "nixpkgs": {"locked": {"narHash": "sha256-dependencies"}},
        }}}
        ci.verify_lock(metadata, rev)
        with self.assertRaises(ValueError):
            ci.verify_lock(metadata, "b" * 40)
        del metadata["locks"]["nodes"]["nixpkgs"]["locked"]["narHash"]
        with self.assertRaises(ValueError):
            ci.verify_lock(metadata, rev)


if __name__ == "__main__":
    unittest.main()
