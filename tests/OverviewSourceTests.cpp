#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string& label) {
    if (condition)
        return;

    ++failures;
    std::cerr << "FAIL: " << label << '\n';
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file)
        return {};

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string extractFunction(const std::string& source, const std::string& signature) {
    for (auto start = source.find(signature); start != std::string::npos; start = source.find(signature, start + 1)) {
        const auto open = source.find('{', start);
        if (open == std::string::npos)
            return {};

        // Ignore forward declarations.
        const auto semicolon = source.find(';', start);
        if (semicolon != std::string::npos && semicolon < open)
            continue;

        int depth = 0;
        for (size_t i = open; i < source.size(); ++i) {
            if (source[i] == '{')
                ++depth;
            else if (source[i] == '}') {
                --depth;
                if (depth == 0)
                    return source.substr(start, i - start + 1);
            }
        }

        return {};
    }

    return {};
}

}

int main() {
    const auto source = readFile("Overview.cpp");
    expect(!source.empty(), "Overview.cpp can be read from repo root");

    const auto function = extractFunction(source, "void removeOverview(");
    expect(!function.empty(), "removeOverview function exists");

    const auto lockPos     = function.find("const auto MON = g_pOverview->pMonitor.lock();");
    const auto resetPos    = function.find("g_pOverview.reset();");
    const auto damagePos   = function.find("g_pHyprRenderer->damageMonitor(MON);");
    const auto schedulePos = function.find("MON->scheduleFrame();");

    expect(lockPos != std::string::npos, "removeOverview captures monitor before teardown");
    expect(resetPos != std::string::npos, "removeOverview destroys active overview");
    expect(damagePos != std::string::npos, "removeOverview damages monitor after teardown");
    expect(schedulePos != std::string::npos, "removeOverview schedules a frame after teardown");
    expect(lockPos < resetPos, "monitor is captured before overview reset");
    expect(resetPos < damagePos, "monitor damage happens after overview reset");
    expect(damagePos < schedulePos, "frame scheduling follows monitor damage");

    const auto mainSource = readFile("main.cpp");
    expect(!mainSource.empty(), "main.cpp can be read from repo root");
    expect(mainSource.find("const Time::steady_tp& now") != std::string::npos, "render hook uses the Hyprland 0.56 time-point ABI");
    expect(mainSource.find("_ZN7Monitor8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE") != std::string::npos,
           "damage hook uses the Hyprland 0.56 namespaced monitor symbol");
    expect(mainSource.find("_ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE") == std::string::npos,
           "damage hook no longer uses the pre-0.56 monitor symbol");

    const auto configSource = readFile("PluginConfig.cpp");
    expect(configSource.find("plugin:hyprexpo:drag_drop_enable") != std::string::npos,
           "drag/drop enable configuration is registered");
    expect(source.find("plugin:hyprexpo:drag_drop_enable") != std::string::npos,
           "drag/drop enable configuration has a compatibility default");
    expect(source.find("if (**PDRAGDROPENABLE)\n                beginWindowDrag();") != std::string::npos,
           "drag/drop enable configuration gates drag start");
    expect(source.find("if (**PDRAGDROPENABLE && finishWindowDrag())") != std::string::npos,
           "drag/drop enable configuration gates drag completion");

    const auto dispatchersSource = readFile("Dispatchers.cpp");
    expect(!dispatchersSource.empty(), "Dispatchers.cpp can be read from repo root");
    const auto expoDispatcher = extractFunction(dispatchersSource, "static SDispatchResult onExpoDispatcher(std::string arg) {");
    expect(!expoDispatcher.empty(), "expo dispatcher function exists");

    const auto numberKeyDispatcher = extractFunction(dispatchersSource, "static SDispatchResult changeToSingleDigitWorkspace(const std::string& arg) {");
    expect(!numberKeyDispatcher.empty(), "number-key dispatcher function exists");
    expect(numberKeyDispatcher.find("g_pOverview->selectWorkspaceByID(workspaceID)") != std::string::npos,
           "workspace-mode raw number keys preserve workspace-ID selection");

    const auto rawNumberSelection = extractFunction(dispatchersSource, "bool shouldSelectWorkspaceFromKey(const IKeyboard::SKeyEvent& event) {");
    expect(!rawNumberSelection.empty(), "raw number-key selection function exists");
    expect(rawNumberSelection.find("g_pNumberKeyModeConfig") != std::string::npos,
           "raw number-key handling reads the dedicated retained mode");
    expect(rawNumberSelection.find("g_pNumberKeyModeConfig->value()") != std::string::npos,
           "raw number-key handling reads the retained V2 string value");
    expect(rawNumberSelection.find("HyprlandAPI::getConfigValue") == std::string::npos,
           "raw number-key handling does not dereference the deprecated config API");
    expect(configSource.find("addConfigValue(createNumberKeyModeConfig())") != std::string::npos,
           "number-key mode registration retains the V2 config value");
    expect(dispatchersSource.find("g_pNumberKeyModeConfig.reset()") != std::string::npos,
           "number-key mode releases its retained V2 config value during teardown");
    const auto passthroughMode = rawNumberSelection.find("ENumberKeyMode::Passthrough");
    const auto indexMode       = rawNumberSelection.find("ENumberKeyMode::Index", passthroughMode);
    const auto workspaceMode   = rawNumberSelection.find("return changeToSingleDigitWorkspace(arg).success;", indexMode);
    expect(passthroughMode != std::string::npos && indexMode != std::string::npos &&
               rawNumberSelection.substr(passthroughMode, indexMode - passthroughMode).find("return false;") != std::string::npos,
           "passthrough mode leaves raw number keys uncancelled");
    expect(indexMode != std::string::npos && workspaceMode != std::string::npos &&
               rawNumberSelection.substr(indexMode, workspaceMode - indexMode).find("g_pOverview->onKbSelectToken(visibleIndex)") != std::string::npos &&
               rawNumberSelection.substr(indexMode, workspaceMode - indexMode).find("return true;") != std::string::npos,
           "index mode selects from active-overview visible tile positions");
    expect(workspaceMode != std::string::npos,
           "workspace mode retains the legacy global-workspace fallback");
    expect(rawNumberSelection.find("if (arg == \"0\")") != std::string::npos,
           "workspace mode preserves legacy zero-key passthrough");
    expect(mainSource.find("if (shouldSelectWorkspaceFromKey(event))\n            info.cancelled = true;") != std::string::npos,
           "raw key events are cancelled only when the mode-specific handler consumes them");

    const auto interactionSource = readFile("OverviewInteraction.cpp");
    expect(!interactionSource.empty(), "OverviewInteraction.cpp can be read from repo root");
    const auto numberSelection = extractFunction(interactionSource, "void COverview::onKbSelectNumber(int num) {");
    expect(!numberSelection.empty(), "workspace-number dispatcher selection function exists");
    expect(numberSelection.find("selectWorkspaceByID(num)") != std::string::npos,
           "kb_selectn remains workspace-ID based");
    expect(numberSelection.find("number_key_mode") == std::string::npos && numberSelection.find("numberKeyToVisibleIndex") == std::string::npos,
           "kb_selectn semantics do not depend on the raw number-key mode");

    const auto numberDispatcher = extractFunction(dispatchersSource, "static SDispatchResult onKbSelectNumberDispatcher(std::string arg) {");
    const auto indexDispatcher  = extractFunction(dispatchersSource, "static SDispatchResult onKbSelectIndexDispatcher(std::string arg) {");
    expect(numberDispatcher.find("g_pOverview->onKbSelectNumber(num)") != std::string::npos,
           "kb_selectn keeps routing to workspace-number selection");
    expect(indexDispatcher.find("g_pOverview->onKbSelectToken(idx - 1)") != std::string::npos,
           "kb_selecti keeps routing to one-based visible-index selection");

    const auto toggleStart = expoDispatcher.find("if (arg == \"toggle\")");
    const auto cancelStart = expoDispatcher.find("if (arg == \"cancel\")", toggleStart);
    const auto toggleBlock = toggleStart == std::string::npos || cancelStart == std::string::npos ? std::string{} : expoDispatcher.substr(toggleStart, cancelStart - toggleStart);
    expect(toggleBlock.find("g_pOverview->close(false);") != std::string::npos, "plain toggle close does not select a fallback workspace");

    const auto offStart = expoDispatcher.find("if (arg == \"off\" || arg == \"close\" || arg == \"disable\")");
    const auto offEnd   = expoDispatcher.find("\n    if (g_pOverview)\n        return {};", offStart);
    const auto offBlock = offStart == std::string::npos || offEnd == std::string::npos ? std::string{} : expoDispatcher.substr(offStart, offEnd - offStart);
    expect(offBlock.find("g_pOverview->close(false);") != std::string::npos, "plain off and close commands do not select a fallback workspace");

    const auto overviewConstructor = extractFunction(source, "COverview::COverview(");
    expect(!overviewConstructor.empty(), "overview constructor exists");
    const auto gapExpansionPos = overviewConstructor.find("Hyprexpo::expandDynamicWorkspaceIDs(");
    const auto dynamicResizePos = overviewConstructor.find("images.resize(visibleWorkspaceIDs.size())");
    expect(gapExpansionPos != std::string::npos, "dynamic workspace enumeration uses the bounded expansion helper");
    expect(dynamicResizePos != std::string::npos && gapExpansionPos < dynamicResizePos, "dynamic expansion is bounded before image allocation");
    expect(overviewConstructor.find("for (int64_t id = minID; id <= maxID; ++id)") == std::string::npos,
           "dynamic workspace enumeration has no unbounded min-to-max fill loop");

    const auto renderSource = readFile("OverviewRender.cpp");
    expect(!renderSource.empty(), "OverviewRender.cpp can be read from repo root");
    const auto fullRender = extractFunction(renderSource, "void COverview::fullRender(");
    expect(!fullRender.empty(), "overview fullRender function exists");
    expect(fullRender.find("Hyprexpo::shouldShowWorkspaceLabel(") != std::string::npos,
           "runtime label rendering uses modern label_enable and label_show policy in every grid mode");
    expect(fullRender.find("Hyprexpo::resolveBorderSpec(") != std::string::npos,
           "runtime border rendering uses modern-first border resolution with legacy fallback");

    expect(dispatchersSource.find("HyprlandAPI::getConfigValue") == std::string::npos,
           "gesture config avoids the legacy hyprlang getter, which is null under CONFIG_LUA");

    const auto gestureSync = extractFunction(dispatchersSource, "void syncExpoGestureFromConfig(");
    expect(!gestureSync.empty(), "syncExpoGestureFromConfig exists");
    expect(gestureSync.find("g_unloading || g_gestureRegistrationDisabled") != std::string::npos, "gesture sync bails out while the plugin is unloading");

    const auto gestureRegister = extractFunction(dispatchersSource, "static SDispatchResult registerExpoGesture(");
    expect(!gestureRegister.empty(), "registerExpoGesture definition exists");
    expect(gestureRegister.find("g_unloading || g_gestureRegistrationDisabled") != std::string::npos,
           "every gesture registration path, including the Lua helper, is fenced during unload");

    const auto exitFunction = extractFunction(mainSource, "APICALL EXPORT void PLUGIN_EXIT(");
    expect(!exitFunction.empty(), "PLUGIN_EXIT exists");
    const auto disablePos = exitFunction.find("disableExpoGestureRegistration();");
    const auto reloadPos  = exitFunction.find("Config::mgr()->reload();");
    expect(disablePos != std::string::npos, "PLUGIN_EXIT disables gesture registration");
    expect(reloadPos != std::string::npos, "PLUGIN_EXIT reloads the config to clear registered gestures");
    expect(disablePos < reloadPos, "the registration fence is set before the teardown reload re-runs the config");

    expect(mainSource.find("config.reloaded.listen") != std::string::npos, "the gesture is re-applied after every config reload");

    if (failures != 0)
        return 1;

    std::cout << "OverviewSourceTests passed\n";
    return 0;
}
