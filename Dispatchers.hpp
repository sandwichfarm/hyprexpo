#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/devices/IKeyboard.hpp>

SP<Config::Values::CStringValue> createCancelKeyConfig();
void                             resetDispatcherRuntime();
void                             registerHyprexpoDispatchers();
// Applies plugin:hyprexpo:gesture_fingers/gesture_direction. Must run after every config reload,
// because reloading clears all registered trackpad gestures.
void                             syncExpoGestureFromConfig();
// Makes syncExpoGestureFromConfig() a no-op. Must run before the config reload in PLUGIN_EXIT so
// teardown cannot leave behind a gesture that outlives this library.
void                             disableExpoGestureSync();
bool                             isRenderingOverview();
bool                             shouldCancelOverview(const IKeyboard::SKeyEvent& event);
bool                             shouldSelectWorkspaceFromKey(const IKeyboard::SKeyEvent& event);
