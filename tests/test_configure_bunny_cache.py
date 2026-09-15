from pathlib import Path
import os
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/configure-bunny-cache.sh"


class ConfigureBunnyCacheTests(unittest.TestCase):
    def run_script(self, values, *args):
        with tempfile.TemporaryDirectory() as directory:
            bin_dir = Path(directory) / "bin"
            bin_dir.mkdir()
            log = Path(directory) / "gh.log"
            fake_gh = bin_dir / "gh"
            fake_gh.write_text("""#!/usr/bin/env bash
set -eu
printf '%s|' "$*" >> "$GH_LOG"
cat | wc -c | tr -d '[:space:]' >> "$GH_LOG"
printf '\\n' >> "$GH_LOG"
""")
            fake_gh.chmod(0o755)
            result = subprocess.run(
                [str(SCRIPT), *args],
                input="\n".join(values) + "\n",
                text=True,
                capture_output=True,
                env={**os.environ, "PATH": f"{bin_dir}:{os.environ['PATH']}", "GH_LOG": str(log)},
            )
            return result, log.read_text() if log.exists() else ""

    def test_sets_two_variables_and_three_secrets_without_echoing_them(self):
        values = [
            "https://cache.example.net/hyprland-nix",
            "cache:YWJjZA==",
            "cache-zone",
            "storage-password",
            "cache:ZGVmZw==",
        ]
        result, calls = self.run_script(values, "--repo", "owner/repository")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Configured Bunny cache variables and secrets for owner/repository.", result.stdout)
        self.assertNotIn("storage-password", result.stdout + result.stderr)
        self.assertNotIn("ZGVmZw", result.stdout + result.stderr)
        self.assertEqual(
            [call.split("|")[0] for call in calls.splitlines()],
            [
                "auth status --hostname github.com",
                "variable set BUNNY_CACHE_URL --repo owner/repository",
                "variable set BUNNY_CACHE_PUBLIC_KEY --repo owner/repository",
                "secret set BUNNY_CACHE_STORAGE_ZONE --repo owner/repository",
                "secret set BUNNY_CACHE_STORAGE_PASSWORD --repo owner/repository",
                "secret set BUNNY_CACHE_SIGNING_KEY --repo owner/repository",
            ],
        )
        self.assertEqual([call.rsplit("|", 1)[1] for call in calls.splitlines()[1:]], [
            str(len(value)) for value in values
        ])

    def test_invalid_input_does_not_call_gh(self):
        result, calls = self.run_script([
            "http://cache.example.net/cache", "cache:YWJjZA==", "cache-zone", "password", "cache:ZGVmZw=="
        ])
        self.assertEqual(result.returncode, 2)
        self.assertIn("BUNNY_CACHE_URL must be an HTTPS", result.stderr)
        self.assertEqual(calls, "")

    def test_bad_repository_is_rejected_before_prompts(self):
        result, calls = self.run_script([], "--repo", "owner/repository/extra")
        self.assertEqual(result.returncode, 2)
        self.assertIn("Repository must be OWNER/REPO", result.stderr)
        self.assertEqual(calls, "")


if __name__ == "__main__":
    unittest.main()
