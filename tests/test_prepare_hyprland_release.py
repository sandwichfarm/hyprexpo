import json
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
PREPARE = ROOT / "scripts/prepare-hyprland-release.py"


class ReleasePacketTests(unittest.TestCase):
    def observation(self, releases):
        return {"plugin_base": "p" * 40, "development_target": "d" * 40, "release_targets": [{"name": "v0.56.2"}], "eligible_releases": releases}

    def test_packet_is_local_and_non_publishing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "observation.json"
            output = root / "packet.json"
            source.write_text(json.dumps(self.observation([{"id": 2, "tag": "v0.57.0", "target": "t" * 40}])))
            result = subprocess.run(["python3", str(PREPARE), str(source), "--tag", "v0.57.0", "--output", str(output)], text=True, capture_output=True)
            self.assertEqual(result.returncode, 0)
            packet = json.loads(output.read_text())
            self.assertEqual(packet["status"], "prepared_not_published")
            self.assertEqual(packet["publication"], "forbidden by this preparation command")
            self.assertIn("pin ancestry and final integrated-tree validation", packet["required_gates"])

    def test_unknown_or_ambiguous_release_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "observation.json"
            source.write_text(json.dumps(self.observation([{"tag": "v0.57.0"}, {"tag": "v0.57.0"}])))
            result = subprocess.run(["python3", str(PREPARE), str(source), "--tag", "v0.57.0", "--output", str(root / "packet.json")], text=True, capture_output=True)
            self.assertEqual(result.returncode, 2)
            self.assertFalse((root / "packet.json").exists())


if __name__ == "__main__":
    unittest.main()
