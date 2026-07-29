#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/devices/IKeyboard.hpp>

SP<Config::Values::CStringValue> createCancelKeyConfig();
void                             resetDispatcherRuntime();
void                             registerHyprexpoDispatchers();
void                             syncExpoGestureFromConfig();
void                             disableExpoGestureSync();
bool                             isRenderingOverview();
bool                             shouldCancelOverview(const IKeyboard::SKeyEvent& event);
bool                             shouldSelectWorkspaceFromKey(const IKeyboard::SKeyEvent& event);
