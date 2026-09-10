#!/usr/bin/env python3
"""Exercise the actual release recipes against disposable, offline Git repos."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ReleaseGuardsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="hyprexpo-release-test-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.repo = self.directory / "repo"
        self.remote = self.directory / "remote.git"
        self.repo.mkdir()
        self.env = {
            key: value for key, value in os.environ.items()
            if not key.startswith("GIT_")
            and key not in {"MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES", "MAKEFILES"}
        }
        self.env.update({
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": os.devnull,
            "GIT_TERMINAL_PROMPT": "0",
            "GIT_ALLOW_PROTOCOL": "file",
            "LC_ALL": "C",
        })
        self.git("init", "--initial-branch=master")
        self.git("config", "user.name", "Release fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        self.git("config", "tag.gpgsign", "false")
        for relative in ("Makefile", "scripts/version.sh", "scripts/check-commit-pins.sh"):
            destination = self.repo / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, destination)
        (self.repo / "VERSION").write_text("v1.2.3\n")
        self.git("add", ".")
        self.git("commit", "-m", "Provide the initial release fixture")
        self.pin = self.git("rev-parse", "HEAD").stdout.strip()
        self.set_pin(self.pin)
        self.git("init", "--bare", "--initial-branch=master", str(self.remote))
        self.git("remote", "add", "origin", str(self.remote))
        self.git("push", "-u", "origin", "master")

    def run_command(self, *args, check=True):
        result = subprocess.run(
            args, cwd=self.repo, env=self.env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20,
        )
        if check:
            self.assertEqual(result.returncode, 0, result.stdout)
        return result

    def git(self, *args, **kwargs):
        return self.run_command("git", *args, **kwargs)

    def make(self, *args):
        return self.run_command("make", "--no-print-directory", *args, check=False)

    def set_pin(self, pin):
        (self.repo / "hyprpm.toml").write_text(
            '[repository]\nname = "fixture"\ncommit_pins = [\n'
            f'    ["{"a" * 40}", "{pin}"],\n]\n'
        )
        self.git("add", "hyprpm.toml")
        self.git("commit", "-m", "Select the fixture plugin revision")

    def stage_unrelated_change(self):
        (self.repo / "unrelated.txt").write_text("Keep this staged change.\n")
        self.git("add", "unrelated.txt")

    def snapshot(self):
        return {
            "version": (self.repo / "VERSION").read_bytes(),
            "head": self.git("rev-parse", "HEAD").stdout,
            "index": self.git("write-tree").stdout,
            "tags": self.git("for-each-ref", "refs/tags").stdout,
            "remote_refs": self.git("--git-dir", str(self.remote), "for-each-ref").stdout,
        }

    def assert_rejected_without_changes(self, *args):
        self.stage_unrelated_change()
        before = self.snapshot()
        result = self.make(*args)
        # Check side effects even if a later echo incorrectly hides the error.
        self.assertEqual(self.snapshot(), before, result.stdout)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("hyprpm plugin pins must be fetchable", result.stdout)
        self.assertNotIn("next: make", result.stdout)
        self.assertNotIn("pushed branch + tag", result.stdout)

    def make_unlanded_pin(self):
        # The object exists locally, but this unmerged PR commit is not in HEAD.
        tree = self.git("rev-parse", "HEAD^{tree}").stdout.strip()
        unlanded = self.git("commit-tree", tree, "-p", "HEAD", "-m", "Unmerged PR").stdout.strip()
        self.set_pin(unlanded)

    def test_version_rejects_unlanded_pin_before_mutation(self):
        self.make_unlanded_pin()
        self.assert_rejected_without_changes("version", "v1.2.3+1")

    def test_tag_rejects_unlanded_pin_before_mutation(self):
        self.make_unlanded_pin()
        self.assert_rejected_without_changes("tag")

    def test_publish_rejects_unlanded_pin_before_either_push(self):
        self.make_unlanded_pin()
        self.git("tag", "-a", "v1.2.3", "-m", "Invalid release fixture")
        self.assert_rejected_without_changes("publish")

    def test_version_rejects_unavailable_pin_before_mutation(self):
        self.set_pin("0" * 40)
        self.assert_rejected_without_changes("version", "v=v1.2.3+1")

    def test_tag_rejects_unavailable_pin_before_mutation(self):
        self.set_pin("0" * 40)
        self.assert_rejected_without_changes("tag")

    def test_publish_rejects_unavailable_pin_before_either_push(self):
        self.set_pin("0" * 40)
        self.git("tag", "-a", "v1.2.3", "-m", "Invalid release fixture")
        self.assert_rejected_without_changes("publish")

    def test_valid_version_tag_publish_flow(self):
        self.stage_unrelated_change()
        old_head = self.git("rev-parse", "HEAD").stdout.strip()
        result = self.make("version", "v1.2.3+1")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual((self.repo / "VERSION").read_text(), "v1.2.3+1\n")
        self.assertEqual(self.git("rev-parse", "HEAD^").stdout.strip(), old_head)
        self.assertEqual(self.git("diff", "--cached", "--name-only").stdout, "unrelated.txt\n")
        self.assertEqual(self.git("diff", "HEAD^", "HEAD", "--name-only").stdout, "VERSION\n")
        result = self.make("tag")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(self.git("cat-file", "-t", "v1.2.3+1").stdout, "tag\n")
        self.assertEqual(self.git("rev-parse", "v1.2.3+1^{commit}").stdout,
                         self.git("rev-parse", "HEAD").stdout)
        result = self.make("publish")
        self.assertEqual(result.returncode, 0, result.stdout)
        for ref in ("refs/heads/master", "refs/tags/v1.2.3+1"):
            self.assertEqual(self.git("--git-dir", str(self.remote), "rev-parse", ref).stdout,
                             self.git("rev-parse", ref).stdout)

    def test_rejected_branch_push_does_not_push_tag(self):
        result = self.make("version", "v1.2.3+1")
        self.assertEqual(result.returncode, 0, result.stdout)
        result = self.make("tag")
        self.assertEqual(result.returncode, 0, result.stdout)
        hook = self.remote / "hooks" / "update"
        hook.write_text('#!/bin/sh\ncase "$1" in refs/heads/*) exit 1;; esac\n')
        hook.chmod(0o755)
        before = self.snapshot()
        result = self.make("publish")
        self.assertEqual(self.snapshot(), before, result.stdout)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("hook declined", result.stdout)
        self.assertNotIn("pushed branch + tag", result.stdout)
        # Prove this remote would accept the tag if the recipe tried its second push.
        self.git("push", "origin", "v1.2.3+1")
        self.assertEqual(self.git("--git-dir", str(self.remote), "rev-parse", "refs/tags/v1.2.3+1").stdout,
                         self.git("rev-parse", "refs/tags/v1.2.3+1").stdout)


if __name__ == "__main__":
    unittest.main()
