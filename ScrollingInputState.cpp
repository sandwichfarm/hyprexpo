#include "ScrollingInputState.hpp"

#include <algorithm>
#include <cmath>

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

SInputTransition resetOwned(const SInputState& state, bool consume) {
    auto reset = resetInput(state, EResetReason::Cancel);
    reset.effects.consume = consume;
    return reset;
}

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
            if (state.owner == EInputOwner::Mouse)
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
