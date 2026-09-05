cmake_minimum_required(VERSION 3.27)

project(hyprexpo
    DESCRIPTION "hyprexpo plugin for Hyprland"
    VERSION 0.1
)

set(CMAKE_CXX_STANDARD 23)

set(HYPREXPO_SRC
    main.cpp
    Dispatchers.cpp
    PluginConfig.cpp
    IOverviewSession.cpp
    Overview.cpp
    OverviewInteraction.cpp
    OverviewRender.cpp
    OverviewCapture.cpp
    ScrollingOverview.cpp
    ScrollingInputState.cpp
    ExpoGesture.cpp
    OverviewPassElement.cpp
    HyprexpoLogic.cpp
    ScrollingOverviewLogic.cpp
    ScrollingMutationTransaction.cpp
    ScrollingLayoutAdapter.cpp
    ScrollingDiagnostics.cpp
)

add_library(hyprexpo SHARED ${HYPREXPO_SRC})

# Bake the plugin version from the VERSION file (single source of truth).
execute_process(
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/scripts/version.sh
    OUTPUT_VARIABLE HYPREXPO_VERSION_BAKED
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
target_compile_definitions(hyprexpo PRIVATE HYPREXPO_VERSION="${HYPREXPO_VERSION_BAKED}")

find_package(PkgConfig REQUIRED)

set(HYPREXPO_LUA_PKG_CONFIG "" CACHE STRING "Lua pkg-config module to use; leave empty to try lua5.4 then lua")
set(HYPREXPO_LUA_PKG_CONFIG_CANDIDATES lua5.4 lua)
if(HYPREXPO_LUA_PKG_CONFIG)
    set(HYPREXPO_LUA_PKG_CONFIG_CANDIDATES ${HYPREXPO_LUA_PKG_CONFIG})
endif()

foreach(lua_pkg IN LISTS HYPREXPO_LUA_PKG_CONFIG_CANDIDATES)
    pkg_check_modules(HYPREXPO_LUA QUIET IMPORTED_TARGET ${lua_pkg})
    if(HYPREXPO_LUA_FOUND)
        set(HYPREXPO_LUA_PKG_CONFIG_RESOLVED ${lua_pkg})
        break()
    endif()
endforeach()

if(NOT HYPREXPO_LUA_FOUND)
    message(FATAL_ERROR "Could not find Lua through pkg-config; tried: ${HYPREXPO_LUA_PKG_CONFIG_CANDIDATES}")
endif()

message(STATUS "Using Lua pkg-config module: ${HYPREXPO_LUA_PKG_CONFIG_RESOLVED}")

pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    hyprland
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
add_executable(HyprexpoLogicTests
    HyprexpoLogic.cpp
    ScrollingOverviewLogic.cpp
    ScrollingInputState.cpp
    ScrollingMutationTransaction.cpp
    tests/HyprexpoLogicTests.cpp
)
add_test(NAME HyprexpoLogicTests COMMAND HyprexpoLogicTests)

add_executable(OverviewSourceTests
    tests/OverviewSourceTests.cpp
)
add_test(NAME OverviewSourceTests COMMAND OverviewSourceTests)
add_executable(RegistryTeardownTests tests/RegistryTeardownTests.cpp)
add_test(NAME RegistryTeardownTests COMMAND RegistryTeardownTests)
add_executable(ScrollingInputOracle
    HyprexpoLogic.cpp
    ScrollingOverviewLogic.cpp
    ScrollingInputState.cpp
    tests/ScrollingInputOracle.cpp
)
set_tests_properties(OverviewSourceTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

install(TARGETS hyprexpo)
