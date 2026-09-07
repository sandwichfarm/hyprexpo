project('hyprexpo', 'cpp',
  version: '0.1',
  default_options: ['buildtype=release'],
)

cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.has_argument('-std=c++23')
  add_global_arguments('-std=c++23', language: 'cpp')
elif cpp_compiler.has_argument('-std=c++2b')
  add_global_arguments('-std=c++2b', language: 'cpp')
else
  error('Could not configure current C++ compiler (' + cpp_compiler.get_id() + ' ' + cpp_compiler.version() + ') with required C++ standard (C++23)')
endif

add_project_arguments(
  [
    '-Wno-narrowing',
  ],
  language: 'cpp')

# Bake the plugin version from the VERSION file (single source of truth).
version_baked = run_command('sh', meson.current_source_dir() / 'scripts/version.sh', check: true).stdout().strip()
add_project_arguments('-DHYPREXPO_VERSION="' + version_baked + '"', language: 'cpp')

src = files(
  'src/main.cpp',
  'src/Dispatchers.cpp',
  'src/PluginConfig.cpp',
  'src/IOverviewSession.cpp',
  'src/Overview.cpp',
  'src/OverviewInteraction.cpp',
  'src/OverviewRender.cpp',
  'src/OverviewCapture.cpp',
  'src/ScrollingOverview.cpp',
  'src/ScrollingInputState.cpp',
  'src/ExpoGesture.cpp',
  'src/OverviewPassElement.cpp',
  'src/HyprexpoLogic.cpp',
  'src/ScrollingOverviewLogic.cpp',
  'src/ScrollingMutationTransaction.cpp',
  'src/ScrollingLayoutAdapter.cpp',
  'src/ScrollingDiagnostics.cpp',
)

hyprland = dependency('hyprland')
lua_dep = dependency('lua5.4', required: false)
if not lua_dep.found()
  lua_dep = dependency('lua')
endif

shared_module(meson.project_name(), src,
  dependencies: [
    dependency('hyprland'),
    dependency('pixman-1'),
    dependency('libdrm'),
    dependency('pangocairo'),
    dependency('libinput'),
    dependency('libudev'),
    dependency('wayland-server'),
    dependency('xkbcommon'),
    lua_dep,
  ],
  install: true,
)

logic_tests = executable('HyprexpoLogicTests',
  files(
    'src/HyprexpoLogic.cpp',
    'src/ScrollingOverviewLogic.cpp',
    'src/ScrollingInputState.cpp',
    'src/ScrollingMutationTransaction.cpp',
    'tests/HyprexpoLogicTests.cpp',
  ),
)

test('HyprexpoLogicTests', logic_tests)

source_tests = executable('OverviewSourceTests',
  files('tests/OverviewSourceTests.cpp'),
)

test('OverviewSourceTests', source_tests,
  workdir: meson.current_source_dir(),
)

registry_tests = executable('RegistryTeardownTests', files('tests/RegistryTeardownTests.cpp'))
test('RegistryTeardownTests', registry_tests)

executable('ScrollingInputOracle', files(
  'src/HyprexpoLogic.cpp',
  'src/ScrollingOverviewLogic.cpp',
  'src/ScrollingInputState.cpp',
  'tests/ScrollingInputOracle.cpp',
))
