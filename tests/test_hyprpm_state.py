#!/usr/bin/env python3
"""Exercise installed-state validation and install boundaries using real Git repos."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
GUARD = ROOT / "scripts/check-hyprpm-state.py"


class HyprpmStateTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="hyprexpo-state-test-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.repo = self.directory / "caller"
        self.remote = self.directory / "remote.git"
        self.install_dir = self.directory / "installed"
        self.repo.mkdir()
        self.install_dir.mkdir()
        self.state = self.install_dir / "state.toml"
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
            "XDG_CACHE_HOME": str(self.directory / "cache"),
            "XDG_DATA_HOME": str(self.directory / "data"),
        })
        self.git("init", "--initial-branch=master")
        self.git("config", "user.name", "State fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        self.git("config", "tag.gpgsign", "false")
        (self.repo / "payload").write_text("Fixture tree.\n")
        self.git("add", ".")
        self.git("commit", "-m", "Provide the retained master history")
        self.master = self.git("rev-parse", "HEAD").stdout.strip()
        self.tree = self.git("rev-parse", "HEAD^{tree}").stdout.strip()
        self.git("init", "--bare", "--initial-branch=master", str(self.remote))
        self.git("remote", "add", "origin", str(self.remote))
        self.git("push", "-u", "origin", "master")

    def command(self, *args, check=True, env=None):
        result = subprocess.run(
            args, cwd=self.repo, env=env or self.env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20,
        )
        if check:
            self.assertEqual(result.returncode, 0, result.stdout)
        return result

    def git(self, *args, **kwargs):
        return self.command("git", *args, **kwargs)

    def commit(self, message, parent=None):
        args = ["commit-tree", self.tree, "-m", message]
        if parent:
            args.extend(["-p", parent])
        return self.git(*args).stdout.strip()

    def publish_branch(self, name, commit):
        self.git("push", "origin", f"{commit}:refs/heads/{name}")

    def set_revision(self, revision, url=None):
        self.state.write_text(
            "[repository]\n"
            f"url = {json.dumps(str(self.remote) if url is None else url)}\n"
            f"rev = {json.dumps(revision)}\n"
        )

    def snapshot(self, path):
        if not path.exists() and not path.is_symlink():
            return None
        info = path.lstat()
        contents = os.readlink(path) if path.is_symlink() else path.read_bytes()
        return (info.st_ino, info.st_mode, info.st_size, info.st_mtime_ns, contents)

    def guard(self, expected, env=None):
        before = self.snapshot(self.state)
        result = self.command(sys.executable, str(GUARD), str(self.state), check=False, env=env)
        self.assertEqual(self.snapshot(self.state), before, result.stdout)
        self.assertEqual(result.returncode, expected, result.stdout)
        return result

    def test_absent_state_passes_without_git(self):
        self.guard(0, env={**self.env, "PATH": str(self.directory / "no-programs")})

    def test_unpinned_state_passes_without_git_or_remote(self):
        states = (
            "[repository]\n",
            '[repository]\nurl = "https://unreachable.invalid/repo.git"\n',
            '[repository]\nrev = ""\nurl = "https://unreachable.invalid/repo.git"\n',
        )
        for contents in states:
            with self.subTest(contents=contents):
                self.state.write_text(contents)
                self.guard(0, env={**self.env, "PATH": str(self.directory / "no-programs")})

    def test_malformed_toml_and_invalid_utf8_are_rejected(self):
        for contents in (b"[repository\n", b"[repository]\nrev = \xff\n"):
            with self.subTest(contents=contents):
                self.state.write_bytes(contents)
                self.guard(1)

    def test_missing_or_invalid_repository_table_is_rejected(self):
        for contents in ("", "enabled = true\n", 'repository = "invalid"\n'):
            with self.subTest(contents=contents):
                self.state.write_text(contents)
                self.guard(1)

    def test_non_string_revisions_are_rejected(self):
        for value in ("123", "true", "[]", "{}"):
            with self.subTest(value=value):
                self.state.write_text(f"[repository]\nrev = {value}\n")
                self.guard(1)

    @unittest.skipIf(os.geteuid() == 0, "Root can read permission-denied fixtures")
    def test_unreadable_state_is_rejected_without_changing_permissions(self):
        self.set_revision("master")
        self.state.chmod(0)
        before = self.state.stat()
        try:
            result = self.command(sys.executable, str(GUARD), str(self.state), check=False)
            after = self.state.stat()
            self.assertEqual(result.returncode, 1, result.stdout)
            self.assertEqual((after.st_ino, after.st_mode, after.st_mtime_ns),
                             (before.st_ino, before.st_mode, before.st_mtime_ns))
        finally:
            self.state.chmod(0o600)

    def test_state_directory_is_rejected(self):
        self.state.mkdir()
        result = self.command(sys.executable, str(GUARD), str(self.state), check=False)
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertTrue(self.state.is_dir())

    def test_missing_or_invalid_repository_url_is_rejected(self):
        for url_line in ("", 'url = ""\n', "url = 42\n"):
            with self.subTest(url_line=url_line):
                self.state.write_text('[repository]\nrev = "master"\n' + url_line)
                self.guard(1)

    def test_unavailable_remote_fails_closed(self):
        self.set_revision("master", str(self.directory / "missing.git"))
        self.guard(1)

    def test_transport_failure_fails_closed(self):
        # The transport allowlist rejects HTTPS immediately without using the network.
        self.set_revision(self.master, "https://unreachable.invalid/repo.git")
        self.guard(1)

    def test_master_names_resolve_from_fresh_remote(self):
        for revision in ("master", "origin/master", "refs/heads/master",
                         "refs/remotes/origin/master"):
            with self.subTest(revision=revision):
                self.set_revision(revision)
                self.guard(0)

    def test_retained_development_and_maintenance_names(self):
        for branch in ("hyprland-git", "release/0.56"):
            retained = self.commit(f"Independent {branch} history")
            self.publish_branch(branch, retained)
            for revision in (f"origin/{branch}", f"refs/remotes/origin/{branch}"):
                with self.subTest(revision=revision):
                    self.set_revision(revision)
                    self.guard(0)

    def test_commit_hashes_in_each_retained_history(self):
        for branch in ("master", "hyprland-git", "release/0.56"):
            ancestor = self.master if branch == "master" else self.commit(f"Base for {branch}")
            tip = self.commit(f"Advance {branch}", parent=ancestor)
            self.publish_branch(branch, tip)
            for revision in (ancestor, tip):
                with self.subTest(branch=branch, revision=revision):
                    self.set_revision(revision)
                    self.guard(0)

    def test_lightweight_and_annotated_version_tags_retain_history(self):
        for name, annotated in (("v1.2.3", False), ("v1.2.3+4", True)):
            ancestor = self.commit(f"Independent history for {name}")
            tip = self.commit(f"Release {name}", parent=ancestor)
            args = ["tag", name, tip]
            if annotated:
                args.extend(["-a", "-m", f"Release {name}"])
            self.git(*args)
            self.git("push", "origin", f"refs/tags/{name}")
            for revision in (name, f"refs/tags/{name}", ancestor, tip):
                with self.subTest(revision=revision):
                    self.set_revision(revision)
                    self.guard(0)

    def test_temporary_names_are_rejected_even_when_pointing_to_master(self):
        for branch in ("temporary-fix", "release/temporary"):
            self.publish_branch(branch, self.master)
            for revision in (branch, f"origin/{branch}", f"refs/heads/{branch}",
                             f"refs/remotes/origin/{branch}"):
                with self.subTest(revision=revision):
                    self.set_revision(revision)
                    self.guard(1)

    def test_arbitrary_revision_expressions_are_rejected(self):
        for revision in ("HEAD", "master~0", "master^{commit}", "master@{0}",
                         self.master[:12], "--all", " ", "refs/remotes/origin/HEAD"):
            with self.subTest(revision=revision):
                self.set_revision(revision)
                self.guard(1)

    def test_nonversion_tags_do_not_authorize_temporary_history(self):
        temporary = self.commit("Only a development tag retains this commit")
        self.git("tag", "snapshot", temporary)
        self.git("push", "origin", "refs/tags/snapshot")
        for revision in ("snapshot", "refs/tags/snapshot", temporary):
            with self.subTest(revision=revision):
                self.set_revision(revision)
                self.guard(1)

    def test_temporary_branch_only_hash_is_rejected_even_when_fetchable(self):
        temporary = self.commit("Unmerged temporary change", parent=self.master)
        self.publish_branch("temporary-fix", temporary)
        probe = self.directory / "probe.git"
        self.git("clone", "--bare", "--no-local", str(self.remote), str(probe))
        self.git("--git-dir", str(probe), "cat-file", "-e", temporary)
        self.set_revision(temporary)
        self.guard(1)

    def test_deleted_pr_hash_is_rejected_despite_local_remote_objects(self):
        deleted = self.commit("Deleted PR change", parent=self.master)
        self.publish_branch("deleted-pr", deleted)
        self.git("push", "origin", "--delete", "deleted-pr")
        self.git("cat-file", "-e", deleted)
        self.git("--git-dir", str(self.remote), "cat-file", "-e", deleted)
        probe = self.directory / "probe.git"
        self.git("clone", "--bare", "--no-local", str(self.remote), str(probe))
        result = self.git("--git-dir", str(probe), "cat-file", "-e", deleted, check=False)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.set_revision(deleted)
        self.guard(1)

    def test_unavailable_hash_is_rejected(self):
        self.set_revision("0" * 40)
        self.guard(1)

    def test_caller_refs_do_not_authorize_or_override_remote_revisions(self):
        local_only = self.commit("Caller has its own retained-looking refs")
        self.git("update-ref", "refs/remotes/origin/master", local_only)
        self.git("tag", "v9.9.9", local_only)
        for revision in (local_only, "v9.9.9"):
            with self.subTest(revision=revision):
                self.set_revision(revision)
                self.guard(1)
        self.set_revision("origin/master")
        self.guard(0)

    def test_clone_templates_cannot_inject_local_only_release_history(self):
        local_only = self.commit("Unpublished orphan injected by a Git template")
        missing = self.git("--git-dir", str(self.remote), "cat-file", "-e", local_only, check=False)
        self.assertNotEqual(missing.returncode, 0, missing.stdout)
        template = self.directory / "template"
        (template / "refs/tags").mkdir(parents=True)
        (template / "refs/tags/v9.9.9").write_text(local_only + "\n")
        (template / "objects/info").mkdir(parents=True)
        (template / "objects/info/alternates").write_text(str(self.repo / ".git/objects") + "\n")
        config = self.directory / "template.gitconfig"
        config.write_text(f"[init]\n\ttemplateDir = {template}\n")
        environments = {
            "environment": {**self.env, "GIT_TEMPLATE_DIR": str(template)},
            "configuration": {**self.env, "GIT_CONFIG_GLOBAL": str(config)},
        }
        self.set_revision(local_only)
        for source, environment in environments.items():
            with self.subTest(source=source):
                # Prove an ordinary fresh clone inherits the forged tag and objects.
                probe = self.directory / f"template-probe-{source}"
                self.git("clone", "--no-local", "--no-checkout", str(self.remote), str(probe),
                         env=environment)
                injected = self.git("-C", str(probe), "rev-parse", "v9.9.9^{commit}")
                self.assertEqual(injected.stdout.strip(), local_only)
                self.guard(1, env=environment)

    def prepare_install(self):
        for relative in ("Makefile", "scripts/version.sh", "scripts/check-hyprpm-state.py",
                         "scripts/dev-link.sh"):
            destination = self.repo / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, destination)
        (self.repo / "VERSION").write_text("v1.2.3\n")
        self.new_binary = self.directory / "new-hyprexpo.so"
        self.new_binary.write_bytes(b"new plugin fixture\n")
        self.old_binary = self.install_dir / "hyprexpo.so"
        self.old_binary.write_bytes(b"installed plugin fixture\n")
        self.backup = self.install_dir / "hyprexpo.so.bak"
        self.backup.write_bytes(b"existing backup fixture\n")
        self.env["HYPREXPO_DEV_SO"] = str(self.new_binary)

    def make(self, goal):
        return self.command(
            "make", "--no-print-directory", "-j4", goal,
            f"TARGET={self.new_binary}", f"INSTALL_DIR={self.install_dir}",
            "INSTALL_USER=fixture", "SRC=", "HEADERS=", "CXX=false", "INCLUDES=", "LIBS=",
            check=False,
        )

    def install_snapshot(self):
        return {str(path): self.snapshot(path) for path in
                (self.state, self.new_binary, self.old_binary, self.backup)}

    def test_make_check_target_and_install_reject_without_mutation(self):
        self.prepare_install()
        self.set_revision("0" * 40)
        for goal in ("check-hyprpm-state", "install"):
            with self.subTest(goal=goal):
                before = self.install_snapshot()
                result = self.make(goal)
                self.assertEqual(self.install_snapshot(), before, result.stdout)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertNotIn("No rule to make target", result.stdout)

    def test_make_install_accepts_retained_revision(self):
        self.prepare_install()
        self.set_revision(self.master)
        state_before = self.snapshot(self.state)
        result = self.make("install")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(self.old_binary.read_bytes(), self.new_binary.read_bytes())
        self.assertEqual(self.snapshot(self.state), state_before)

    def test_make_install_accepts_absent_and_unpinned_state(self):
        self.prepare_install()
        for contents in (None, '[repository]\nrev = ""\nurl = "missing"\n'):
            with self.subTest(contents=contents):
                if contents is not None:
                    self.state.write_text(contents)
                before = self.snapshot(self.state)
                result = self.make("install")
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertEqual(self.old_binary.read_bytes(), self.new_binary.read_bytes())
                self.assertEqual(self.snapshot(self.state), before)

    @unittest.skipIf(os.geteuid() == 0, "dev-link intentionally rejects root")
    def test_dev_link_rejects_before_replacing_explicit_symlink_or_backup(self):
        self.prepare_install()
        previous_binary = self.directory / "previous.so"
        previous_binary.write_bytes(b"previous symlink destination\n")
        self.old_binary.unlink()
        self.old_binary.symlink_to(previous_binary)
        self.set_revision("0" * 40)
        before = self.install_snapshot()
        previous_before = self.snapshot(previous_binary)
        result = self.command("bash", "scripts/dev-link.sh", "-t", str(self.old_binary), check=False)
        self.assertEqual(self.install_snapshot(), before, result.stdout)
        self.assertEqual(self.snapshot(previous_binary), previous_before, result.stdout)
        self.assertNotEqual(result.returncode, 0, result.stdout)

    @unittest.skipIf(os.geteuid() == 0, "dev-link intentionally rejects root")
    def test_dev_link_accepts_retained_revision_and_preserves_state(self):
        self.prepare_install()
        self.set_revision("origin/master")
        original = self.old_binary.read_bytes()
        state_before = self.snapshot(self.state)
        result = self.command("bash", "scripts/dev-link.sh", "-t", str(self.old_binary), check=False)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertTrue(self.old_binary.is_symlink())
        self.assertEqual(self.old_binary.resolve(), self.new_binary)
        self.assertEqual(self.backup.read_bytes(), original)
        self.assertEqual(self.snapshot(self.state), state_before)

    @unittest.skipIf(os.geteuid() == 0, "dev-link intentionally rejects root")
    def test_dev_link_restore_remains_available_for_rejected_state(self):
        self.prepare_install()
        self.old_binary.unlink()
        self.old_binary.symlink_to(self.new_binary)
        self.new_binary.unlink()
        self.set_revision("0" * 40)
        state_before = self.snapshot(self.state)
        backup = self.backup.read_bytes()
        result = self.command("bash", "scripts/dev-link.sh", "--restore", "-t",
                              str(self.old_binary), check=False)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertFalse(self.old_binary.is_symlink())
        self.assertEqual(self.old_binary.read_bytes(), backup)
        self.assertFalse(self.backup.exists())
        self.assertEqual(self.snapshot(self.state), state_before)

    def prepare_manager(self):
        executable_dir = self.directory / "bin"
        executable_dir.mkdir()
        executable = executable_dir / "hyprpm"
        executable.write_text(
            '#!/bin/sh\n'
            'printf "%s\\0" "$@" > "$HYPREXPO_MANAGER_LOG"\n'
            'exit "${HYPREXPO_MANAGER_STATUS:-0}"\n'
        )
        executable.chmod(0o755)
        self.manager_log = self.directory / "manager-arguments"
        self.env["HYPREXPO_MANAGER_LOG"] = str(self.manager_log)
        self.env["PATH"] = str(executable_dir) + os.pathsep + self.env["PATH"]
        self.set_revision("master")

    def add_repository(self, *args):
        before = self.snapshot(self.state)
        result = self.command("bash", str(ROOT / "scripts/hyprpm-add.sh"), *args, check=False)
        self.assertEqual(self.snapshot(self.state), before, result.stdout)
        return result

    def test_add_wrapper_rejects_temporary_branches_and_hashes_before_manager(self):
        self.prepare_manager()
        temporary = self.commit("Unmerged candidate still exists upstream", parent=self.master)
        self.publish_branch("candidate-pr", temporary)
        for revision in ("candidate-pr", "origin/candidate-pr", temporary):
            with self.subTest(revision=revision):
                result = self.add_repository(str(self.remote), revision)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertFalse(self.manager_log.exists(), result.stdout)
        self.git("push", "origin", "--delete", "candidate-pr")
        for revision in (temporary, "0" * 40):
            with self.subTest(deleted_or_unavailable=revision):
                result = self.add_repository(str(self.remote), revision)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertFalse(self.manager_log.exists(), result.stdout)

    def test_add_wrapper_rejects_temporary_name_even_when_master_is_retained(self):
        self.prepare_manager()
        self.publish_branch("candidate-pr", self.master)
        result = self.add_repository(str(self.remote), "origin/candidate-pr")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertFalse(self.manager_log.exists(), result.stdout)

    def test_add_wrapper_retained_and_unpinned_inputs_keep_original_arguments(self):
        self.prepare_manager()
        development = self.commit("Retained development history")
        self.publish_branch("hyprland-git", development)
        inputs = (
            (str(self.remote),),
            (str(self.remote), ""),
            (str(self.remote), self.master),
            (str(self.remote), "origin/hyprland-git"),
        )
        for args in inputs:
            with self.subTest(args=args):
                result = self.add_repository(*args)
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertEqual(self.manager_log.read_bytes().split(b"\0")[:-1],
                                 [b"add", *[arg.encode() for arg in args]])
                self.manager_log.unlink()

    def test_add_wrapper_transport_failure_never_invokes_manager(self):
        self.prepare_manager()
        result = self.add_repository("https://unreachable.invalid/repo.git", self.master)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertFalse(self.manager_log.exists(), result.stdout)

    def test_add_wrapper_preserves_manager_failure_status(self):
        self.prepare_manager()
        self.env["HYPREXPO_MANAGER_STATUS"] = "7"
        result = self.add_repository(str(self.remote))
        self.assertEqual(result.returncode, 7, result.stdout)
        self.assertTrue(self.manager_log.exists(), result.stdout)


if __name__ == "__main__":
    unittest.main()
