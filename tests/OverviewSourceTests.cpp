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

size_t countOccurrences(const std::string& source, const std::string& needle) {
    size_t count = 0;
    for (size_t pos = source.find(needle); pos != std::string::npos; pos = source.find(needle, pos + needle.size()))
        ++count;
    return count;
}

}

int main() {
    const auto source = readFile("Overview.cpp");
    expect(!source.empty(), "Overview.cpp can be read from repo root");

    const auto function = extractFunction(source, "void removeOverview(");
    expect(!function.empty(), "removeOverview function exists");

    const auto lockPos     = function.find("const auto MON = OV->pMonitor.lock();");
    const auto resetPos    = function.find("destroyOverview(OV);");
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
    expect(source.find("if (**PDRAGDROPENABLE && TARGET)\n                TARGET->beginWindowDrag();") != std::string::npos,
           "drag/drop enable configuration gates drag start");
    expect(source.find("if (**PDRAGDROPENABLE && SOURCE)") != std::string::npos && source.find("SOURCE->finishWindowDrag()") != std::string::npos,
           "drag/drop enable configuration gates drag completion");

    const auto dispatchersSource = readFile("Dispatchers.cpp");
    expect(!dispatchersSource.empty(), "Dispatchers.cpp can be read from repo root");
    const auto gestureHeader = readFile("ExpoGesture.hpp");
    expect(!gestureHeader.empty(), "ExpoGesture.hpp can be read from repo root");
    const auto gestureSource = readFile("ExpoGesture.cpp");
    expect(!gestureSource.empty(), "ExpoGesture.cpp can be read from repo root");
    const auto overviewHeader = readFile("Overview.hpp");
    expect(!overviewHeader.empty(), "Overview.hpp can be read from repo root");
    const auto expoDispatcher = extractFunction(dispatchersSource, "static SDispatchResult onExpoDispatcher(std::string arg) {");
    expect(!expoDispatcher.empty(), "expo dispatcher function exists");

    const auto numberKeyDispatcher = extractFunction(dispatchersSource, "static SDispatchResult changeToSingleDigitWorkspace(const std::string& arg) {");
    expect(!numberKeyDispatcher.empty(), "number-key dispatcher function exists");
    expect(numberKeyDispatcher.find("OV->selectWorkspaceByID(workspaceID)") != std::string::npos,
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
               rawNumberSelection.substr(indexMode, workspaceMode - indexMode).find("OV->onKbSelectToken(visibleIndex)") != std::string::npos &&
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

    const auto enterSubmap = extractFunction(interactionSource, "void COverview::enterSubmapIfEnabled() {");
    expect(!enterSubmap.empty(), "keyboard navigation submap entry function exists");
    const auto captureSubmapPos = enterSubmap.find("previousSubmap = g_pKeybindManager->getCurrentSubmap().name;");
    const auto firstSubmapRef   = enterSubmap.find("if (g_submapRefs++ == 0)");
    const auto shareSubmapPos   = enterSubmap.find("g_previousSubmap = previousSubmap;", captureSubmapPos);
    const auto enterSubmapPos   = enterSubmap.find("Config::Actions::setSubmap(\"hyprexpo\");");
    const auto activateGuardPos = enterSubmap.find("submapActive = true;");
    expect(captureSubmapPos != std::string::npos, "submap entry captures the exact active Hyprland submap");
    expect(firstSubmapRef != std::string::npos && firstSubmapRef < captureSubmapPos,
           "only the first overview in a multi-monitor session captures the active submap");
    expect(shareSubmapPos != std::string::npos && captureSubmapPos < shareSubmapPos,
           "the first overview publishes the captured submap for the whole overview session");
    expect(enterSubmapPos != std::string::npos, "submap entry switches to the hyprexpo navigation submap");
    expect(activateGuardPos != std::string::npos, "submap entry marks the navigation submap active");
    expect(captureSubmapPos < enterSubmapPos, "active submap capture happens before entering hyprexpo");
    expect(enterSubmapPos < activateGuardPos, "submap-active guard is set only after entering hyprexpo");

    const auto resetSubmap = extractFunction(interactionSource, "void COverview::resetSubmapIfNeeded() {");
    expect(!resetSubmap.empty(), "keyboard navigation submap reset function exists");
    const auto lastSubmapRef    = resetSubmap.find("if (--g_submapRefs <= 0)");
    const auto sharedRestorePos = resetSubmap.find("previousSubmap = g_previousSubmap;");
    const auto restoreSubmapPos = resetSubmap.find("Config::Actions::setSubmap(previousSubmap);");
    const auto clearGuardPos    = resetSubmap.find("submapActive = false;");
    expect(lastSubmapRef != std::string::npos && sharedRestorePos != std::string::npos && lastSubmapRef < sharedRestorePos,
           "only the last overview restores the session-global captured submap");
    expect(restoreSubmapPos != std::string::npos, "submap reset restores the exact captured submap");
    expect(resetSubmap.find("Config::Actions::setSubmap(\"reset\");") == std::string::npos,
           "submap reset never hardcodes Hyprland's default submap");
    expect(clearGuardPos != std::string::npos, "submap reset clears the active guard");
    expect(sharedRestorePos < restoreSubmapPos && restoreSubmapPos < clearGuardPos,
           "shared captured submap restoration happens before clearing the active guard");

    const auto numberSelection = extractFunction(interactionSource, "bool COverview::onKbSelectNumber(int num) {");
    expect(!numberSelection.empty(), "workspace-number dispatcher selection function exists");
    expect(numberSelection.find("selectWorkspaceByID(num)") != std::string::npos,
           "kb_selectn remains workspace-ID based");
    expect(numberSelection.find("number_key_mode") == std::string::npos && numberSelection.find("numberKeyToVisibleIndex") == std::string::npos,
           "kb_selectn semantics do not depend on the raw number-key mode");

    const auto numberDispatcher = extractFunction(dispatchersSource, "static SDispatchResult onKbSelectNumberDispatcher(std::string arg) {");
    const auto indexDispatcher  = extractFunction(dispatchersSource, "static SDispatchResult onKbSelectIndexDispatcher(std::string arg) {");
    expect(numberDispatcher.find("if (OV->onKbSelectNumber(num))") != std::string::npos && numberDispatcher.find("closeOverviewsSelecting(OV);") != std::string::npos,
           "kb_selectn closes every overview only after successful workspace-number selection");
    expect(indexDispatcher.find("if (OV->onKbSelectToken(idx - 1))") != std::string::npos && indexDispatcher.find("closeOverviewsSelecting(OV);") != std::string::npos,
           "kb_selecti closes every overview only after successful visible-index selection");

    const auto toggleStart = expoDispatcher.find("if (arg == \"toggle\")");
    const auto cancelStart = expoDispatcher.find("if (arg == \"cancel\")", toggleStart);
    const auto toggleBlock = toggleStart == std::string::npos || cancelStart == std::string::npos ? std::string{} : expoDispatcher.substr(toggleStart, cancelStart - toggleStart);
    expect(toggleBlock.find("closeOverviews(false);") != std::string::npos, "plain toggle close does not select a fallback workspace");

    const auto offStart = expoDispatcher.find("if (arg == \"off\" || arg == \"close\" || arg == \"disable\")");
    const auto offEnd   = expoDispatcher.find("\n    if (overviewOpen())\n        return {};", offStart);
    const auto offBlock = offStart == std::string::npos || offEnd == std::string::npos ? std::string{} : expoDispatcher.substr(offStart, offEnd - offStart);
    expect(offBlock.find("closeOverviews(false);") != std::string::npos, "plain off and close commands do not select a fallback workspace");

    const auto enableAllStart = expoDispatcher.find("if (ALL_MONITORS && (arg == \"on\" || arg == \"enable\"))");
    const auto alreadyOpen    = expoDispatcher.find("if (overviewOpen())", enableAllStart);
    expect(enableAllStart != std::string::npos && alreadyOpen != std::string::npos && enableAllStart < alreadyOpen,
           "on all and enable all fill missing monitor entries before the already-open early return");
    expect(toggleBlock.find("openOverviews(ALL_MONITORS);") != std::string::npos && toggleBlock.find("closeOverviews(false);") != std::string::npos,
           "toggle all preserves close-when-any-open semantics");

    const auto selectStart = expoDispatcher.find("if (arg == \"select\")");
    const auto bringStart  = expoDispatcher.find("if (arg == \"bring\")", selectStart);
    const auto toggleAfterBring = expoDispatcher.find("if (arg == \"toggle\")", bringStart);
    const auto selectBlock = selectStart == std::string::npos || bringStart == std::string::npos ? std::string{} : expoDispatcher.substr(selectStart, bringStart - selectStart);
    const auto bringBlock  = bringStart == std::string::npos || toggleAfterBring == std::string::npos ? std::string{} : expoDispatcher.substr(bringStart, toggleAfterBring - bringStart);
    expect(selectBlock.find("overviewForGlobalPoint(") != std::string::npos && selectBlock.find("activeOverview()") == std::string::npos,
           "pointer select resolves the overview under the cursor instead of keyboard ownership");
    expect(bringBlock.find("overviewForGlobalPoint(") != std::string::npos && bringBlock.find("activeOverview()") == std::string::npos,
           "pointer bring resolves the overview under the cursor instead of keyboard ownership");
    expect(bringBlock.find("const auto DESTINATION = OV->pMonitor.lock();") != std::string::npos &&
               bringBlock.find("bringWindowFromWorkspace(OV->selectedWorkspaceID(), DESTINATION)") != std::string::npos,
           "bring passes the cursor overview's monitor as the explicit destination");

    const auto bringWindow = extractFunction(dispatchersSource, "static SDispatchResult bringWindowFromWorkspace(int64_t sourceWorkspaceID, const PHLMONITOR& destinationMonitor) {");
    expect(!bringWindow.empty() && bringWindow.find("destinationMonitor->m_activeWorkspace") != std::string::npos,
           "bring moves onto the explicitly chosen cursor monitor workspace");
    expect(bringWindow.find("focusState ? focusState->monitor()") == std::string::npos && bringWindow.find("State::monitorState()->query()") == std::string::npos,
           "bring never recomputes its destination from keyboard focus or cursor fallback");

    const auto activeFunction = extractFunction(source, "COverview* activeOverview() {");
    expect(activeFunction.find("g_keyboardOverviewMonitor.lock()") != std::string::npos && activeFunction.find("overviewForMonitor(KEYBOARD)") != std::string::npos,
           "active overview honors a persistent explicit keyboard owner before compositor focus");
    expect(source.find("bool overviewRegistered(const COverview* overview)") != std::string::npos,
           "registry exposes exact overview liveness for delayed callbacks");

    const auto moveFocus = extractFunction(interactionSource, "bool COverview::moveFocus(int dx, int dy) {");
    const auto kbMove    = extractFunction(interactionSource, "bool COverview::onKbMoveFocus(const std::string& dir) {");
    expect(!moveFocus.empty() && moveFocus.find("return true;") != std::string::npos && moveFocus.find("return false;") != std::string::npos,
           "local focus movement reports whether it found a tile");
    expect(kbMove.find("moveOverviewFocusAcrossMonitors(this") != std::string::npos,
           "cross-monitor focus runs only after local movement fails");
    expect(source.find("Hyprexpo::selectDirectionalTile(") != std::string::npos && source.find("g_keyboardOverviewMonitor = TARGET->pMonitor;") != std::string::npos,
           "cross-monitor focus uses pure geometry and persists destination keyboard ownership");

    expect(source.find("void closeOverviewsSelecting(COverview* selecting)") != std::string::npos &&
               dispatchersSource.find("static void closeOverviewsSelecting(") == std::string::npos,
           "one registry coordinator owns selector and peer closure");
    expect(interactionSource.find("bool COverview::selectHoveredWorkspace()") != std::string::npos,
           "pointer and touch selection can distinguish success from an invalid tile");

    const auto resetDrag = extractFunction(source, "void resetOverviewDrag(");
    expect(!resetDrag.empty(), "registry provides one centralized idempotent drag reset");
    expect(resetDrag.find("transitionOverviewDrag(") != std::string::npos && resetDrag.find("\"left_ptr\"") != std::string::npos && resetDrag.find("damageMonitor") != std::string::npos,
           "central drag reset clears pure state, restores left_ptr, and damages affected live monitors");
    expect(resetDrag.find("liveOverviewMonitorKeys()") != std::string::npos,
           "central reset always supplies current registry liveness when a monitor weak reference has expired");
    const auto destroyOne = extractFunction(source, "void destroyOverview(COverview* overview) {");
    const auto destroyAll = extractFunction(source, "void destroyAllOverviews() {");
    const auto moveOwnerPos   = destroyOne.find("auto OWNER = std::move(*IT);");
    const auto eraseSlotPos   = destroyOne.find("g_overviews.erase(IT);");
    const auto destroyOwnerPos = destroyOne.find("OWNER.reset();");
    expect(destroyOne.find("resetOverviewDrag(") < moveOwnerPos, "single-overview destruction resets drag before unregistering ownership");
    expect(moveOwnerPos != std::string::npos && eraseSlotPos != std::string::npos && destroyOwnerPos != std::string::npos && moveOwnerPos < eraseSlotPos && eraseSlotPos < destroyOwnerPos,
           "single-overview teardown unregisters the moved owner before its cursor-restoring destructor runs");
    const auto swapRegistryPos = destroyAll.find("OWNERS.swap(g_overviews);");
    const auto clearOwnersPos  = destroyAll.find("OWNERS.clear();");
    expect(destroyAll.find("resetOverviewDrag(") < swapRegistryPos && swapRegistryPos < clearOwnersPos,
           "all-overview teardown empties the registry before any owner destructor runs");

    const auto byAnimVar  = extractFunction(source, "COverview* overviewForAnimVar(");
    const auto byMonitor  = extractFunction(source, "COverview* overviewForMonitor(");
    const auto byKey      = extractFunction(source, "COverview* overviewForMonitorKey(");
    const auto byPoint    = extractFunction(source, "COverview* overviewForGlobalPoint(");
    expect(byAnimVar.find("if (!OV)") < byAnimVar.find("OV->ownsAnimVar("), "animation-owner lookup skips null registry slots before dereference");
    expect(byMonitor.find("if (!OV)") < byMonitor.find("OV->pMonitor"), "monitor lookup skips null registry slots before dereference");
    expect(byKey.find("if (!OV)") < byKey.find("OV->pMonitor"), "monitor-key lookup skips null registry slots before dereference");
    expect(byPoint.find("if (!OV)") < byPoint.find("OV->pMonitor"), "point lookup skips null registry slots before dereference");
    expect(source.find("if (OV)\n            snapshot.push_back(OV.get());") != std::string::npos,
           "safe registry snapshots never publish null overview pointers");
    expect(interactionSource.find("activeOverview() != this") == std::string::npos,
           "settle timer liveness never depends on whichever overview currently owns keyboard input");

    const auto redrawTimer = extractFunction(interactionSource, "void COverview::redrawDraggedWorkspace(int64_t workspaceID) {");
    expect(redrawTimer.find("[this]") == std::string::npos,
           "settle timer callbacks never capture a raw overview pointer");
    expect(redrawTimer.find("const auto OVERVIEWKEY = overviewMonitorKey(") != std::string::npos && redrawTimer.find("[OVERVIEWKEY]") != std::string::npos &&
               redrawTimer.find("overviewForMonitorKey(OVERVIEWKEY)") != std::string::npos,
           "settle timer callbacks re-resolve their overview by stable monitor key on every tick");
    expect(redrawTimer.find("OVERVIEW->redrawSettleTimer.get() != self.get()") != std::string::npos,
           "a timer cannot mutate a replacement overview registered on the same monitor");
    expect(redrawTimer.find("settlingRedrawWorkspaceIDs") != std::string::npos && redrawTimer.find("OVERVIEW->tileForWorkspaceID(workspaceID)") != std::string::npos,
           "every settle tick re-resolves the queued workspace identity to its current tile");

    const auto overviewDestructor = extractFunction(source, "COverview::~COverview() {");
    const auto cancelTimerPos     = overviewDestructor.find("redrawSettleTimer->cancel();");
    const auto restoreCursorPos   = overviewDestructor.find("Pointer::Cursor::overrideController->unsetOverride");
    expect(cancelTimerPos != std::string::npos && restoreCursorPos != std::string::npos && cancelTimerPos < restoreCursorPos,
           "overview teardown cancels and detaches settle timers before cursor restoration can re-enter callbacks");

    const auto headerSource = readFile("Overview.hpp");
    expect(headerSource.find("dragStartLocal") == std::string::npos && headerSource.find("dragSourceID") == std::string::npos && headerSource.find("dragWindow") == std::string::npos,
           "per-overview drag ownership fields are removed in favor of the registry session");
    const auto beginDrag  = extractFunction(interactionSource, "void COverview::beginWindowDrag() {");
    const auto updateDrag = extractFunction(interactionSource, "void COverview::updateWindowDrag() {");
    const auto finishDrag = extractFunction(interactionSource, "bool COverview::finishWindowDrag() {");
    expect(beginDrag.find("hitTestGlobalTile(") != std::string::npos && beginDrag.find("EOverviewDragEventType::Press") != std::string::npos,
           "drag press ownership comes from pure global hit testing and the shared coordinator");
    expect(updateDrag.find("sourceMonitorKey") != std::string::npos && updateDrag.find("EOverviewDragEventType::Move") != std::string::npos &&
               updateDrag.find("EOverviewDragEventType::Target") != std::string::npos,
           "only the shared source owner advances move and target transitions");
    expect(finishDrag.find("EOverviewDragEventType::Release") != std::string::npos && finishDrag.find("resetOverviewDrag(") != std::string::npos,
           "release consumes the pure coordinator result and terminates through centralized reset");
    expect(finishDrag.find("TARGETWS->m_monitor.lock() != TARGETMON") != std::string::npos,
           "release rejects a target workspace that does not belong to the target overview monitor");
    expect(countOccurrences(finishDrag, "moveWindowToWorkspace(") == 1,
           "validated release moves the window exactly once");
    const auto moveWindowPos    = finishDrag.find("moveWindowToWorkspace(");
    const auto settleWindowPos  = finishDrag.find("settleWorkspaceMoveAnimation(", moveWindowPos);
    const auto sourceCapturePos = finishDrag.find("SOURCEOV->redrawDraggedWorkspace(SOURCEWORKSPACEID)", settleWindowPos);
    const auto targetCapturePos = finishDrag.find("TARGETOV->redrawDraggedWorkspace(TARGETWORKSPACEID)", settleWindowPos);
    expect(moveWindowPos != std::string::npos && settleWindowPos != std::string::npos && sourceCapturePos != std::string::npos && targetCapturePos != std::string::npos &&
               moveWindowPos < settleWindowPos && settleWindowPos < sourceCapturePos && settleWindowPos < targetCapturePos,
           "source and destination workspace identities queue independent recaptures only after move animations settle");
    expect(headerSource.find("settlingRedrawWorkspaceIDs") != std::string::npos && headerSource.find("settlingRedrawIDs") == std::string::npos,
           "settled recapture state stores workspace identities instead of stale tile indices");
    const auto flushRedraws = extractFunction(interactionSource, "void COverview::flushQueuedRedraws() {");
    expect(flushRedraws.find("MON->scheduleFrame();") != std::string::npos,
           "every completed framebuffer recapture schedules a compositor frame");
    expect(finishDrag.find("\"left_ptr\"") == std::string::npos,
           "release cannot restore the cursor outside centralized reset");

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
    const auto closeOverview = extractFunction(renderSource, "void COverview::close(bool switchToSelection) {");
    expect(!closeOverview.empty(), "overview close function exists");
    expect(closeOverview.find("resetSubmapIfNeeded();") != std::string::npos,
           "normal overview close restores the captured submap");

    // An anchored grid (workspace_method "<output> first N") lays out
    // max_workspace slots whether or not those workspaces exist. Selecting a
    // tile for one that has never been opened must create it: changeWorkspace()
    // cannot, and the selection used to silently do nothing.
    const auto ensureTileWsPos = closeOverview.find("ensureWorkspaceForTile(SAFEID)");
    const auto changeWsPos     = closeOverview.find("MON->changeWorkspace(");
    expect(ensureTileWsPos != std::string::npos,
           "selection resolves the tile's workspace through the creating helper");
    expect(changeWsPos != std::string::npos && ensureTileWsPos < changeWsPos,
           "the tile's workspace is created before the monitor is switched to it");
    expect(closeOverview.find("if (TILE.workspaceID != WORKSPACE_INVALID)") != std::string::npos,
           "a WORKSPACE_INVALID tile still falls back to the next empty workspace");

    // overviewDestructor is already extracted above, next to the settle-timer checks.
    expect(!overviewDestructor.empty(), "overview destructor exists");
    expect(overviewDestructor.find("resetSubmapIfNeeded();") != std::string::npos,
           "overview teardown restores the captured submap");

    const auto fullRender = extractFunction(renderSource, "void COverview::fullRender(");
    expect(!fullRender.empty(), "overview fullRender function exists");
    const auto closeFunction = extractFunction(renderSource, "void COverview::close(bool switchToSelection) {");
    expect(closeFunction.find("MON->changeWorkspace(") != std::string::npos && closeFunction.find("Config::Actions::changeWorkspace(") == std::string::npos,
           "selection switches the workspace through the overview's owning monitor");
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
    expect(fullRender.find("g_overviewDrag.state.sourceMonitorKey") != std::string::npos && fullRender.find("g_overviewDrag.state.targetMonitorKey") != std::string::npos,
           "source and destination overview renders query the one shared drag session");
    expect(fullRender.find(".pointerLocal") != std::string::npos && fullRender.find("MON->m_scale") != std::string::npos,
           "destination proxy geometry uses target-local pointer coordinates and target monitor scale");
    expect(source.find("Config::mgr()->getConfigValue(name).setByUser") != std::string::npos,
           "config compatibility exposes explicit-setting metadata for both config providers");

    expect(dispatchersSource.find("HyprlandAPI::getConfigValue") == std::string::npos,
           "gesture config avoids the legacy hyprlang getter, which is null under CONFIG_LUA");

    const auto gestureSync = extractFunction(dispatchersSource, "void syncExpoGestureFromConfig(");
    expect(!gestureSync.empty(), "syncExpoGestureFromConfig exists");
    expect(gestureSync.find("g_unloading || g_gestureRegistrationDisabled") != std::string::npos, "gesture sync bails out while the plugin is unloading");
    expect(gestureSync.find("registerExpoGesture(FINGERS, DIR, \"expo\"") != std::string::npos,
           "plain config synchronization remains expo-only");

    const auto gestureRegister = extractFunction(dispatchersSource, "static SDispatchResult registerExpoGesture(");
    expect(!gestureRegister.empty(), "registerExpoGesture definition exists");
    expect(gestureRegister.find("g_unloading || g_gestureRegistrationDisabled") != std::string::npos,
           "every gesture registration path, including the Lua helper, is fenced during unload");
    expect(gestureRegister.find("action == \"expo\"") != std::string::npos &&
               gestureRegister.find("makeUnique<CExpoGesture>(EExpoGestureAction::Expo)") != std::string::npos,
           "Lua expo registration constructs an explicit expo gesture");
    expect(gestureRegister.find("action == \"cancel\"") != std::string::npos &&
               gestureRegister.find("makeUnique<CExpoGesture>(EExpoGestureAction::Cancel)") != std::string::npos,
           "Lua cancel registration constructs an explicit cancel gesture");
    expect(gestureRegister.find("expected expo|cancel|unset") != std::string::npos,
           "invalid Lua gesture actions report the complete accepted set");

    expect(gestureHeader.find("enum class EExpoGestureAction") != std::string::npos,
           "gesture action modes use an explicit enum");
    expect(gestureHeader.find("CExpoGesture(EExpoGestureAction action)") != std::string::npos,
           "gesture construction requires an explicit action");
    expect(gestureHeader.find("\n    CExpoGesture()") == std::string::npos,
           "gesture construction cannot silently default an action");
    expect(gestureHeader.find("const EExpoGestureAction m_action") != std::string::npos,
           "each gesture retains an immutable action mode");
    expect(gestureHeader.find("COverview*    overview() const;") != std::string::npos &&
               gestureHeader.find("PHLMONITORREF m_monitor;") != std::string::npos,
           "each gesture retains an origin-monitor lookup instead of a singleton overview pointer");

    const auto gestureBegin = extractFunction(gestureSource, "void CExpoGesture::begin(");
    expect(!gestureBegin.empty(), "gesture begin function exists");
    const auto monitorQueryStart = gestureBegin.find("const auto monitor");
    const auto monitorCapture    = gestureBegin.find("m_monitor = monitor;", monitorQueryStart);
    const auto overviewResolve   = gestureBegin.find("auto* const OV = overview();", monitorCapture);
    const auto cancelBeginStart = gestureBegin.find("if (m_action == EExpoGestureAction::Cancel)");
    const auto expoBeginStart   = gestureBegin.find("\n    if (!OV)\n", cancelBeginStart);
    const auto cancelBeginBlock = cancelBeginStart == std::string::npos || expoBeginStart == std::string::npos ? std::string{} :
                                                                                                               gestureBegin.substr(cancelBeginStart, expoBeginStart - cancelBeginStart);
    expect(monitorQueryStart != std::string::npos && monitorCapture != std::string::npos && overviewResolve != std::string::npos && cancelBeginStart != std::string::npos &&
               monitorQueryStart < monitorCapture && monitorCapture < overviewResolve && overviewResolve < cancelBeginStart,
           "gesture begin captures and resolves its origin monitor before either action runs");
    expect(cancelBeginBlock.find("!OV || OV->closeCommitted()") != std::string::npos,
           "cancel begin is inert without a mutable overview");
    expect(cancelBeginBlock.find("OV->beginCancelSwipe()") != std::string::npos,
           "cancel begin delegates target reset and interactive closing to the overview");
    expect(cancelBeginBlock.find("selectHoveredWorkspace") == std::string::npos && cancelBeginBlock.find("createOverview") == std::string::npos,
           "cancel begin neither selects a hovered workspace nor creates an overview");
    expect(overviewHeader.find("void beginCancelSwipe();") != std::string::npos,
           "overview exposes an owned cancel-begin transition");
    const auto cancelSwipeBegin = extractFunction(interactionSource, "void COverview::beginCancelSwipe(");
    expect(!cancelSwipeBegin.empty(), "overview cancel-begin transition exists");
    const auto resetCancelTarget = cancelSwipeBegin.find("closeOnID = openedID");
    const auto startCancelClose  = cancelSwipeBegin.find("closing", resetCancelTarget);
    const auto enableCancelClose = cancelSwipeBegin.find("= true", startCancelClose);
    expect(resetCancelTarget != std::string::npos && startCancelClose != std::string::npos && enableCancelClose != std::string::npos && resetCancelTarget < startCancelClose,
           "cancel begin replaces any aborted expo target with the opening workspace before closing");
    const auto expoBeginBlock = expoBeginStart == std::string::npos ? std::string{} : gestureBegin.substr(expoBeginStart);
    expect(expoBeginBlock.find("createOverview(monitor, true)") != std::string::npos &&
               expoBeginBlock.find("OV->selectHoveredWorkspace()") != std::string::npos &&
               expoBeginBlock.find("OV->setClosing(true)") != std::string::npos,
           "expo begin retains open and hovered-selection close behavior");

    const auto gestureOverview = extractFunction(gestureSource, "COverview* CExpoGesture::overview() const {");
    expect(gestureOverview.find("overviewForMonitor(m_monitor.lock())") != std::string::npos,
           "gesture callbacks re-resolve only the overview owned by the captured origin monitor");

    const auto gestureUpdate = extractFunction(gestureSource, "void CExpoGesture::update(");
    expect(gestureUpdate.find("auto* const OV = overview();") != std::string::npos &&
               gestureUpdate.find("!OV || OV->closeCommitted()") != std::string::npos,
           "gesture updates re-resolve the origin overview and ignore committed closes");
    const auto firstUpdateGuard = gestureUpdate.find("if (m_firstUpdate)");
    const auto accumulateDelta  = gestureUpdate.find("m_lastDelta += distance(e);");
    expect(firstUpdateGuard != std::string::npos && accumulateDelta != std::string::npos && firstUpdateGuard < accumulateDelta,
           "gesture updates discard the first compositor delta before accumulating movement");

    const auto gestureEnd = extractFunction(gestureSource, "void CExpoGesture::end(");
    expect(!gestureEnd.empty(), "gesture end function exists");
    expect(gestureEnd.find("auto* const OV = overview();") != std::string::npos &&
               gestureEnd.find("!OV || OV->closeCommitted()") != std::string::npos,
           "gesture end re-resolves the origin overview and ignores committed closes");
    expect(gestureEnd.find("OV->setClosing(false)") != std::string::npos,
           "gesture end clears transient closing before threshold evaluation");
    expect(gestureEnd.find("OV->onSwipeEnd(m_action == EExpoGestureAction::Expo)") != std::string::npos,
           "gesture completion selects only for the expo action");
    expect(gestureEnd.find("if (auto* const STILL_ALIVE = overview())") != std::string::npos &&
               gestureEnd.find("STILL_ALIVE->resetSwipe()") != std::string::npos,
           "gesture completion re-resolves the origin overview before its post-close reset");

    const auto swipeEnd = extractFunction(interactionSource, "void COverview::onSwipeEnd(");
    expect(!swipeEnd.empty(), "overview swipe-end function exists");
    expect(swipeEnd.find("void COverview::onSwipeEnd(bool switchToSelection)") != std::string::npos,
           "overview swipe completion accepts the selection decision");
    const auto degenerateSpanStart = swipeEnd.find("if (std::abs(span.x) <= 1e-6)");
    const auto thresholdStart = swipeEnd.find("if (PERC > 0.5)", degenerateSpanStart);
    const auto incompleteStart = swipeEnd.find("*size = MON->m_size", thresholdStart);
    const auto degenerateSpanBlock = degenerateSpanStart == std::string::npos || thresholdStart == std::string::npos ? std::string{} :
                                                                                                                       swipeEnd.substr(degenerateSpanStart, thresholdStart - degenerateSpanStart);
    const auto thresholdBlock = thresholdStart == std::string::npos || incompleteStart == std::string::npos ? std::string{} :
                                                                                                               swipeEnd.substr(thresholdStart, incompleteStart - thresholdStart);
    const auto incompleteBlock = incompleteStart == std::string::npos ? std::string{} : swipeEnd.substr(incompleteStart);
    expect(degenerateSpanBlock.find("close(switchToSelection)") != std::string::npos &&
               thresholdBlock.find("close(switchToSelection)") != std::string::npos,
           "completed swipe paths forward the action-specific selection choice");
    expect(incompleteBlock.find("*pos  = {0, 0}") != std::string::npos &&
               incompleteBlock.find("swipeWasCommenced = true") != std::string::npos &&
               incompleteBlock.find("m_isSwiping       = false") != std::string::npos &&
               incompleteBlock.find("close(") == std::string::npos,
           "incomplete swipe still resets animation state and leaves the overview open");

    expect(source.find("#include <hyprland/src/desktop/view/Window.hpp>") != std::string::npos &&
               source.find("#include <hyprland/src/desktop/view/window/Window.hpp>") == std::string::npos &&
               source.find("#include <hyprland/src/desktop/view/window/WindowPresentation.hpp>") == std::string::npos,
           "overview capture uses the tagged Hyprland release window header");
    expect(source.find("window->m_isMapped") != std::string::npos &&
               source.find("window->m_pinned") != std::string::npos &&
               source.find("window->alpha(") != std::string::npos &&
               source.find("window->m_monitorMovedFrom != -1") != std::string::npos &&
               source.find("window->m_monitorMovedFrom                                      = -1;") != std::string::npos,
           "overview capture uses release mapped, pinned, alpha, and moved-monitor APIs");
    expect(source.find("window->mapped()") == std::string::npos && source.find("WINDOW_STATE_PINNED") == std::string::npos &&
               source.find("->presentation()") == std::string::npos,
           "overview capture does not reintroduce git-only Hyprland window APIs");

    const auto applyPinnedState = extractFunction(source, "std::vector<SPinnedWindowPreviewState> applyPinnedWindowPreviewState(");
    const auto restorePinnedState = extractFunction(source, "void restorePinnedWindowPreviewState(");
    expect(applyPinnedState.find(".pinned = window->m_pinned") != std::string::npos &&
               applyPinnedState.find("window->m_pinned = false") != std::string::npos &&
               applyPinnedState.find("window->m_workspace.reset()") != std::string::npos,
           "pinned preview suppression saves and clears pinned state and temporarily detaches the workspace");
    expect(restorePinnedState.find("state.window->m_workspace = state.workspace") != std::string::npos &&
               restorePinnedState.find("state.window->m_pinned    = state.pinned") != std::string::npos,
           "pinned preview suppression restores workspace ownership and the saved pinned state");

    expect(dispatchersSource.find("uint32_t modMask = 0") != std::string::npos &&
               dispatchersSource.find("g_pKeybindManager->stringToModMask(mods)") != std::string::npos &&
               dispatchersSource.find("Keybinds::modMaskFromString") == std::string::npos,
           "gesture registration uses the tagged Hyprland release modifier parser and mask type");
    expect(interactionSource.find("#include <hyprland/src/managers/KeybindManager.hpp>") != std::string::npos &&
               interactionSource.find("g_pKeybindManager->getCurrentSubmap().name") != std::string::npos &&
               interactionSource.find("#include <hyprland/src/keybinds/Manager.hpp>") == std::string::npos &&
               interactionSource.find("Keybinds::mgr()") == std::string::npos,
           "submap ownership uses the tagged Hyprland release keybind manager API");

    const auto exitFunction = extractFunction(mainSource, "APICALL EXPORT void PLUGIN_EXIT(");
    expect(!exitFunction.empty(), "PLUGIN_EXIT exists");
    const auto disablePos = exitFunction.find("disableExpoGestureRegistration();");
    const auto reloadPos  = exitFunction.find("Config::mgr()->reload();");
    expect(disablePos != std::string::npos, "PLUGIN_EXIT disables gesture registration");
    expect(reloadPos != std::string::npos, "PLUGIN_EXIT reloads the config to clear registered gestures");
    expect(disablePos < reloadPos, "the registration fence is set before the teardown reload re-runs the config");

    expect(mainSource.find("config.reloaded.listen") != std::string::npos, "the gesture is re-applied after every config reload");

    const auto multiMonitorDocs = readFile("docs/guides/multi-monitor.md");
    const auto keyboardDocs     = readFile("docs/configuration/keyboard.md");
    const auto dispatcherDocs   = readFile("docs/reference/dispatchers.md");
    expect(multiMonitorDocs.find("global logical geometry") != std::string::npos && multiMonitorDocs.find("target monitor") != std::string::npos &&
               multiMonitorDocs.find("not supported yet") == std::string::npos,
           "multi-monitor guide documents geometry-based navigation and target-local drag/drop");
    expect(keyboardDocs.find("local wrapping takes precedence") != std::string::npos && keyboardDocs.find("keyboard owner") != std::string::npos,
           "keyboard guide documents explicit ownership and local-wrap precedence");
    expect(dispatcherDocs.find("fills any missing monitor overviews") != std::string::npos && dispatcherDocs.find("closes every open overview") != std::string::npos,
           "dispatcher reference distinguishes idempotent enabling from toggle close and peer dismissal");
    expect(dispatcherDocs.find("monitor under the pointer") != std::string::npos,
           "dispatcher reference identifies cursor ownership for pointer select and bring");

    if (failures != 0)
        return 1;

    std::cout << "OverviewSourceTests passed\n";
    return 0;
}
