cmake_minimum_required(VERSION 3.27)

project(hyprexpo
    DESCRIPTION "hyprexpo plugin for Hyprland"
    VERSION 0.1
)

set(CMAKE_CXX_STANDARD 23)

set(HYPREXPO_SRC
    src/main.cpp
    src/Dispatchers.cpp
    src/PluginConfig.cpp
    src/IOverviewSession.cpp
    src/Overview.cpp
    src/OverviewInteraction.cpp
    src/OverviewBackground.cpp
    src/OverviewRender.cpp
    src/OverviewCapture.cpp
    src/ScrollingOverview.cpp
    src/ScrollingInputState.cpp
    src/ExpoGesture.cpp
    src/OverviewPassElement.cpp
    src/HyprexpoLogic.cpp
    src/ScrollingOverviewLogic.cpp
    src/ScrollingMutationTransaction.cpp
    src/ScrollingLayoutAdapter.cpp
    src/ScrollingDiagnostics.cpp
)

add_library(hyprexpo SHARED ${HYPREXPO_SRC})

# An unspecified configuration otherwise leaves synchronous image filtering at
# -O0. Keep explicit configurations (including Debug) and test assertions intact.
target_compile_options(hyprexpo PRIVATE "$<$<CONFIG:>:-O2>")

# Bake the plugin version from the VERSION file (single source of truth).
execute_process(
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/scripts/version.sh
    OUTPUT_VARIABLE HYPREXPO_VERSION_BAKED
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
target_compile_definitions(hyprexpo PRIVATE HYPREXPO_VERSION="${HYPREXPO_VERSION_BAKED}")

find_package(PkgConfig REQUIRED)

set(HYPREXPO_LUA_PKG_CONFIG "" CACHE STRING "Lua 5.5 pkg-config module to use; leave empty to detect")
set(HYPREXPO_LUA_PKG_CONFIG_CANDIDATES lua lua5.5 lua55 lua-5.5 lua-55)
if(HYPREXPO_LUA_PKG_CONFIG)
    set(HYPREXPO_LUA_PKG_CONFIG_CANDIDATES ${HYPREXPO_LUA_PKG_CONFIG})
endif()

foreach(lua_pkg IN LISTS HYPREXPO_LUA_PKG_CONFIG_CANDIDATES)
    pkg_check_modules(HYPREXPO_LUA QUIET IMPORTED_TARGET "${lua_pkg}>=5.5" "${lua_pkg}<5.6")
    if(HYPREXPO_LUA_FOUND)
        set(HYPREXPO_LUA_PKG_CONFIG_RESOLVED ${lua_pkg})
        break()
    endif()
endforeach()

if(NOT HYPREXPO_LUA_FOUND)
    message(FATAL_ERROR "Lua 5.5 development files are required for Hyprland 0.57; tried pkg-config modules: ${HYPREXPO_LUA_PKG_CONFIG_CANDIDATES}")
endif()

message(STATUS "Using Lua pkg-config module: ${HYPREXPO_LUA_PKG_CONFIG_RESOLVED}")

pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    hyprland
    hyprgraphics
    libdrm
    libinput
    libudev
    pangocairo
    pixman-1
    wayland-server
    xkbcommon
)
target_link_libraries(hyprexpo PRIVATE rt PkgConfig::deps PkgConfig::HYPREXPO_LUA)

enable_testing()
add_executable(BackgroundImageTests tests/BackgroundImageTests.cpp)
add_test(NAME BackgroundImageTests COMMAND BackgroundImageTests)
add_executable(HyprexpoLogicTests
    src/HyprexpoLogic.cpp
    src/ScrollingOverviewLogic.cpp
    src/ScrollingInputState.cpp
    src/ScrollingMutationTransaction.cpp
    tests/HyprexpoLogicTests.cpp
)
add_test(NAME HyprexpoLogicTests COMMAND HyprexpoLogicTests)

add_executable(OverviewSourceTests
    tests/OverviewSourceTests.cpp
)
add_test(NAME OverviewSourceTests COMMAND OverviewSourceTests)
add_executable(PreviewFramebufferTests tests/PreviewFramebufferTests.cpp)
add_test(NAME PreviewFramebufferTests COMMAND PreviewFramebufferTests)
add_executable(RegistryTeardownTests tests/RegistryTeardownTests.cpp)
add_test(NAME RegistryTeardownTests COMMAND RegistryTeardownTests)
add_executable(ScrollingInputOracle
    src/HyprexpoLogic.cpp
    src/ScrollingOverviewLogic.cpp
    src/ScrollingInputState.cpp
    tests/ScrollingInputOracle.cpp
)
set_tests_properties(OverviewSourceTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

install(TARGETS hyprexpo)
