#include "ScrollingInputState.hpp"

#include "ScrollingRequestId.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <string_view>

namespace Hyprexpo::Scrolling {

namespace {

constexpr uint32_t PRIMARY_BUTTON = 0x111;

bool finitePoint(SPoint point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

bool sameHit(const SHitResult& lhs, const SHitResult& rhs) {
    return lhs.kind == rhs.kind && lhs.workspaceID == rhs.workspaceID && lhs.targetToken == rhs.targetToken && lhs.columnIndex == rhs.columnIndex && lhs.rowIndex == rhs.rowIndex;
}

bool exactTarget(const SHitResult& hit) {
    return hit.kind == EHitKind::Target && hit.targetToken != 0;
}

SHitResult targetHitAt(const SInputContext& context, SPoint globalPoint) {
    if (!context.scene)
        return {};
    const auto local = monitorLocalPoint(globalPoint, context.monitor);
    if (!local)
        return {};
    return hitTest(*context.scene, *local, context.pan);
}

bool targetExists(const SScene& scene, const SHitResult& target) {
    if (!exactTarget(target))
        return false;
    return std::ranges::any_of(scene.targets, [&](const auto& item) { return item.workspaceID == target.workspaceID && item.token == target.targetToken; });
}

SDropSource dropSourceFor(const SScene& scene, const SHitResult& hit) {
    const auto count = std::ranges::count_if(scene.targets, [&](const auto& target) {
        return target.workspaceID == hit.workspaceID && target.nativeColumnIndex == hit.columnIndex;
    });
    return {.workspaceID = hit.workspaceID, .columnIndex = hit.columnIndex, .rowIndex = hit.rowIndex, .sourceColumnWillDisappear = count == 1};
}

std::optional<SDropIntent> dropAt(const SInputState& state, const SInputContext& context, SPoint globalPoint) {
    if (!context.scene)
        return std::nullopt;
    const auto local = monitorLocalPoint(globalPoint, context.monitor);
    if (!local)
        return std::nullopt;
    return resolveDrop(*context.scene, state.dragSource, *local, context.pan);
}

SInputState idleState() {
    return {};
}

bool ownsTarget(const SInputState& state) {
    return state.mode == EInputMode::MousePressPending || state.mode == EInputMode::TouchPressPending || state.mode == EInputMode::WindowDrag;
}

const char* inputModeName(EInputMode mode) {
    switch (mode) {
        case EInputMode::Idle: return "Idle";
        case EInputMode::MousePressPending: return "MousePressPending";
        case EInputMode::TouchPressPending: return "TouchPressPending";
        case EInputMode::CanvasPan: return "CanvasPan";
        case EInputMode::WindowDrag: return "WindowDrag";
    }
    return "Idle";
}

const char* dropKindName(EDropKind kind) {
    switch (kind) {
        case EDropKind::Invalid: return "Invalid";
        case EDropKind::NoOp: return "NoOp";
        case EDropKind::ExistingColumn: return "ExistingColumn";
        case EDropKind::NewColumnBefore: return "NewColumnBefore";
        case EDropKind::NewColumnAfter: return "NewColumnAfter";
        case EDropKind::CrossWorkspace: return "CrossWorkspace";
        case EDropKind::MixedFallback: return "MixedFallback";
        case EDropKind::TerminalWorkspace: return "TerminalWorkspace";
    }
    return "Invalid";
}

std::vector<std::string> splitInputFields(std::string_view value, char delimiter) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find(delimiter, start);
        fields.emplace_back(value.substr(start, end == std::string_view::npos ? value.size() - start : end - start));
        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }
    return fields;
}

template <typename T>
bool parseInputNumber(const std::string& value, T& out) {
    const auto result = std::from_chars(value.data(), value.data() + value.size(), out);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size();
}

std::optional<EResetReason> parseResetReason(const std::string& value) {
    if (value == "cancel")
        return EResetReason::Cancel;
    if (value == "refresh")
        return EResetReason::Refresh;
    if (value == "close")
        return EResetReason::Close;
    if (value == "teardown")
        return EResetReason::Teardown;
    return std::nullopt;
}

SInputTransition resetOwned(const SInputState& state, bool consume) {
    auto reset = resetInput(state, EResetReason::Cancel);
    reset.effects.consume = consume;
    return reset;
}

}

SParsedInputSequence parseInputSequence(const std::string& sequence, size_t maxEvents) {
    SParsedInputSequence parsed;
    const auto specs = splitInputFields(sequence, '|');
    if (specs.size() < 2 || !validRequestID(specs.front())) {
        parsed.error = "expected requestId followed by one or more strict input events";
        return parsed;
    }
    if (specs.size() - 1 > maxEvents) {
        parsed.error = "input sequence exceeds the event limit";
        return parsed;
    }
    parsed.requestId = specs.front();
    parsed.steps.reserve(specs.size() - 1);
    for (size_t index = 1; index < specs.size(); ++index) {
        const auto fields = splitInputFields(specs[index], ':');
        if (fields.empty() || fields.front().empty()) {
            parsed.error = "input event name is empty";
            return parsed;
        }
        if (fields.front() == "reset") {
            if (fields.size() != 2) {
                parsed.error = "reset event expects exactly one reason";
                return parsed;
            }
            const auto reason = parseResetReason(fields[1]);
            if (!reason) {
                parsed.error = "reset reason must be cancel|refresh|close|teardown";
                return parsed;
            }
            parsed.steps.push_back({.event = std::nullopt, .reset = *reason});
            continue;
        }

        SInputEvent event;
        auto parsePoint = [&](size_t xIndex, size_t yIndex) {
            return fields.size() > yIndex && parseInputNumber(fields[xIndex], event.globalLogicalPoint.x) && parseInputNumber(fields[yIndex], event.globalLogicalPoint.y) &&
                std::isfinite(event.globalLogicalPoint.x) && std::isfinite(event.globalLogicalPoint.y);
        };
        bool valid = false;
        if (fields.front() == "mouse_move" && fields.size() == 3 && parsePoint(1, 2)) {
            event.kind = EInputKind::MouseMove;
            valid = true;
        } else if (fields.front() == "mouse_button" && fields.size() == 5 && parsePoint(1, 2)) {
            int pressed = 0;
            if (parseInputNumber(fields[3], event.button) && parseInputNumber(fields[4], pressed) && (pressed == 0 || pressed == 1)) {
                event.kind = EInputKind::MouseButton;
                event.pressed = pressed == 1;
                valid = true;
            }
        } else if (fields.front() == "mouse_axis" && fields.size() == 4 && parsePoint(1, 2) && parseInputNumber(fields[3], event.axisDelta) && std::isfinite(event.axisDelta)) {
            event.kind = EInputKind::MouseAxis;
            valid = true;
        } else if ((fields.front() == "touch_down" || fields.front() == "touch_motion") && fields.size() == 4 && parseInputNumber(fields[1], event.touchId) && parsePoint(2, 3)) {
            event.kind = fields.front() == "touch_down" ? EInputKind::TouchDown : EInputKind::TouchMotion;
            valid = true;
        } else if ((fields.front() == "touch_up" || fields.front() == "touch_cancel") && fields.size() == 2 && parseInputNumber(fields[1], event.touchId)) {
            event.kind = fields.front() == "touch_up" ? EInputKind::TouchUp : EInputKind::TouchCancel;
            valid = true;
        }
        if (!valid) {
            parsed.error = "invalid input event schema at index " + std::to_string(index - 1);
            return parsed;
        }
        parsed.steps.push_back({.event = event, .reset = std::nullopt});
    }
    parsed.valid = true;
    return parsed;
}

std::string inputDiagnosticJson(const std::string& requestId, const std::vector<SInputDiagnosticRecord>& records, const SInputState& finalState, bool hasDropIntent) {
    std::string eventsJson = "[";
    for (size_t index = 0; index < records.size(); ++index) {
        if (index > 0)
            eventsJson += ',';
        const auto& record = records[index];
        const auto& effects = record.effects;
        const auto drop = effects.dropIntent ? dropKindName(effects.dropIntent->kind) : "None";
        const auto selectedWorkspace = effects.selection ? effects.selection->workspaceID : 0;
        const auto selectedTarget = effects.selection ? effects.selection->targetToken : 0;
        eventsJson += std::format("{{\"index\":{},\"state\":\"{}\",\"consume\":{},\"hoverChanged\":{},\"clearHover\":{},\"pan\":{},\"panDelta\":{},"
                                  "\"select\":{},\"selectWorkspaceId\":{},\"selectTargetToken\":{},\"beginDrag\":{},\"updateDrag\":{},\"finishDrag\":{},\"cancelDrag\":{},\"resetOwnership\":{},\"drop\":\"{}\"}}",
                                  index, inputModeName(record.state.mode), effects.consume, effects.hoverChanged, effects.clearHover, record.pan, effects.panDelta,
                                  effects.selection.has_value(), selectedWorkspace, selectedTarget, effects.beginDrag, effects.updateDrag, effects.finishDrag, effects.cancelDrag, effects.resetOwnership, drop);
    }
    eventsJson += ']';
    return std::format("{{\"requestId\":\"{}\",\"events\":{},\"finalState\":\"{}\",\"owningTouchId\":{},\"hasDropIntent\":{}}}",
                       requestId, eventsJson, inputModeName(finalState.mode), finalState.owningTouchId, hasDropIntent);
}

std::optional<SPoint> monitorLocalPoint(SPoint globalLogicalPoint, const SMonitorGeometry& monitor) {
    if (!finitePoint(globalLogicalPoint) || !finitePoint(monitor.position) || !std::isfinite(monitor.logicalSize.w) || !std::isfinite(monitor.logicalSize.h) ||
        monitor.logicalSize.w <= 0.0 || monitor.logicalSize.h <= 0.0 || !std::isfinite(monitor.scale) || monitor.scale <= 0.0)
        return std::nullopt;
    const SPoint local{globalLogicalPoint.x - monitor.position.x, globalLogicalPoint.y - monitor.position.y};
    if (local.x < 0.0 || local.y < 0.0 || local.x >= monitor.logicalSize.w || local.y >= monitor.logicalSize.h)
        return std::nullopt;
    return local;
}

std::optional<SPoint> touchToGlobalLogical(SPoint normalizedPoint, const SMonitorGeometry& monitor) {
    if (!finitePoint(normalizedPoint) || normalizedPoint.x < 0.0 || normalizedPoint.y < 0.0 || normalizedPoint.x > 1.0 || normalizedPoint.y > 1.0 ||
        !monitorLocalPoint(monitor.position, monitor))
        return std::nullopt;

    SPoint transformed = normalizedPoint;
    switch (monitor.transform) {
        case EOutputTransform::Normal: break;
        case EOutputTransform::Rotate90: transformed = {1.0 - normalizedPoint.y, normalizedPoint.x}; break;
        case EOutputTransform::Rotate180: transformed = {1.0 - normalizedPoint.x, 1.0 - normalizedPoint.y}; break;
        case EOutputTransform::Rotate270: transformed = {normalizedPoint.y, 1.0 - normalizedPoint.x}; break;
        case EOutputTransform::Flipped: transformed = {1.0 - normalizedPoint.x, normalizedPoint.y}; break;
        case EOutputTransform::Flipped90: transformed = {normalizedPoint.y, normalizedPoint.x}; break;
        case EOutputTransform::Flipped180: transformed = {normalizedPoint.x, 1.0 - normalizedPoint.y}; break;
        case EOutputTransform::Flipped270: transformed = {1.0 - normalizedPoint.y, 1.0 - normalizedPoint.x}; break;
    }
    return SPoint{monitor.position.x + transformed.x * monitor.logicalSize.w, monitor.position.y + transformed.y * monitor.logicalSize.h};
}

SInputTransition resetInput(const SInputState& state, EResetReason) {
    SInputTransition result{.state = idleState(), .effects = {}};
    result.effects.clearHover = state.hover.kind != EHitKind::Outside;
    result.effects.cancelDrag = state.mode == EInputMode::MousePressPending || state.mode == EInputMode::TouchPressPending || state.mode == EInputMode::WindowDrag;
    result.effects.resetOwnership = true;
    return result;
}

SInputTransition transitionInput(const SInputState& state, const SInputEvent& event, const SInputContext& context) {
    SInputTransition result{.state = state, .effects = {}};
    if (!context.scene || !context.scene->valid || !std::isfinite(context.pan) || !std::isfinite(context.viewportHeight) || context.viewportHeight <= 0.0 ||
        !std::isfinite(context.dragThreshold) || context.dragThreshold <= 0.0)
        return result;

    const bool matchingTouch = state.owner == EInputOwner::Touch && event.touchId == state.owningTouchId;
    if (ownsTarget(state) && !targetExists(*context.scene, state.pressed)) {
        const bool ownedEvent = state.owner == EInputOwner::Mouse ? event.kind == EInputKind::MouseMove || event.kind == EInputKind::MouseButton || event.kind == EInputKind::MouseAxis : matchingTouch;
        if (ownedEvent) {
            result = resetInput(state, EResetReason::StaleTarget);
            result.effects.consume = true;
        }
        return result;
    }

    if (event.kind == EInputKind::MouseMove) {
        if (state.owner == EInputOwner::Touch)
            return result;
        if (state.mode == EInputMode::Idle) {
            const auto hit = targetHitAt(context, event.globalLogicalPoint);
            const SHitResult hover = exactTarget(hit) ? hit : SHitResult{};
            result.effects.hoverChanged = !sameHit(state.hover, hover);
            result.effects.clearHover = state.hover.kind != EHitKind::Outside && hover.kind == EHitKind::Outside;
            result.state.hover = hover;
            result.state.lastGlobalPoint = event.globalLogicalPoint;
            return result;
        }

        result.effects.consume = true;
        if (state.mode == EInputMode::CanvasPan && state.owner == EInputOwner::Mouse) {
            const double requested = state.lastGlobalPoint.y - event.globalLogicalPoint.y;
            const double next = panBy(*context.scene, context.pan, requested, context.viewportHeight);
            result.effects.panDelta = next - context.pan;
            result.state.lastGlobalPoint = event.globalLogicalPoint;
            return result;
        }

        const double distance = std::hypot(event.globalLogicalPoint.x - state.pressGlobalPoint.x, event.globalLogicalPoint.y - state.pressGlobalPoint.y);
        if (state.mode == EInputMode::MousePressPending && distance >= context.dragThreshold) {
            result.state.mode = EInputMode::WindowDrag;
            result.state.dragSource = dropSourceFor(*context.scene, state.pressed);
            result.effects.beginDrag = true;
        } else if (state.mode == EInputMode::WindowDrag) {
            result.effects.updateDrag = true;
        }
        if (result.state.mode == EInputMode::WindowDrag) {
            result.effects.dropIntent = dropAt(result.state, context, event.globalLogicalPoint);
            result.state.dropIntent = result.effects.dropIntent.value_or(SDropIntent{});
        }
        result.state.lastGlobalPoint = event.globalLogicalPoint;
        return result;
    }

    if (event.kind == EInputKind::MouseAxis) {
        if (state.owner != EInputOwner::None) {
            result.effects.consume = true;
            return result;
        }
        if (!monitorLocalPoint(event.globalLogicalPoint, context.monitor))
            return result;
        const double next = panBy(*context.scene, context.pan, event.axisDelta, context.viewportHeight);
        result.effects.consume = true;
        result.effects.panDelta = next - context.pan;
        return result;
    }

    if (event.kind == EInputKind::MouseButton) {
        if (event.button != PRIMARY_BUTTON || state.owner == EInputOwner::Touch)
            return result;
        if (event.pressed) {
            if (state.mode != EInputMode::Idle)
                return result;
            const auto local = monitorLocalPoint(event.globalLogicalPoint, context.monitor);
            if (!local)
                return result;
            const auto hit = hitTest(*context.scene, *local, context.pan);
            result.effects.consume = true;
            result.state.owner = EInputOwner::Mouse;
            result.state.pressGlobalPoint = event.globalLogicalPoint;
            result.state.lastGlobalPoint = event.globalLogicalPoint;
            result.state.hover = exactTarget(hit) ? hit : SHitResult{};
            if (exactTarget(hit)) {
                result.state.mode = EInputMode::MousePressPending;
                result.state.pressed = hit;
            } else {
                result.state.mode = EInputMode::CanvasPan;
            }
            return result;
        }

        if (state.owner != EInputOwner::Mouse)
            return result;
        result.effects.consume = true;
        if (state.mode == EInputMode::MousePressPending) {
            const auto released = targetHitAt(context, event.globalLogicalPoint);
            if (sameHit(released, state.pressed))
                result.effects.selection = released;
        } else if (state.mode == EInputMode::WindowDrag) {
            result.effects.dropIntent = dropAt(state, context, event.globalLogicalPoint);
            if (result.effects.dropIntent && result.effects.dropIntent->kind != EDropKind::Invalid)
                result.effects.finishDrag = true;
            else
                result.effects.cancelDrag = true;
        }
        const auto terminal = result.effects;
        result.state = idleState();
        result.effects = terminal;
        result.effects.resetOwnership = true;
        return result;
    }

    if (event.kind == EInputKind::TouchDown) {
        if (state.owner != EInputOwner::None)
            return result;
        const auto local = monitorLocalPoint(event.globalLogicalPoint, context.monitor);
        if (!local)
            return result;
        const auto hit = hitTest(*context.scene, *local, context.pan);
        result.effects.consume = true;
        result.state.owner = EInputOwner::Touch;
        result.state.owningTouchId = event.touchId;
        result.state.pressGlobalPoint = event.globalLogicalPoint;
        result.state.lastGlobalPoint = event.globalLogicalPoint;
        result.state.hover = exactTarget(hit) ? hit : SHitResult{};
        if (exactTarget(hit)) {
            result.state.mode = EInputMode::TouchPressPending;
            result.state.pressed = hit;
        } else {
            result.state.mode = EInputMode::CanvasPan;
        }
        return result;
    }

    if (event.kind == EInputKind::TouchMotion) {
        if (!matchingTouch)
            return result;
        result.effects.consume = true;
        if (state.mode == EInputMode::CanvasPan) {
            const double requested = state.lastGlobalPoint.y - event.globalLogicalPoint.y;
            const double next = panBy(*context.scene, context.pan, requested, context.viewportHeight);
            result.effects.panDelta = next - context.pan;
        } else {
            const double distance = std::hypot(event.globalLogicalPoint.x - state.pressGlobalPoint.x, event.globalLogicalPoint.y - state.pressGlobalPoint.y);
            if (state.mode == EInputMode::TouchPressPending && distance >= context.dragThreshold) {
                result.state.mode = EInputMode::WindowDrag;
                result.state.dragSource = dropSourceFor(*context.scene, state.pressed);
                result.effects.beginDrag = true;
            } else if (state.mode == EInputMode::WindowDrag) {
                result.effects.updateDrag = true;
            }
            if (result.state.mode == EInputMode::WindowDrag) {
                result.effects.dropIntent = dropAt(result.state, context, event.globalLogicalPoint);
                result.state.dropIntent = result.effects.dropIntent.value_or(SDropIntent{});
            }
        }
        result.state.lastGlobalPoint = event.globalLogicalPoint;
        return result;
    }

    if (event.kind == EInputKind::TouchUp) {
        if (!matchingTouch)
            return result;
        result.effects.consume = true;
        if (state.mode == EInputMode::TouchPressPending) {
            const auto released = targetHitAt(context, state.lastGlobalPoint);
            if (sameHit(released, state.pressed))
                result.effects.selection = released;
        } else if (state.mode == EInputMode::WindowDrag) {
            result.effects.dropIntent = dropAt(state, context, state.lastGlobalPoint);
            if (result.effects.dropIntent && result.effects.dropIntent->kind != EDropKind::Invalid)
                result.effects.finishDrag = true;
            else
                result.effects.cancelDrag = true;
        }
        const auto terminal = result.effects;
        result.state = idleState();
        result.effects = terminal;
        result.effects.resetOwnership = true;
        return result;
    }

    if (event.kind == EInputKind::TouchCancel) {
        if (!matchingTouch)
            return result;
        return resetOwned(state, true);
    }

    return result;
}

}
