import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LuaBuildDependenciesTests(unittest.TestCase):
    def configure(self, backend, packages, override=None):
        if not shutil.which(backend) or not shutil.which("pkg-config"):
            self.skipTest(f"{backend} and pkg-config are required")

        with tempfile.TemporaryDirectory(prefix="hyprexpo-lua-test-") as directory:
            fixture = Path(directory)
            pcdir = fixture / "pkgconfig"
            pcdir.mkdir()
            for name, version in packages.items():
                (pcdir / f"{name}.pc").write_text(
                    f"Name: {name}\nDescription: Test-only Lua module\n"
                    f"Version: {version}\nLibs: -llua_fixture\nCflags:\n"
                )

            env = os.environ.copy()
            for name in (
                "LUA_PKG_CONFIG", "MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES",
                "CMAKE_PREFIX_PATH", "PKG_CONFIG_SYSROOT_DIR",
            ):
                env.pop(name, None)
            env["PKG_CONFIG_PATH"] = ""
            env["PKG_CONFIG_LIBDIR"] = str(pcdir)
            env["PKG_CONFIG"] = shutil.which("pkg-config")

            if backend == "make":
                probe = fixture / "probe.mk"
                probe.write_text(
                    "probe-lua: check-lua\n"
                    "\t@printf 'Using Lua pkg-config module: %s\\n' '$(LUA_PKG_CONFIG)'\n"
                )
                command = [
                    "make", "--no-print-directory", "-s",
                    "-f", str(ROOT / "Makefile"), "-f", str(probe), "probe-lua",
                ]
                if override is not None:
                    command.append(f"LUA_PKG_CONFIG={override}")
                cwd = ROOT
            elif backend == "cmake":
                # Exercise the real dependency-selection block without requiring
                # Hyprland headers or unrelated graphics dependencies in fixtures.
                source = (ROOT / "CMakeLists.txt").read_text()
                block = source[
                    source.index('set(HYPREXPO_LUA_PKG_CONFIG ""'):
                    source.index("pkg_check_modules(deps REQUIRED")
                ]
                (fixture / "CMakeLists.txt").write_text(
                    "cmake_minimum_required(VERSION 3.27)\n"
                    "project(lua_fixture NONE)\nfind_package(PkgConfig REQUIRED)\n"
                    + block
                )
                command = ["cmake", "-S", str(fixture), "-B", str(fixture / "build")]
                if override is not None:
                    command.append(f"-DHYPREXPO_LUA_PKG_CONFIG={override}")
                cwd = fixture
            else:
                if not shutil.which("ninja"):
                    self.skipTest("ninja is required for Meson")
                source = (ROOT / "meson.build").read_text()
                block = source[
                    source.index("lua_pkg_config = get_option"):
                    source.index("shared_module(")
                ]
                (fixture / "meson.build").write_text(
                    "project('lua_fixture', 'cpp')\n" + block
                )
                shutil.copyfile(ROOT / "meson_options.txt", fixture / "meson_options.txt")
                command = ["meson", "setup", str(fixture / "build"), str(fixture)]
                if override is not None:
                    command.append(f"-Dlua_pkg_config={override}")
                cwd = fixture

            return subprocess.run(
                command, cwd=cwd, env=env, text=True, capture_output=True, timeout=60,
            )

    def assert_selected(self, backend, packages, module, override=None):
        result = self.configure(backend, packages, override)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(f"Using Lua pkg-config module: {module}", result.stdout)

    def assert_rejected(self, backend, packages, override=None):
        result = self.configure(backend, packages, override)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Lua 5.5 development files are required", result.stdout + result.stderr)

    def test_mixed_installation_selects_generic_lua55(self):
        for backend in ("make", "cmake", "meson"):
            with self.subTest(backend=backend):
                self.assert_selected(backend, {"lua": "5.5.1", "lua5.4": "5.4.9"}, "lua")

    def test_old_generic_module_falls_back_to_versioned_lua55(self):
        for backend in ("make", "cmake", "meson"):
            with self.subTest(backend=backend):
                self.assert_selected(backend, {"lua": "5.4.9", "lua5.5": "5.5.1"}, "lua5.5")

    def test_versioned_aliases_are_supported(self):
        for backend in ("make", "cmake", "meson"):
            for module in ("lua55", "lua-5.5", "lua-55"):
                with self.subTest(backend=backend, module=module):
                    self.assert_selected(backend, {module: "5.5.0"}, module)

    def test_missing_old_and_future_lua_are_rejected(self):
        for backend in ("make", "cmake", "meson"):
            for packages in ({}, {"lua": "5.4.9", "lua5.4": "5.4.9"}, {"lua": "5.6.0"}):
                with self.subTest(backend=backend, packages=packages):
                    self.assert_rejected(backend, packages)

    def test_explicit_compatible_override_is_honored(self):
        for backend in ("make", "cmake", "meson"):
            with self.subTest(backend=backend):
                self.assert_selected(
                    backend, {"lua": "5.5.1", "custom-lua": "5.5.0"},
                    "custom-lua", override="custom-lua",
                )

    def test_explicit_incompatible_or_missing_override_does_not_fall_back(self):
        for backend in ("make", "cmake", "meson"):
            for override in ("lua5.4", "missing", ""):
                with self.subTest(backend=backend, override=override):
                    # Empty override means autodetect for CMake/Meson, but an
                    # explicitly empty Make variable must not omit Lua silently.
                    if not override and backend != "make":
                        continue
                    self.assert_rejected(
                        backend, {"lua": "5.5.1", "lua5.4": "5.4.9"}, override=override,
                    )

    def test_make_gcc_flag_uses_the_compiler_option_spelling(self):
        with tempfile.TemporaryDirectory() as directory:
            probe = Path(directory) / "probe.mk"
            probe.write_text("probe-flags:\n\t@printf '%s\\n' '$(EXTRA_FLAGS)'\n")
            for compiler, expected in (("g++", "-fno-gnu-unique"), ("clang++", "")):
                with self.subTest(compiler=compiler):
                    env = os.environ.copy()
                    for key in ("MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES"):
                        env.pop(key, None)
                    result = subprocess.run(
                        ["make", "-s", "--no-print-directory", "-f", str(ROOT / "Makefile"),
                         "-f", str(probe), "probe-flags", f"CXX={compiler}"],
                        cwd=ROOT, env=env, text=True, capture_output=True, check=True,
                    )
                    self.assertEqual(result.stdout.strip(), expected)

    def test_make_relinks_existing_artifacts_after_build_configuration_changes(self):
        with tempfile.TemporaryDirectory(prefix="hyprexpo-lua-rebuild-") as directory:
            fixture = Path(directory)
            shutil.copyfile(ROOT / "Makefile", fixture / "Makefile")
            (fixture / "scripts").mkdir()
            shutil.copyfile(ROOT / "scripts/version.sh", fixture / "scripts/version.sh")
            (fixture / "VERSION").write_text("v1.2.3\n")
            pcdir = fixture / "pkgconfig"
            pcdir.mkdir()
            (pcdir / "lua.pc").write_text(
                "Name: lua\nDescription: Lua fixture\nVersion: 5.5.1\nLibs:\nCflags:\n"
            )
            env = os.environ.copy()
            for key in ("MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES", "MAKEFILES",
                        "LUA_PKG_CONFIG", "PKG_CONFIG_SYSROOT_DIR"):
                env.pop(key, None)
            env.update(PKG_CONFIG_PATH="", PKG_CONFIG_LIBDIR=str(pcdir))
            os.utime(fixture / "VERSION", (100, 100))
            os.utime(fixture / "Makefile", (300, 300))
            for goal, variable in (("all", "TARGET"), ("dev-build", "DEV_TARGET")):
                with self.subTest(goal=goal):
                    artifact = fixture / f"{goal}.so"
                    artifact.write_text("old Lua ABI artifact\n")
                    os.utime(artifact, (200, 200))
                    result = subprocess.run(
                        ["make", "--no-print-directory", goal, f"{variable}={artifact}",
                         "SRC=", "HEADERS=", "INCLUDES=", "LIBS=",
                         "PKG_CONFIG=pkg-config", "LUA_PKG_CONFIG=lua",
                         "COMPILE_PLUGIN=printf 'relinked\\n' > $@"],
                        cwd=fixture, env=env, text=True, capture_output=True, timeout=20,
                    )
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual(artifact.read_text(), "relinked\n")


if __name__ == "__main__":
    unittest.main()
