#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/devices/IKeyboard.hpp>

SP<Config::Values::CStringValue> createCancelKeyConfig();
SP<Config::Values::CStringValue> createNumberKeyModeConfig();
void                             resetDispatcherRuntime();
void                             registerHyprexpoDispatchers();
void                             syncExpoGestureFromConfig();
void                             disableExpoGestureRegistration();
bool                             isRenderingOverview();
bool                             shouldCancelOverview(const IKeyboard::SKeyEvent& event);
bool                             shouldSelectWorkspaceFromKey(const IKeyboard::SKeyEvent& event);
