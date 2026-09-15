import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from urllib.error import HTTPError
from urllib.request import Request

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('bunny_upload', ROOT / 'scripts/upload-bunny-cache.py')
cache = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cache)


class UploadTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'nar').mkdir()
        for name in ('nar/a.nar.zst', 'a.narinfo', 'nix-cache-info'):
            (self.root / name).write_text(name)
        self.env = {'BUNNY_CACHE_STORAGE_ZONE': 'cache-zone', 'BUNNY_CACHE_STORAGE_PASSWORD': 'secret'}
        self.calls = []

    def opener(self, request, timeout):
        self.calls.append((request.method, request.full_url))
        self.assertEqual(request.get_header('Accesskey'), 'secret')
        if request.method == 'HEAD':
            raise HTTPError(request.full_url, 404, 'missing', {}, None)
        self.assertEqual(int(request.get_header('Content-length')), len(request.data.read()))
        class Response:
            status = 201
            def __enter__(self): return self
            def __exit__(self, *args): pass
        return Response()

    def test_payload_precedes_metadata_and_cache_info(self):
        cache.publish(self.root, self.env, self.opener)
        self.assertEqual([url.rsplit('/', 1)[1] for method, url in self.calls if method == 'PUT'],
                         ['a.nar.zst', 'a.narinfo', 'nix-cache-info'])
        self.assertTrue(all('/cache-zone/hyprland-nix/' in url for _, url in self.calls))

    def test_failed_payload_does_not_publish_metadata(self):
        def fail(request, timeout):
            self.calls.append(request.full_url)
            raise HTTPError(request.full_url, 503, 'failed', {}, None)
        with self.assertRaises(RuntimeError):
            cache.publish(self.root, self.env, fail)
        self.assertTrue(all('/nar/' in url for url in self.calls))

    def test_existing_payload_is_not_uploaded(self):
        def hit(request, timeout):
            if request.method == 'HEAD':
                from contextlib import nullcontext
                return nullcontext()
            return self.opener(request, timeout)
        cache.publish(self.root, self.env, hit)
        self.assertFalse(any('/nar/' in url for _, url in self.calls))

    def test_invalid_configuration_never_connects(self):
        for key, value in [('BUNNY_CACHE_STORAGE_ZONE', ''), ('BUNNY_CACHE_STORAGE_PASSWORD', ''),
                           ('BUNNY_CACHE_STORAGE_ENDPOINT', 'http://example.com'),
                           ('BUNNY_CACHE_STORAGE_ENDPOINT', 'https://user:password@example.com'),
                           ('BUNNY_CACHE_PREFIX', '../site')]:
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                cache.publish(self.root, {**self.env, key: value}, self.opener)
        self.assertEqual(self.calls, [])

    def test_symlink_is_rejected(self):
        (self.root / 'nar/a.nar.zst').unlink()
        (self.root / 'nar/a.nar.zst').symlink_to('/etc/hosts')
        with self.assertRaises(ValueError):
            cache.publish(self.root, self.env, self.opener)
        self.assertEqual(self.calls, [])

    def test_redirect_cannot_forward_credentials(self):
        request = Request('https://storage.bunnycdn.com/cache/item',
                          headers={'AccessKey': 'secret'}, method='HEAD')
        for destination in ('https://other.example/item', 'http://storage.bunnycdn.com/item'):
            with self.assertRaises(HTTPError) as caught:
                cache.NoRedirects().redirect_request(request, None, 302, 'redirect', {}, destination)
            caught.exception.close()


class BuildCacheTests(unittest.TestCase):
    def run_config(self, extra):
        env = {k: v for k, v in os.environ.items() if not k.startswith('BUNNY_CACHE_')}
        env.update(extra)
        return subprocess.run(['bash', '-euc', 'source "$1"; configure_hyprland_cache; printf "%s" "${NIX_CONFIG:-}"',
                               'test', str(ROOT / 'scripts/ci-hyprland-cache.sh')], env=env, text=True, capture_output=True)

    def test_unconfigured_cache_keeps_existing_configuration(self):
        result = self.run_config({'NIX_CONFIG': 'sandbox = true'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, 'sandbox = true')

    def test_signed_cache_appends_standard_trust(self):
        result = self.run_config({'BUNNY_CACHE_URL': 'https://cache.example/hyprland-nix/',
                                  'BUNNY_CACHE_PUBLIC_KEY': 'cache:YWJjZA==', 'NIX_CONFIG': 'sandbox = true'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('extra-substituters = https://cache.example/hyprland-nix', result.stdout)
        self.assertIn('extra-trusted-public-keys = cache:YWJjZA==', result.stdout)
        self.assertIn('sandbox = true', result.stdout)
        self.assertNotIn('require-sigs = false', result.stdout)

    def test_partial_configuration_and_injection_fail(self):
        for extra in ({'BUNNY_CACHE_URL': 'https://cache.example'},
                      {'BUNNY_CACHE_PUBLIC_KEY': 'cache:YWJjZA=='},
                      {'BUNNY_CACHE_URL': 'https://cache.example\nrequire-sigs = false', 'BUNNY_CACHE_PUBLIC_KEY': 'cache:YWJjZA=='}):
            self.assertNotEqual(self.run_config(extra).returncode, 0)

    def test_writer_missing_credentials_fails_before_build(self):
        result = self.run_config({'BUNNY_CACHE_URL': 'https://cache.example/hyprland-nix',
                                  'BUNNY_CACHE_PUBLIC_KEY': 'cache:YWJjZA==', 'BUNNY_CACHE_WRITE': 'true'})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('signing key', result.stderr)

    def test_build_failure_never_uploads(self):
        with tempfile.TemporaryDirectory() as directory:
            env = dict(os.environ, BUNNY_CACHE_WRITE='true')
            result = subprocess.run(['bash', '-euc', 'source "$1"; nix() { return 23; }; prepare_hyprland_cache "$2" "$2"',
                                     'test', str(ROOT / 'scripts/ci-hyprland-cache.sh'), directory], env=env, capture_output=True)
            self.assertEqual(result.returncode, 23)

    def test_read_only_builds_both_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(['bash', '-euc', '''source "$1"
nix() { printf '%s\n' "$*" >&2; printf '[]'; }
prepare_hyprland_cache "$2" "$2"
''', 'test', str(ROOT / 'scripts/ci-hyprland-cache.sh'), directory],
                env=dict(os.environ, BUNNY_CACHE_WRITE='false'), text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('#hyprland.dev', result.stderr)
            self.assertIn('--no-update-lock-file', result.stderr)
            self.assertNotIn('copy --to', result.stderr)

    def test_workflow_secrets_exclude_all_prs_and_untrusted_refs(self):
        for name in ('compatibility.yml', 'upstream-tip.yml'):
            workflow = (ROOT / '.github/workflows' / name).read_text()
            for line in workflow.splitlines():
                if 'secrets.BUNNY_CACHE_' in line:
                    self.assertIn("github.event_name != 'pull_request'", line)
                    self.assertIn("github.ref == 'refs/heads/master'", line)
                    self.assertIn("github.ref == 'refs/heads/hyprland-git'", line)

    def test_writer_publishes_only_missing_closures_and_propagates_upload_failure(self):
        for present, upload_exit in ((False, 0), (True, 0), (False, 19)):
            with self.subTest(present=present, upload_exit=upload_exit), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'scripts').mkdir()
                (root / 'scripts/upload-bunny-cache.py').write_text(
                    f'import sys\nprint("UPLOAD_CALLED")\nsys.exit({upload_exit})\n')
                env = dict(os.environ, BUNNY_CACHE_WRITE='true', BUNNY_CACHE_SIGNING_KEY='test-key',
                           BUNNY_CACHE_URL='https://cache.example/hyprland-nix',
                           BUNNY_CACHE_STORAGE_ZONE='test', BUNNY_CACHE_STORAGE_PASSWORD='test-password',
                           CACHE_PRESENT='0' if present else '1', RUNNER_TEMP=directory)
                result = subprocess.run(['bash', '-euc', '''source "$1"
nix() {
    case "$1" in
        build) printf '[{"outputs":{"out":"/nix/store/test-out","dev":"/nix/store/test-dev"}}]' ;;
        path-info) return "$CACHE_PRESENT" ;;
        copy) printf 'COPY_CALLED %s\n' "$*" ;;
        *) return 99 ;;
    esac
}
prepare_hyprland_cache "$2" "$2"
''', 'test', str(ROOT / 'scripts/ci-hyprland-cache.sh'), directory], env=env, text=True, capture_output=True)
                self.assertEqual(result.returncode, upload_exit, result.stderr)
                self.assertEqual('UPLOAD_CALLED' in result.stdout, not present)
                self.assertEqual('COPY_CALLED' in result.stdout, not present)
                self.assertEqual(list(root.glob('hyprland-cache.*')), [])
                if not present:
                    self.assertIn('/nix/store/test-dev', result.stdout)
                    self.assertIn('/nix/store/test-out', result.stdout)


if __name__ == '__main__':
    unittest.main()
