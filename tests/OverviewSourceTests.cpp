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

void expectContains(const std::string& source, const std::string& token, const std::string& label) {
    expect(source.find(token) != std::string::npos, label);
}

void expectAbsent(const std::string& source, const std::string& token, const std::string& label) {
    expect(source.find(token) == std::string::npos, label);
}

void expectOrder(const std::string& source, const std::string& first, const std::string& second, const std::string& label) {
    const auto firstPosition = source.find(first);
    const auto secondPosition = source.find(second);
    expect(firstPosition != std::string::npos && secondPosition != std::string::npos && firstPosition < secondPosition, label);
}

void expectLastOrder(const std::string& source, const std::string& first, const std::string& second, const std::string& label) {
    const auto firstPosition  = source.find(first);
    const auto secondPosition = source.rfind(second);
    expect(firstPosition != std::string::npos && secondPosition != std::string::npos && firstPosition < secondPosition, label);
}

}

int main() {
    const auto source = readFile("Overview.cpp");
    const auto overviewHeader = readFile("Overview.hpp");
    expect(!source.empty(), "Overview.cpp can be read from repo root");

    const auto function = extractFunction(source, "void removeOverview(");
    expect(!function.empty(), "removeOverview function exists");

    const auto lockPos     = function.find("const auto MON = g_pOverview->monitor();");
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
    expect(fullRender.find("if (!closing && (**PLABELEN || **PSELECTEN || showWorkspaceNumbers))") != std::string::npos,
           "workspace and selection labels stop rendering as soon as overview close begins");
    expect(fullRender.find("Hyprexpo::resolveBorderSpec(") != std::string::npos,
           "runtime border rendering uses modern-first border resolution with legacy fallback");
    expect(fullRender.find("Hyprexpo::resolveLabelPosition(") != std::string::npos && fullRender.find("Hyprexpo::resolveLabelFontSize(") != std::string::npos,
           "runtime label rendering resolves explicit modern and legacy option precedence");
    expect(fullRender.find("CompatHyprlandAPI::configValueSetByUser(") != std::string::npos,
           "runtime label compatibility checks whether each option was explicitly configured");
    expect(source.find("Config::mgr()->getConfigValue(name).setByUser") != std::string::npos,
           "config compatibility exposes explicit-setting metadata for both config providers");

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

    const auto adapterHeader = readFile("ScrollingLayoutAdapter.hpp");
    const auto adapterSource = readFile("ScrollingLayoutAdapter.cpp");
    const auto mutationHeader = readFile("ScrollingMutationTransaction.hpp");
    const auto mutationSource = readFile("ScrollingMutationTransaction.cpp");
    const auto scrollingHeader = readFile("ScrollingOverview.hpp");
    const auto scrollingSource = readFile("ScrollingOverview.cpp");
    const auto requestIdHeader = readFile("ScrollingRequestId.hpp");
    const auto diagnosticSource = readFile("ScrollingDiagnostics.cpp");
    const auto diagnosticScript = readFile("scripts/read-scrolling-diagnostic.sh");
    expect(!adapterHeader.empty() && !adapterSource.empty(), "scrolling adapter source can be read from repo root");
    expect(!mutationHeader.empty() && !mutationSource.empty(), "scrolling transaction source can be read from repo root");
    expect(!requestIdHeader.empty() && !diagnosticSource.empty() && !diagnosticScript.empty(), "shared request ID and diagnostic sources can be read from repo root");
    expectContains(requestIdHeader, "bool validRequestID", "one pure validator owns the request ID grammar");
    expectContains(requestIdHeader, "c == '.'", "shared request ID grammar accepts dots");
    expectContains(diagnosticSource, "validRequestID(request.requestID)", "topology diagnostics use the shared request ID validator");

    for (const auto& token : {"NullWorkspace", "InertWorkspace", "MissingSpace", "MissingAlgorithm", "MissingTiledAlgorithm", "WrongAlgorithmName", "CastFailure", "ExpiredTarget",
                              "ExpiredColumn", "ExpiredData", "MissingScrollingData", "ColumnCardinalityMismatch", "TargetCardinalityMismatch", "InvalidGeometry"})
        expectContains(adapterHeader, token, "adapter exposes typed fail-closed result " + std::string{token});
    for (const auto& token : {"workspace->inert()", "workspace->m_space", "algorithm()", "tiledAlgo()", "algoMatcher()->getNameForTiledAlgo", "dynamic_cast<Layout::Tiled::CScrollingAlgorithm*>",
                              "dataFor(target)", ".lock()", "stripCount()", "getDirection()", "getOffset()", "getStrip(", "targetSizes.size()", "targetDatas.size()", "calculateStripStart(",
                              "calculateStripSize(", "layoutBox", "windowStableID", "algorithmFingerprint", "dataFingerprint"})
        expectContains(adapterSource, token, "adapter implements guarded snapshot token " + std::string{token});
    expectOrder(adapterSource, "algoMatcher()->getNameForTiledAlgo", "dynamic_cast<Layout::Tiled::CScrollingAlgorithm*>", "adapter verifies matcher name before dynamic cast");
    expectOrder(adapterSource, "dataFor(target)", "column->scrollingData.lock()", "adapter locks native ownership hops after resolving target data");

    const auto snapshotImplementation = extractFunction(adapterSource, "SSnapshotResult snapshotWorkspaceImpl(");
    for (const auto& token : {".moveTape(", ".moveTapeNormalized(", ".snapToGrid(", ".setOffset(", ".adjustOffset(", ".centerCol(", ".fitCol(", ".focusColumn(", ".addStrip(",
                              ".insertStrip(", ".removeStrip(", ".swapStrips(", ".setColumnWidth(", ".setTargetSize(", ".recalculate(", "beginDragTarget", "moveWindowToWorkspace", "glReadPixels(",
                              "readPixels(", "std::ofstream", "m_title", "m_class"})
        expectAbsent(snapshotImplementation + diagnosticSource, token, "read-only snapshot/diagnostic forbids " + std::string{token});
    expectAbsent(adapterHeader, "CScrollingAlgorithm*", "adapter DTO/header retains no raw scrolling algorithm pointer");

    for (const auto& token : {"moveScrollingTarget", "SMutationResult", "executeMutation(", "snapshotPreState", "removeTarget", "addTarget", "restorePreState",
                              "setColumnWidth", "setTargetSize", "recalculate", "verifyPostconditions", "MutationResult"})
        expectContains(adapterHeader + adapterSource + mutationHeader + mutationSource, token, "native transaction boundary exposes " + std::string{token});
    const auto nativeMove = extractFunction(adapterSource, "SMutationResult moveScrollingTarget(");
    expect(!nativeMove.empty(), "same-workspace native transaction entry point exists");
    expectOrder(nativeMove, "snapshotPreState", "executeMutation(", "native transaction snapshots before the engine can remove a target");
    expectContains(adapterSource, "catch (const std::exception&", "native mutation catches standard host exceptions");
    expectContains(adapterSource, "catch (...)", "native mutation catches unknown host exceptions");
    expectOrder(adapterSource, "restorePreState", "setColumnWidth", "rollback rebuilds structure before restoring exact widths");
    expectOrder(adapterSource, "setColumnWidth", "setTargetSize", "rollback restores widths before row proportions");
    expectOrder(adapterSource, "setTargetSize", "recalculate", "native structure and sizes are restored before final recalculation");
    expectOrder(adapterSource, "recalculate", "verifyPostconditions", "native readback verifies postconditions after final recalculation");
    const auto nativeClassPosition = adapterSource.find("class CNativeMutationOperations");
    const auto nativeMutationImplementation = nativeClassPosition == std::string::npos ? std::string{} : adapterSource.substr(nativeClassPosition);
    for (const auto& token : {"beginDragTarget", "getMouseCoordsInternal", ".moveTape(", ".adjustOffset(", ".centerCol(", ".fitCol(", ".focusColumn("})
        expectAbsent(nativeMove + nativeMutationImplementation, token,
                     "native transaction forbids cursor/drag/camera operation " + std::string{token});
    expectContains(adapterSource, "controller->setDirection", "native transaction restores the exact pre-state direction after host structure changes");
    expectContains(adapterSource, "controller->setOffset(workspaceState.offset)", "native transaction restores the exact pre-state offset after host structure changes");

    for (const auto& token : {"Desktop::globalWindowController()->moveWindowToWorkspace", "controllerMove", "reverse", "reResolve", "nextUnusedOrdinaryWorkspaceID",
                              "State::workspaceState()->create", "m_monitor", "MixedFallback", "TerminalWorkspace"})
        expectContains(adapterSource + mutationSource, token, "cross/terminal transaction exposes " + std::string{token});
    expectOrder(adapterSource, "moveWindowToWorkspace", "reResolve", "cross move discards stale ownership before destination positioning");
    expectContains(adapterSource, "if (reverse)", "rollback controller path explicitly reverses ownership");
    expectContains(adapterSource, "m_createdDestination", "terminal rollback tracks the workspace created by this transaction");
    for (const auto& token : {"proveCreatedDestinationRollback", "stripCount()", "workspacesCopy()", "m_native.erase", "m_createdDestination.reset()"})
        expectContains(adapterSource, token, "terminal rollback proves empty controller/workspace release via " + std::string{token});

    expectContains(scrollingSource, "moveScrollingTarget(", "one validated release enters the native transaction boundary");
    expectContains(scrollingSource, "EMutationOutcome::Committed", "committed mutation refreshes the scrolling session");
    expectContains(scrollingSource, "EMutationOutcome::RolledBack", "rolled-back mutation refreshes the scrolling session");
    expectContains(scrollingSource, "EMutationOutcome::RollbackFailed", "fatal rollback failure safe-closes the scrolling session");
    expectContains(scrollingSource, "Log::logger->log(Log::ERR", "fatal mutation emits a high-severity diagnostic");
    const auto applyEffects = extractFunction(scrollingSource, "void CScrollingOverview::applyInputEffects(");
    expectOrder(applyEffects, "m_pendingDropIntent.reset()", "moveScrollingTarget(", "release consumes the retained transaction intent before native mutation");
    expectLastOrder(applyEffects, "moveScrollingTarget(", "refreshAfterMutation();", "commit/rollback refresh happens after the exact-once transaction result");
    expectContains(diagnosticSource, "mutationDiagnosticJson", "mutation outcomes serialize correlated postcondition evidence");
    expectContains(diagnosticSource, "violatedInvariantIDs", "mutation diagnostics include exact violated invariant IDs");
    const auto mutationDiagnostic = extractFunction(diagnosticSource, "std::string mutationDiagnosticJson(");
    for (const auto& token : {"requestId", "sessionGeneration", "beforeSummary", "afterSummary", "beforeHash", "afterHash", "mutationOutcome", "violatedInvariantIDs"})
        expectContains(mutationDiagnostic, token, "mutation diagnostic carries correlated structural evidence " + std::string{token});
    for (const auto& token : {"requestKind", "placement", "destinationColumnIndex", "destinationRowIndex", "beforeState", "afterState", "columns", "targets", "members", "width", "size"})
        expectContains(diagnosticSource, token, "mutation diagnostic retains exact native postcondition field " + std::string{token});
    for (const auto& token : {"m_title", "m_class", "windowTitle", "windowClass"})
        expectAbsent(mutationDiagnostic, token, "mutation diagnostic excludes title/class secret surface " + std::string{token});
    expectContains(scrollingSource, "m_mutationRequestSequence", "release diagnostics allocate a session-local correlation sequence");
    expectContains(adapterHeader + adapterSource, "runNativeMutationTest", "debug acceptance resolves and executes the loaded native transaction boundary");
    expectContains(adapterSource, "m_fault", "native mutation operations retain only request-scoped fault injection");
    expectContains(adapterSource, "checkpoint(EMutationPhase phase, EMutationStep step, EFaultWhen when)", "native operations evaluate exact transaction fault boundaries");
    expectContains(dispatchersSource, "hyprexpo:scrolling_mutation_test", "loaded native mutation acceptance dispatcher is registered");

    for (const auto& token : {"requestId", "sessionGeneration", "marker", "hyprlandVersion", "runtimeHash", "clientHash", "monitorId", "workspaceId", "algorithmFingerprint", "dataFingerprint", "direction",
                              "offsetBefore", "offsetAfter", "activeWorkspaceBefore", "activeWorkspaceAfter", "focusedWindowBefore", "focusedWindowAfter", "columns", "targets", "layoutBox", "visible",
                              "group", "floating", "fullscreen", "pinned", "topologyEqual", "directionEqual", "offsetEqual", "orderEqual",
                              "widthsEqual", "membershipEqual", "sizesEqual", "specialStateEqual", "algorithmEqual", "dataEqual", "activeWorkspaceEqual", "focusEqual", "mutationOutcome", "rollbackStatus", "status"})
        expectContains(diagnosticSource, token, "diagnostic JSON includes observability field " + std::string{token});
    expectContains(diagnosticSource, "snapshotWorkspace", "diagnostic serializes copied adapter snapshots");
    expectContains(diagnosticSource, "snapshotsEquivalent", "diagnostic performs before/after readback equality");
    expectContains(dispatchersSource, "hyprexpo:scrolling_debug", "read-only scrolling diagnostic dispatcher is registered");
    expectContains(dispatchersSource, "HYPREXPO_SCROLLING_DIAGNOSTIC {}", "dispatcher emits one structured diagnostic log record");
    expectContains(diagnosticScript, "expected exactly one diagnostic record", "reader rejects missing or duplicate request records");
    expectContains(diagnosticScript, "valid_request_id", "reader validates the request ID before filtering logs");

    const auto captureHeader = readFile("OverviewCapture.hpp");
    const auto captureSource = readFile("OverviewCapture.cpp");
    const auto configHeader  = readFile("HyprexpoConfig.hpp");
    const auto makefile      = readFile("Makefile");
    const auto cmake         = readFile("CMakeLists.txt");
    const auto meson         = readFile("meson.build");
    expect(!captureHeader.empty() && !captureSource.empty(), "shared overview capture source can be read from repo root");
    expectContains(captureHeader, "captureWorkspacePreview", "shared capture API exposes the grid and mixed-row workspace boundary");
    expectContains(captureHeader, "captureWindowPreview", "shared capture API exposes tight scrolling-target capture");
    expectContains(captureHeader, "completed", "tight capture result distinguishes completed textures from failed attempts");

    const auto workspaceCapture = extractFunction(captureSource, "bool captureWorkspacePreview(");
    expect(!workspaceCapture.empty(), "shared workspace capture implementation exists");
    for (const auto& token : {"beginRender(", "clearWithColor(", "applyExclusiveWorkspacePreviewState(", "applyWorkspaceWindowGoalState(", "CPinnedWindowPreviewGuard",
                              "renderWorkspace(", "restoreWorkspaceWindowGoalState(", "restoreWorkspacePreviewStates(", "restoreActiveWorkspaceAfterPreview(", "rendererState.finish()"})
        expectContains(workspaceCapture, token, "shared workspace capture preserves grid operation " + std::string{token});
    expectOrder(workspaceCapture, "beginRender(", "clearWithColor(", "workspace capture begins before clearing");
    expectOrder(workspaceCapture, "clearWithColor(", "applyExclusiveWorkspacePreviewState(", "workspace capture clears before temporary workspace state");
    expectOrder(workspaceCapture, "applyWorkspaceWindowGoalState(", "renderWorkspace(", "workspace goal state is applied before rendering");
    expectLastOrder(workspaceCapture, "renderWorkspace(", "restoreWorkspaceWindowGoalState(", "workspace goal state is restored after rendering");
    expectLastOrder(workspaceCapture, "renderWorkspace(", "restoreWorkspacePreviewStates(", "workspace preview state restores after rendering");
    expectLastOrder(workspaceCapture, "renderWorkspace(", "restoreActiveWorkspaceAfterPreview(", "active workspace restores after rendering");
    expectLastOrder(workspaceCapture, "restoreActiveWorkspaceAfterPreview(", "rendererState.finish()", "workspace capture ends only after workspace restoration");
    expectContains(overviewConstructor, "captureWorkspacePreview(", "initial grid capture uses the shared workspace helper");
    const auto redrawID = extractFunction(renderSource, "void COverview::redrawID(");
    expectContains(redrawID, "captureWorkspacePreview(", "grid redraw uses the shared workspace helper");
    expectAbsent(source, "g_pHyprRenderer->renderWorkspace(", "grid construction no longer duplicates workspace snapshot rendering");
    expectAbsent(renderSource, "g_pHyprRenderer->renderWorkspace(", "grid redraw no longer duplicates workspace snapshot rendering");

    const auto windowCapture = extractFunction(captureSource, "SWindowCaptureResult captureWindowPreview(");
    expect(!windowCapture.empty(), "tight scrolling-target capture implementation exists");
    for (const auto& token : {"createFB(", "beginFullFakeRender(", "m_bBlockSurfaceFeedback = true", "m_bRenderingSnapshot = true", "startRenderPass()", "renderWindow(",
                              "Render::RENDER_PASS_ALL, true, true", "blockScreenShader = true", "rendererState.finish()", "getTexture()", "result.completed"})
        expectContains(windowCapture, token, "tight target capture uses approved GPU operation " + std::string{token});
    expectOrder(windowCapture, "beginFullFakeRender(", "renderWindow(", "target capture begins fake rendering before the window draw");
    expectOrder(windowCapture, "renderWindow(", "rendererState.finish()", "target capture balances the renderer after drawing");
    expectOrder(windowCapture, "rendererState.finish()", "result.completed", "target capture publishes only after balanced completion");
    for (const auto& token : {"glReadPixels(", "readPixels(", "std::ofstream", ".ppm", "sha256", "SHA256"})
        expectAbsent(captureSource, token, "production capture forbids plugin-side pixel evidence path " + std::string{token});

    for (const auto& token : {"SCROLLING_THUMBNAIL_BUDGET_DEFAULT", "SCROLLING_THUMBNAIL_BUDGET_MIN", "SCROLLING_THUMBNAIL_BUDGET_MAX"})
        expectContains(configHeader, token, "thumbnail budget exposes bounded config constant " + std::string{token});
    expectContains(configSource, "plugin:hyprexpo:scrolling_thumbnail_budget", "scrolling thumbnail budget configuration is registered");
    expectContains(configSource, "HyprexpoConfig::SCROLLING_THUMBNAIL_BUDGET_DEFAULT", "scrolling thumbnail budget registration uses the resolved default");
    expectContains(configSource, ".min = HyprexpoConfig::SCROLLING_THUMBNAIL_BUDGET_MIN", "scrolling thumbnail budget registration enforces the minimum");
    expectContains(configSource, ".max = HyprexpoConfig::SCROLLING_THUMBNAIL_BUDGET_MAX", "scrolling thumbnail budget registration enforces the maximum");
    expectContains(makefile, "OverviewCapture.cpp", "Make production sources include the shared capture boundary");
    expectContains(makefile, "OverviewCapture.hpp", "Make headers include the shared capture boundary");

    const std::string buildDefinitions = makefile + cmake + meson;
    expect(!cmake.empty() && !meson.empty(), "CMake and Meson build definitions can be read from repo root");
    for (const auto& productionSource : {"main.cpp", "Dispatchers.cpp", "PluginConfig.cpp", "IOverviewSession.cpp", "Overview.cpp", "OverviewInteraction.cpp",
                                         "OverviewRender.cpp", "OverviewCapture.cpp", "ScrollingOverview.cpp", "ScrollingInputState.cpp", "ExpoGesture.cpp",
                                         "OverviewPassElement.cpp", "HyprexpoLogic.cpp", "ScrollingOverviewLogic.cpp", "ScrollingMutationTransaction.cpp",
                                         "ScrollingLayoutAdapter.cpp", "ScrollingDiagnostics.cpp"}) {
        expectContains(makefile, productionSource, "Make includes production source " + std::string{productionSource});
        expectContains(cmake, productionSource, "CMake includes production source " + std::string{productionSource});
        expectContains(meson, productionSource, "Meson includes production source " + std::string{productionSource});
    }

    for (const auto& pureSource : {"HyprexpoLogic.cpp", "ScrollingOverviewLogic.cpp", "ScrollingInputState.cpp", "ScrollingMutationTransaction.cpp"}) {
        expectContains(cmake, pureSource, "CMake logic tests include pure source " + std::string{pureSource});
        expectContains(meson, pureSource, "Meson logic tests include pure source " + std::string{pureSource});
    }

    for (const auto& suite : {"HyprexpoLogicTests", "OverviewSourceTests"}) {
        expectContains(makefile, suite, "Make registers test suite " + std::string{suite});
        expectContains(cmake, "add_executable(" + std::string{suite}, "CMake creates test executable " + std::string{suite});
        expectContains(cmake, "add_test(NAME " + std::string{suite}, "CTest registers test suite " + std::string{suite});
        expectContains(meson, "executable('" + std::string{suite} + "'", "Meson creates test executable " + std::string{suite});
        expectContains(meson, "test('" + std::string{suite} + "'", "Meson registers test suite " + std::string{suite});
    }
    expectContains(buildDefinitions, "tests/OverviewSourceTests.cpp", "build definitions include the source-contract suite");

    const auto nestedFixture = readFile("scripts/run-nested.sh");
    const auto validator = readFile("scripts/validate-scrolling-overview.sh");
    const auto scrollingGuide = readFile("docs/guides/scrolling-overview.md");
    const auto runtimeGuide = readFile("docs/guides/runtime-smoke.md");
    const auto optionsGuide = readFile("docs/configuration/options.md");
    const auto keyboardGuide = readFile("docs/configuration/keyboard.md");
    const auto dispatcherGuide = readFile("docs/reference/dispatchers.md");
    const auto readme = readFile("README.md");
    const auto publicDocs = scrollingGuide + runtimeGuide + optionsGuide + keyboardGuide + dispatcherGuide + readme;
    for (const auto& token : {"HYPREXPO_DEV_LAYOUT", "layout:scrolling", "layout:dwindle", "layoutmsg", "consume", "expel", "fit"})
        expectContains(nestedFixture, token, "nested fixture exposes scrolling contract token " + std::string{token});
    for (const auto& token : {"--all", "--evidence", "0.56.1", "5c9377c15f85c50648f35ca5a213754f95b93ca0", "f3ed01d3b024e404563e7ce18efdf1583aaa8cba",
                              "HYPREXPO_SCROLLING_DIAGNOSTIC", "HYPREXPO_SCROLLING_INPUT", "HYPREXPO_SCROLLING_MUTATION", "clients -j", "configerrors", "plugin list", "grim",
                              "same-column", "existing-column", "new-column-before", "new-column-after", "cross-scrolling", "mixed-workspace", "terminal-workspace", "outside-release", "no-op-release",
                              "touch-cancel-pending", "touch-cancel-pan", "touch-cancel-drag", "default-grid"})
        expectContains(validator, token, "validator covers acceptance token " + std::string{token});
    expectContains(validator, "--issue-85-publication-check", "validator exposes an opt-in issue-85 publication fence");
    expectContains(validator, "publication_check=false", "reusable runtime validation defaults publication checks off");
    expectContains(validator, "if [[ $publication_check == true ]]", "branch/base/remote fences execute only when explicitly requested");
    expectContains(scrollingGuide, "Optional pre-publication fence", "docs separate reusable post-merge validation from issue-85 publication checks");
    for (const auto& token : {"input.mouse.move", "input.mouse.button", "input.mouse.axis", "input.touch.down", "input.touch.motion", "input.touch.up", "input.touch.cancel",
                              "scrolling_thumbnail_budget", "m * W * H", "mixed", "terminal", "next empty", "same-column", "new column", "rollback", "0.56.1",
                              "hyprexpo:scrolling_debug", "hyprexpo:scrolling_input_test", "not full Niri parity"})
        expectContains(publicDocs, token, "public docs describe scrolling contract token " + std::string{token});

    const auto sessionHeader   = readFile("IOverviewSession.hpp");
    const auto sessionSource   = readFile("IOverviewSession.cpp");
    const auto passSource      = readFile("OverviewPassElement.cpp");
    const auto gestureSource   = readFile("ExpoGesture.cpp");
    expect(!sessionHeader.empty() && !sessionSource.empty(), "common overview session interface and factory can be read from repo root");
    expect(!scrollingHeader.empty() && !scrollingSource.empty(), "read-only scrolling session source can be read from repo root");

    for (const auto& token : {"class IOverviewSession", "virtual ~IOverviewSession", "virtual void render()", "virtual void damage()", "virtual void onDamageReported()",
                              "virtual void onPreRender()", "virtual void fullRender()", "virtual void close(", "virtual bool closeCommitted()", "virtual void setClosing(",
                              "virtual void resetSwipe()", "virtual void onSwipeUpdate(", "virtual void onSwipeEnd()", "virtual void onWindowMoveToWorkspace(",
                              "virtual void selectHoveredWorkspace()", "virtual void onKbMoveFocus(", "virtual void onKbConfirm()", "virtual void onKbSelectNumber(",
                              "virtual void onKbSelectToken(", "virtual bool selectVisibleToken(", "virtual int64_t selectedWorkspaceID()", "virtual bool selectWorkspaceByID(",
                              "virtual bool selectVisibleIndex(", "virtual bool moveWindowBetweenVisibleIndices(", "virtual bool blocksOverviewRendering()",
                              "virtual bool blocksDamageReporting()", "virtual bool isSwiping()", "virtual PHLMONITOR monitor()", "virtual uint64_t sessionGeneration()"})
        expectContains(sessionHeader, token, "session interface covers caller surface " + std::string{token});
    expectContains(sessionHeader, "virtual void onConfigReload()", "session interface refreshes caches through the polymorphic boundary");
    expectContains(sessionHeader, "std::unique_ptr<IOverviewSession> g_pOverview", "one polymorphic owner stores the active session");
    expectContains(sessionHeader, "createOverviewSession", "session factory is declared beside the polymorphic owner");
    expectContains(overviewHeader, "class COverview final : public IOverviewSession", "existing grid overview implements the common interface without mode branches");
    expectAbsent(overviewHeader, "inline std::unique_ptr<COverview> g_pOverview", "grid header no longer owns a concrete global session");

    for (const auto& token : {"workspaceUsesScrollingLayout(startedOn)", "snapshotWorkspace(startedOn)", "CScrollingOverview", "COverview", "std::make_unique<CScrollingOverview>", "std::make_unique<COverview>"})
        expectContains(sessionSource, token, "factory implements guarded selection token " + std::string{token});
    expectOrder(sessionSource, "snapshotWorkspace(startedOn)", "std::make_unique<CScrollingOverview>", "factory snapshots live native state before selecting scrolling");
    expectContains(sessionSource, "notifyScrollingFailure", "detected scrolling initialization failure is operator-visible");
    expectContains(sessionSource, "return nullptr", "detected scrolling initialization failure returns no session");
    expectAbsent(sessionSource, "using grid fallback", "detected scrolling never silently falls back to grid");
    expectOrder(sessionSource, "if (detectedScrolling)", "return std::make_unique<COverview>", "grid construction is reachable only after the detected-scrolling branch");
    expectContains(dispatchersSource, "createOverviewSession(", "dispatcher creates sessions through the single factory");
    expectContains(dispatchersSource, "failed to initialize native scrolling overview", "dispatcher reports fail-closed scrolling creation");
    expectContains(gestureSource, "createOverviewSession(", "gesture creates sessions through the single factory");
    expectAbsent(dispatchersSource + gestureSource, "std::make_unique<COverview>", "callers never instantiate the concrete grid implementation");

    for (const auto& token : {"->pMonitor", "->m_isSwiping", "->blockOverviewRendering", "->blockDamageReporting"})
        expectAbsent(mainSource + dispatchersSource + passSource + gestureSource, token, "external callers avoid concrete session field " + std::string{token});
    for (const auto& token : {"blocksOverviewRendering()", "blocksDamageReporting()", "isSwiping()", "monitor()"})
        expectContains(mainSource + dispatchersSource + passSource, token, "external callers use virtual accessor " + std::string{token});
    expectAbsent(mainSource + dispatchersSource + passSource + gestureSource, "dynamic_cast<COverview", "session callers contain no concrete overview downcast");

    expectContains(passSource, "sessionGeneration()", "render pass validates the active session identity");
    expectContains(passSource, "m_sessionGeneration", "render pass retains only an immutable generation identity");
    expectOrder(passSource, "if (!g_pOverview", "g_pOverview->fullRender()", "render pass null-checks before virtual rendering");
    expectContains(scrollingHeader, "PHLANIMVAR<float> m_transitionProgress", "scrolling session owns one compositor-managed transition value");
    expectContains(scrollingSource, "Animation::mgr()->createAnimation", "scrolling overview entry and exit use the compositor animation manager");
    expectContains(scrollingSource, "transitionForSwipe(m_swipeClosing, m_swipeDelta", "scrolling swipe delta drives the visible transition progress");
    expectContains(scrollingSource, "applyOverviewTransition", "scrolling render boxes consume the transition transform");
    expectContains(scrollingSource, "transition.opacity", "scrolling render content consumes transition opacity");
    const auto scrollingClose = extractFunction(scrollingSource, "void CScrollingOverview::close(");
    expectContains(scrollingClose, "scheduleScrollingOverviewRemoval(m_sessionGeneration)", "scrolling close defers owner destruction until transition completion");
    expectAbsent(scrollingClose, "g_pOverview.reset()", "scrolling close never destroys itself synchronously");
    expectContains(scrollingSource, "g_pEventLoopManager->doLater", "animation completion leaves its callback before destroying session ownership");
    expectContains(scrollingSource, "sessionGeneration() == generation", "deferred close cannot remove a replacement overview session");
    expectContains(mainSource, "g_pOverview.reset();", "plugin exit destroys the session owner");
    expectOrder(mainSource, "g_pOverview.reset();", "removeAllOfType", "plugin exit destroys session-owned GPU state before pass removal");

    for (const auto& token : {"class CScrollingOverview final : public IOverviewSession", "SCacheKey", "sessionGeneration", "workspaceID", "targetToken",
                              "contentDamageGeneration", "captureWidth", "captureHeight", "budgetGeneration", "PHLWINDOWREF", "WP<Layout::ITarget>", "releaseCacheEntry"})
        expectContains(scrollingHeader + scrollingSource, token, "scrolling session exposes bounded cache/lifetime token " + std::string{token});
    for (const auto& token : {"snapshotWorkspace(", "buildScene(", "initialPan(", "planCaptureBudget(", "captureWindowPreview(", "captureWorkspacePreview(",
                              "EWorkspaceKind::Scrolling", "EWorkspaceKind::Mixed", "EWorkspaceKind::Empty", "EWorkspaceKind::Terminal", "group", "floating", "fullscreen", "pinned"})
        expectContains(scrollingSource, token, "scrolling renderer covers full read-only scene token " + std::string{token});
    expectOrder(scrollingSource, "releaseCacheEntry", "captureWindowPreview(", "stale cache ownership is released before replacement capture");
    expectContains(scrollingSource, "scrollingThumbnailBudgetMultiplier", "scrolling renderer reads the bounded monitor-relative config");
    expectContains(scrollingSource, "captureWorkspacePreview(", "mixed-layout rows reuse the sole workspace capture boundary");
    expectContains(scrollingSource, "m_blockOverviewRendering = true", "mixed-layout capture forces the original grid render path");
    expectContains(scrollingSource, "m_blockDamageReporting   = true", "mixed-layout capture suppresses recursive damage feedback");
    expectContains(mainSource, "g_pOverview->onConfigReload()", "config reload invalidates the active session without a concrete downcast");
    expectAbsent(scrollingSource, "g_pHyprRenderer->renderWorkspace(", "scrolling renderer does not duplicate grid/mixed workspace capture");

    for (const auto& token : {"beginDragTarget", "moveWindowToWorkspace", ".moveTape(", ".setOffset(", ".addStrip(", ".removeStrip(", ".setColumnWidth(", ".setTargetSize(", ".recalculate("})
        expectAbsent(scrollingSource, token, "scrolling input emits intents without native mutation: " + std::string{token});

    const auto inputHeader = readFile("ScrollingInputState.hpp");
    const auto inputSource = readFile("ScrollingInputState.cpp");
    const auto inputScript = readFile("scripts/inject-scrolling-input.sh");
    expect(!inputHeader.empty() && !inputSource.empty(), "pure scrolling input state source can be read from repo root");
    expect(!inputScript.empty(), "deterministic scrolling input injection harness can be read from repo root");

    for (const auto& token : {"m_events.input.mouse.move.listen", "m_events.input.mouse.button.listen", "m_events.input.mouse.axis.listen", "m_events.input.touch.down.listen",
                              "m_events.input.touch.motion.listen", "m_events.input.touch.up.listen", "m_events.input.touch.cancel.listen"})
        expectContains(scrollingSource, token, "scrolling session subscribes to exact signal " + std::string{token});
    for (const auto& token : {"mouseMoveHook", "mouseButtonHook", "mouseAxisHook", "touchDownHook", "touchMotionHook", "touchUpHook", "touchCancelHook"})
        expectContains(scrollingHeader, token, "scrolling session owns listener handle " + std::string{token});

    expectContains(scrollingSource, "transitionInput(", "real and injected scrolling input share the pure transition function");
    expectContains(scrollingSource, "info.cancelled = effects.consume", "callbacks cancel compositor input only from the pure consume effect");
    expectContains(scrollingSource, "g_pInputManager->getMouseCoordsInternal()", "mouse signals normalize through current global logical coordinates");
    expectContains(scrollingSource, "touchToGlobalLogical(", "touch signals normalize through the touched monitor logical geometry");
    expectContains(scrollingSource, "relativeDirection", "mouse axis honors the Hyprland relative-direction payload");
    expectContains(scrollingSource, "applyInputEffects", "all runtime input effects use one application path");
    expectContains(scrollingSource, "resetInputState", "refresh, close, cancel, and teardown share one idempotent reset path");
    const auto scrollingProcessInput = extractFunction(scrollingSource, "SInputEffects CScrollingOverview::processInput(");
    expectContains(scrollingProcessInput, "m_closing || m_closeCommitted", "closing scrolling sessions reject fresh mouse and touch ownership");
    const auto scrollingKbMove = extractFunction(scrollingSource, "void CScrollingOverview::onKbMoveFocus(");
    const auto scrollingKbConfirm = extractFunction(scrollingSource, "void CScrollingOverview::onKbConfirm(");
    expectContains(scrollingKbMove, "m_closing || m_closeCommitted", "closing scrolling sessions reject fresh keyboard navigation");
    expectContains(scrollingKbConfirm, "m_closing || m_closeCommitted", "closing scrolling sessions reject fresh keyboard selection");
    const auto scrollingInstallInput = extractFunction(scrollingSource, "void CScrollingOverview::installInputListeners(");
    expectContains(scrollingInstallInput, "event.device->m_boundOutput.empty()", "touch ownership requires an explicitly bound output");
    expectAbsent(scrollingInstallInput, ": m_monitor.lock()", "unbound touch devices are not guessed onto the overview monitor");
    const auto scrollingRefresh = extractFunction(scrollingSource, "bool CScrollingOverview::refreshScene(");
    const auto scrollingDestructor = extractFunction(scrollingSource, "CScrollingOverview::~CScrollingOverview(");
    expectOrder(scrollingRefresh, "resetInputState(EResetReason::Refresh)", "releaseAllCaptureState();", "scene refresh clears input ownership before replacing cache state");
    expectOrder(scrollingDestructor, "resetInputState(EResetReason::Teardown)", "releaseAllCaptureState();", "destruction clears listeners/input before capture ownership");
    expectContains(sessionHeader, "injectScrollingInput", "injection routes through the polymorphic session without a concrete downcast");
    expectContains(overviewHeader, "injectScrollingInput", "grid session explicitly rejects scrolling-only injection");
    expectAbsent(source, "m_events.input.mouse.axis.listen", "grid mode does not install scrolling axis ownership");
    expectAbsent(source, "m_events.input.touch.cancel.listen", "grid mode does not install scrolling cancel ownership");

    expectContains(configHeader, "SCROLLING_INPUT_DEBUG_DEFAULT", "synthetic input is default-disabled");
    expectContains(configSource, "plugin:hyprexpo:scrolling_input_debug", "synthetic input debug gate is registered");
    expectContains(dispatchersSource, "hyprexpo:scrolling_input_test", "strict synthetic input dispatcher is registered");
    expectContains(dispatchersSource, "scrolling_input_debug", "synthetic input dispatcher checks the debug config gate");
    expectContains(dispatchersSource, "HYPREXPO_SCROLLING_INPUT {}", "request-correlated input diagnostics are emitted to the compositor log");
    expectContains(dispatchersSource, "g_pOverview->injectScrollingInput", "dispatcher routes synthetic input through the active session interface");

    for (const auto& token : {"hover-clear", "non-primary-passthrough", "mouse-click", "same-column", "new-column-before", "new-column-after", "cross-scrolling", "mixed-workspace",
                              "terminal-workspace", "axis-owned", "touch-pan", "touch-tap", "touch-same-column", "touch-cancel", "mismatched-cancel", "touch-reacquire", "mouse-reacquire", "stale-id",
                              "refresh-reset", "teardown-reset"})
        expectContains(inputScript, token, "injection harness covers deterministic case " + std::string{token});
    expectContains(inputScript, "--source-contract", "input harness offers a non-physical source-contract gate");
    expectContains(inputScript, "requestId", "input harness correlates every readback to its request");
    expectContains(inputScript, "ScrollingInputOracle", "source gate executes the same strict input parser and diagnostic serializer without physical hardware");
    expectContains(inputScript, "assert_case", "input harness applies case-specific behavioral oracles");
    for (const auto& token : {"axis-inside", "axis-outside", "axis-clamped", "mouse-canvas-pan", "touch-cancel-pending", "touch-cancel-pan", "touch-cancel-drag",
                              "touch-reacquire", "mouse-reacquire", "outside-release", "no-op-release"})
        expectContains(inputScript, token, "input oracle includes missing deterministic case " + std::string{token});
    for (const auto& token : {".state ==", ".consume ==", ".pan ==", ".panDelta ==", ".select ==", ".beginDrag ==", ".finishDrag ==", ".cancelDrag ==",
                              ".resetOwnership ==", ".drop =="})
        expectContains(inputScript, token, "input oracle asserts exact diagnostic field " + std::string{token});
    expectContains(inputSource, "parseInputSequence", "production and oracle share the strict finite sequence parser");
    expectContains(inputSource, "validRequestID(specs.front())", "input injection uses the shared request ID validator");
    expectContains(inputSource, "inputDiagnosticJson", "production and oracle share request-correlated JSON serialization");
    expectContains(scrollingSource, "parseInputSequence(sequence)", "runtime injection delegates strict parsing to the pure boundary");
    expectContains(scrollingSource, "inputDiagnosticJson(parsed.requestId", "runtime injection delegates readback serialization to the oracle-covered boundary");

    expectContains(makefile, "IOverviewSession.cpp", "Make production sources include the overview factory");
    expectContains(makefile, "ScrollingOverview.cpp", "Make production sources include the scrolling renderer");
    expectContains(makefile, "ScrollingInputState.cpp", "Make production and logic tests include the pure input model");
    expectContains(makefile, "ScrollingMutationTransaction.cpp", "Make production and logic tests include the pure transaction model");
    expectContains(makefile, "ScrollingMutationTransaction.hpp", "Make headers include the transaction contract");
    expectContains(makefile, "scripts/inject-scrolling-input.sh", "Make source tests track the deterministic input harness");
    expectContains(makefile, "IOverviewSession.hpp", "Make headers include the overview interface");
    expectContains(makefile, "ScrollingOverview.hpp", "Make headers include the scrolling renderer contract");

    if (failures != 0)
        return 1;

    std::cout << "OverviewSourceTests passed\n";
    return 0;
}
