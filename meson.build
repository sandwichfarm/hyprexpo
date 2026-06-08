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

src = files(
  'main.cpp',
  'Dispatchers.cpp',
  'PluginConfig.cpp',
  'Overview.cpp',
  'OverviewInteraction.cpp',
  'OverviewRender.cpp',
  'ExpoGesture.cpp',
  'OverviewPassElement.cpp',
  'HyprexpoLogic.cpp',
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
  files('HyprexpoLogic.cpp', 'tests/HyprexpoLogicTests.cpp'),
)

test('HyprexpoLogicTests', logic_tests)
