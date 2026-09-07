#pragma once

#include "ScrollingOverviewLogic.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Hyprexpo::Scrolling {

enum class EOutputTransform {
    Normal,
    Rotate90,
    Rotate180,
    Rotate270,
    Flipped,
    Flipped90,
    Flipped180,
    Flipped270,
};

struct SMonitorGeometry {
    SPoint           position;
    SSize            logicalSize;
    SSize            pixelSize;
    double           scale = 1.0;
    EOutputTransform transform = EOutputTransform::Normal;
};

enum class EInputKind {
    MouseMove,
    MouseButton,
    MouseAxis,
    TouchDown,
    TouchMotion,
    TouchUp,
    TouchCancel,
};

enum class EInputMode {
    Idle,
    MousePressPending,
    TouchPressPending,
    CanvasPan,
    WindowDrag,
};

enum class EInputOwner {
    None,
    Mouse,
    Touch,
};

enum class EResetReason {
    Cancel,
    StaleTarget,
    Refresh,
    Close,
    Teardown,
};

struct SInputEvent {
    EInputKind kind = EInputKind::MouseMove;
    SPoint     globalLogicalPoint;
    double     axisDelta = 0.0;
    int32_t    touchId = -1;
    uint32_t   button = 0;
    bool       pressed = false;
    uint32_t   time = 0;
};

struct SInputState {
    EInputMode  mode = EInputMode::Idle;
    EInputOwner owner = EInputOwner::None;
    SHitResult  hover;
    SHitResult  pressed;
    SDropSource dragSource;
    SDropIntent dropIntent;
    int32_t     owningTouchId = -1;
    SPoint      pressGlobalPoint;
    SPoint      lastGlobalPoint;
};

struct SInputEffects {
    bool                      consume = false;
    bool                      hoverChanged = false;
    bool                      clearHover = false;
    double                    panDelta = 0.0;
    std::optional<SHitResult> selection;
    bool                      beginDrag = false;
    bool                      updateDrag = false;
    bool                      finishDrag = false;
    bool                      cancelDrag = false;
    bool                      resetOwnership = false;
    std::optional<SDropIntent> dropIntent;
};

struct SInputTransition {
    SInputState   state;
    SInputEffects effects;
};

struct SInputContext {
    const SScene*    scene = nullptr;
    SMonitorGeometry monitor;
    double           pan = 0.0;
    double           viewportHeight = 0.0;
    double           dragThreshold = 12.0;
};

struct SParsedInputStep {
    std::optional<SInputEvent>  event;
    std::optional<EResetReason> reset;
};

struct SParsedInputSequence {
    bool                          valid = false;
    std::string                   error;
    std::string                   requestId;
    std::vector<SParsedInputStep> steps;
};

struct SInputDiagnosticRecord {
    SInputState   state;
    SInputEffects effects;
    double        pan = 0.0;
};

std::optional<SPoint> monitorLocalPoint(SPoint globalLogicalPoint, const SMonitorGeometry& monitor);
std::optional<SPoint> touchToGlobalLogical(SPoint normalizedPoint, const SMonitorGeometry& monitor);
SInputTransition      transitionInput(const SInputState& state, const SInputEvent& event, const SInputContext& context);
SInputTransition      resetInput(const SInputState& state, EResetReason reason);
SParsedInputSequence  parseInputSequence(const std::string& sequence, size_t maxEvents = 128);
std::string           inputDiagnosticJson(const std::string& requestId, const std::vector<SInputDiagnosticRecord>& records, const SInputState& finalState, bool hasDropIntent);

}
