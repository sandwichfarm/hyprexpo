#include "ScrollingOverviewLogic.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace Hyprexpo::Scrolling {

namespace {

constexpr double EPSILON = 0.000001;

bool finitePositive(double value) {
    return std::isfinite(value) && value > 0.0;
}

bool finiteNonNegative(double value) {
    return std::isfinite(value) && value >= 0.0;
}

bool contains(const SRect& box, SPoint point) {
    return point.x >= box.x && point.x < box.x + box.w && point.y >= box.y && point.y < box.y + box.h;
}

SPoint center(const SRect& box) {
    return {box.x + box.w / 2.0, box.y + box.h / 2.0};
}

bool horizontal(EDirection direction) {
    return direction == EDirection::Right || direction == EDirection::Left;
}

bool reversed(EDirection direction) {
    return direction == EDirection::Left || direction == EDirection::Up;
}

struct SFocusCandidate {
    SFocusRef ref;
    SRect     box;
    size_t    stableOrder = 0;
};

std::vector<SFocusCandidate> focusCandidates(const SScene& scene) {
    std::vector<SFocusCandidate> candidates;
    candidates.reserve(scene.targets.size() + scene.workspaces.size());

    size_t order = 0;
    for (const auto& target : scene.targets)
        candidates.push_back({.ref = {.kind = EHitKind::Target, .workspaceID = target.workspaceID, .targetToken = target.token}, .box = target.box, .stableOrder = order++});

    for (const auto& workspace : scene.workspaces) {
        EHitKind kind = EHitKind::Outside;
        if (workspace.kind == EWorkspaceKind::Empty)
            kind = EHitKind::EmptyWorkspace;
        else if (workspace.kind == EWorkspaceKind::Mixed)
            kind = EHitKind::MixedWorkspace;
        else if (workspace.kind == EWorkspaceKind::Terminal)
            kind = EHitKind::TerminalWorkspace;
        if (kind != EHitKind::Outside)
            candidates.push_back({.ref = {.kind = kind, .workspaceID = workspace.workspaceID}, .box = workspace.box, .stableOrder = order++});
    }

    return candidates;
}

std::optional<SPlacedWorkspace> workspaceFor(const SScene& scene, int64_t workspaceID) {
    const auto it = std::find_if(scene.workspaces.begin(), scene.workspaces.end(), [workspaceID](const auto& workspace) { return workspace.workspaceID == workspaceID; });
    if (it == scene.workspaces.end())
        return std::nullopt;
    return *it;
}

} // namespace

STapeLayout layoutTape(const STapeSpec& tape, SPoint origin, double crossExtent, double gap) {
    STapeLayout result;
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !finitePositive(crossExtent) || !finiteNonNegative(gap)) {
        result.error = "invalid tape geometry";
        return result;
    }
    if (tape.columns.empty()) {
        result.error = "scrolling tape has no columns";
        return result;
    }

    double primaryExtent = gap * static_cast<double>(tape.columns.size() - 1);
    for (const auto& column : tape.columns) {
        if (!finitePositive(column.extent) || column.targets.empty()) {
            result.error = "scrolling column extent/targets are invalid";
            return result;
        }
        primaryExtent += column.extent;
        if (!std::isfinite(primaryExtent)) {
            result.error = "scrolling tape extent overflowed";
            return result;
        }
        for (const auto& target : column.targets) {
            if (!finitePositive(target.proportion)) {
                result.error = "scrolling target proportion is invalid";
                return result;
            }
        }
    }

    result.bounds = horizontal(tape.direction) ? SRect{origin.x, origin.y, primaryExtent, crossExtent} : SRect{origin.x, origin.y, crossExtent, primaryExtent};
    double primaryCursor = 0.0;
    for (size_t columnIndex = 0; columnIndex < tape.columns.size(); ++columnIndex) {
        const auto& column = tape.columns[columnIndex];
        const double primaryStart = reversed(tape.direction) ? primaryExtent - primaryCursor - column.extent : primaryCursor;
        double proportionTotal = 0.0;
        for (const auto& target : column.targets)
            proportionTotal += target.proportion;
        if (!finitePositive(proportionTotal)) {
            result.error = "scrolling target proportions overflowed";
            result.targets.clear();
            return result;
        }

        double crossCursor = 0.0;
        for (size_t rowIndex = 0; rowIndex < column.targets.size(); ++rowIndex) {
            const auto& target = column.targets[rowIndex];
            const double targetCrossExtent = rowIndex + 1 == column.targets.size() ? crossExtent - crossCursor : crossExtent * target.proportion / proportionTotal;
            if (!finitePositive(targetCrossExtent)) {
                result.error = "scrolling target box is invalid";
                result.targets.clear();
                return result;
            }

            SRect box;
            if (horizontal(tape.direction))
                box = {origin.x + primaryStart, origin.y + crossCursor, column.extent, targetCrossExtent};
            else
                box = {origin.x + crossCursor, origin.y + primaryStart, targetCrossExtent, column.extent};
            result.targets.push_back({.token = target.token,
                                      .columnToken = column.token,
                                      .nativeColumnIndex = columnIndex,
                                      .nativeRowIndex = rowIndex,
                                      .box = box});
            crossCursor += targetCrossExtent;
        }
        primaryCursor += column.extent + gap;
    }

    result.valid = true;
    return result;
}

SScene buildScene(const std::vector<SWorkspaceSpec>& workspaces, int64_t activeWorkspaceID, const SSceneConfig& config) {
    SScene result;
    if (!finitePositive(config.viewportWidth) || !finitePositive(config.viewportHeight) || !finitePositive(config.rowHeight) || !finiteNonNegative(config.rowGap) ||
        !finiteNonNegative(config.columnGap)) {
        result.error = "invalid scene geometry";
        return result;
    }

    std::vector<SWorkspaceSpec> rows = workspaces;
    rows.push_back({.workspaceID = config.terminalWorkspaceID, .kind = EWorkspaceKind::Terminal, .tape = {}});
    if (rows.empty()) {
        result.error = "scene has no workspace rows";
        return result;
    }

    bool activePresent = false;
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const auto& row = rows[rowIndex];
        activePresent |= row.workspaceID == activeWorkspaceID;
        const double y = static_cast<double>(rowIndex) * (config.rowHeight + config.rowGap);
        SPlacedWorkspace placed{.workspaceID = row.workspaceID, .kind = row.kind, .direction = row.tape.direction, .box = {0.0, y, config.viewportWidth, config.rowHeight}};
        result.workspaces.push_back(placed);

        if (row.kind != EWorkspaceKind::Scrolling)
            continue;

        const double crossExtent = horizontal(row.tape.direction) ? config.rowHeight : config.viewportWidth;
        const auto tape = layoutTape(row.tape, {}, crossExtent, config.columnGap);
        if (!tape.valid) {
            result.error = "workspace " + std::to_string(row.workspaceID) + ": " + tape.error;
            result.workspaces.clear();
            result.targets.clear();
            return result;
        }

        const double scale = std::min({1.0, config.viewportWidth / tape.bounds.w, config.rowHeight / tape.bounds.h});
        if (!finitePositive(scale)) {
            result.error = "workspace tape cannot fit its row";
            result.workspaces.clear();
            return result;
        }
        const double offsetX = (config.viewportWidth - tape.bounds.w * scale) / 2.0;
        const double offsetY = y + (config.rowHeight - tape.bounds.h * scale) / 2.0;
        for (const auto& target : tape.targets) {
            SSceneTarget placedTarget;
            placedTarget.token             = target.token;
            placedTarget.columnToken       = target.columnToken;
            placedTarget.nativeColumnIndex = target.nativeColumnIndex;
            placedTarget.nativeRowIndex    = target.nativeRowIndex;
            placedTarget.box               = {offsetX + target.box.x * scale, offsetY + target.box.y * scale, target.box.w * scale, target.box.h * scale};
            placedTarget.workspaceID       = row.workspaceID;
            result.targets.push_back(placedTarget);
        }
    }

    if (!activePresent && !workspaces.empty()) {
        result.error = "active workspace row is absent";
        result.workspaces.clear();
        result.targets.clear();
        return result;
    }

    result.contentHeight = rows.size() * config.rowHeight + (rows.size() - 1) * config.rowGap;
    result.valid         = true;
    return result;
}

double clampPan(const SScene& scene, double pan, double viewportHeight) {
    if (!scene.valid || !std::isfinite(pan) || !finitePositive(viewportHeight))
        return 0.0;
    return std::clamp(pan, 0.0, std::max(0.0, scene.contentHeight - viewportHeight));
}

double initialPan(const SScene& scene, int64_t activeWorkspaceID, double viewportHeight) {
    const auto workspace = workspaceFor(scene, activeWorkspaceID);
    if (!workspace)
        return 0.0;
    return clampPan(scene, workspace->box.y + workspace->box.h / 2.0 - viewportHeight / 2.0, viewportHeight);
}

double panBy(const SScene& scene, double currentPan, double delta, double viewportHeight) {
    if (!std::isfinite(delta))
        return clampPan(scene, currentPan, viewportHeight);
    return clampPan(scene, currentPan + delta, viewportHeight);
}

SHitResult hitTest(const SScene& scene, SPoint viewportPoint, double pan) {
    if (!scene.valid || !std::isfinite(viewportPoint.x) || !std::isfinite(viewportPoint.y) || !std::isfinite(pan))
        return {};
    const SPoint point{viewportPoint.x, viewportPoint.y + pan};

    for (const auto& target : scene.targets) {
        if (contains(target.box, point))
            return {.kind = EHitKind::Target,
                    .workspaceID = target.workspaceID,
                    .targetToken = target.token,
                    .columnIndex = target.nativeColumnIndex,
                    .rowIndex = target.nativeRowIndex};
    }

    for (const auto& workspace : scene.workspaces) {
        if (!contains(workspace.box, point))
            continue;
        if (workspace.kind == EWorkspaceKind::Empty)
            return {.kind = EHitKind::EmptyWorkspace, .workspaceID = workspace.workspaceID};
        if (workspace.kind == EWorkspaceKind::Mixed)
            return {.kind = EHitKind::MixedWorkspace, .workspaceID = workspace.workspaceID};
        if (workspace.kind == EWorkspaceKind::Terminal)
            return {.kind = EHitKind::TerminalWorkspace, .workspaceID = workspace.workspaceID};
        return {};
    }

    return {};
}

SFocusRef moveFocus(const SScene& scene, const SFocusRef& current, EFocusDirection direction) {
    const auto candidates = focusCandidates(scene);
    const auto currentIt = std::find_if(candidates.begin(), candidates.end(), [&current](const auto& candidate) {
        return candidate.ref.kind == current.kind && candidate.ref.workspaceID == current.workspaceID && candidate.ref.targetToken == current.targetToken;
    });
    if (currentIt == candidates.end())
        return current;

    const auto currentCenter = center(currentIt->box);
    struct SRanked {
        const SFocusCandidate* candidate = nullptr;
        bool aligned = false;
        double primary = 0.0;
        double cross = 0.0;
    };
    std::vector<SRanked> ranked;
    for (const auto& candidate : candidates) {
        if (&candidate == &*currentIt)
            continue;
        const auto candidateCenter = center(candidate.box);
        const double dx = candidateCenter.x - currentCenter.x;
        const double dy = candidateCenter.y - currentCenter.y;
        double primary = 0.0;
        double cross = 0.0;
        bool aligned = false;
        if (direction == EFocusDirection::Left || direction == EFocusDirection::Right) {
            primary = direction == EFocusDirection::Left ? -dx : dx;
            cross   = std::abs(dy);
            aligned = candidate.box.y < currentIt->box.y + currentIt->box.h && candidate.box.y + candidate.box.h > currentIt->box.y;
        } else {
            primary = direction == EFocusDirection::Up ? -dy : dy;
            cross   = std::abs(dx);
            aligned = candidate.box.x < currentIt->box.x + currentIt->box.w && candidate.box.x + candidate.box.w > currentIt->box.x;
        }
        if (primary > EPSILON)
            ranked.push_back({.candidate = &candidate, .aligned = aligned, .primary = primary, .cross = cross});
    }
    if (ranked.empty())
        return current;

    const bool anyAligned = std::any_of(ranked.begin(), ranked.end(), [](const auto& item) { return item.aligned; });
    const auto best = std::min_element(ranked.begin(), ranked.end(), [anyAligned](const auto& a, const auto& b) {
        if (anyAligned && a.aligned != b.aligned)
            return a.aligned;
        if (std::abs(a.primary - b.primary) > EPSILON)
            return a.primary < b.primary;
        if (std::abs(a.cross - b.cross) > EPSILON)
            return a.cross < b.cross;
        return a.candidate->stableOrder < b.candidate->stableOrder;
    });
    return best->candidate->ref;
}

size_t adjustDestinationColumnIndex(size_t sourceColumn, size_t destinationColumn, bool sourceColumnRemoved) {
    if (sourceColumnRemoved && sourceColumn < destinationColumn)
        return destinationColumn - 1;
    return destinationColumn;
}

SDropIntent resolveDrop(const SScene& scene, const SDropSource& source, SPoint viewportPoint, double pan, double edgeFraction) {
    const auto hit = hitTest(scene, viewportPoint, pan);
    if (hit.kind == EHitKind::Outside || !std::isfinite(edgeFraction) || edgeFraction <= 0.0 || edgeFraction >= 0.5)
        return {};
    if (hit.kind == EHitKind::EmptyWorkspace)
        return {.kind = EDropKind::CrossWorkspace, .workspaceID = hit.workspaceID};
    if (hit.kind == EHitKind::MixedWorkspace)
        return {.kind = EDropKind::MixedFallback, .workspaceID = hit.workspaceID};
    if (hit.kind == EHitKind::TerminalWorkspace)
        return {.kind = EDropKind::TerminalWorkspace, .workspaceID = hit.workspaceID};

    const auto targetIt = std::find_if(scene.targets.begin(), scene.targets.end(), [&hit](const auto& target) { return target.token == hit.targetToken && target.workspaceID == hit.workspaceID; });
    const auto workspace = workspaceFor(scene, hit.workspaceID);
    if (targetIt == scene.targets.end() || !workspace)
        return {};

    const SPoint point{viewportPoint.x, viewportPoint.y + pan};
    const bool isHorizontal = horizontal(workspace->direction);
    const double primary = isHorizontal ? point.x : point.y;
    const double primaryStart = isHorizontal ? targetIt->box.x : targetIt->box.y;
    const double primaryExtent = isHorizontal ? targetIt->box.w : targetIt->box.h;
    const bool lowEdge = primary < primaryStart + primaryExtent * edgeFraction;
    const bool highEdge = primary >= primaryStart + primaryExtent * (1.0 - edgeFraction);

    EColumnPlacement placement = EColumnPlacement::Existing;
    if (lowEdge || highEdge) {
        const bool nativeBefore = reversed(workspace->direction) ? highEdge : lowEdge;
        placement = nativeBefore ? EColumnPlacement::Before : EColumnPlacement::After;
    }

    size_t rowIndex = hit.rowIndex;
    if (placement == EColumnPlacement::Existing) {
        const double cross = isHorizontal ? point.y : point.x;
        const double crossCenter = isHorizontal ? targetIt->box.y + targetIt->box.h / 2.0 : targetIt->box.x + targetIt->box.w / 2.0;
        rowIndex += cross >= crossCenter ? 1 : 0;
        if (source.workspaceID == hit.workspaceID && source.columnIndex == hit.columnIndex && source.rowIndex < rowIndex)
            --rowIndex;
        if (source.workspaceID == hit.workspaceID && source.columnIndex == hit.columnIndex && source.rowIndex == rowIndex)
            return {.kind = EDropKind::NoOp,
                    .placement = placement,
                    .workspaceID = hit.workspaceID,
                    .columnIndex = hit.columnIndex,
                    .adjustedColumnIndex = hit.columnIndex,
                    .rowIndex = rowIndex};
    }

    const size_t destinationColumn = hit.columnIndex + (placement == EColumnPlacement::After ? 1 : 0);
    const size_t adjustedColumn = adjustDestinationColumnIndex(source.columnIndex, destinationColumn, source.workspaceID == hit.workspaceID && source.sourceColumnWillDisappear);
    if (source.workspaceID != hit.workspaceID)
        return {.kind = EDropKind::CrossWorkspace,
                .placement = placement,
                .workspaceID = hit.workspaceID,
                .columnIndex = destinationColumn,
                .adjustedColumnIndex = adjustedColumn,
                .rowIndex = rowIndex};

    EDropKind kind = EDropKind::ExistingColumn;
    if (placement == EColumnPlacement::Before)
        kind = EDropKind::NewColumnBefore;
    else if (placement == EColumnPlacement::After)
        kind = EDropKind::NewColumnAfter;
    return {.kind = kind,
            .placement = placement,
            .workspaceID = hit.workspaceID,
            .columnIndex = destinationColumn,
            .adjustedColumnIndex = adjustedColumn,
            .rowIndex = rowIndex};
}

SCapturePlan planCaptureBudget(uint32_t monitorWidth, uint32_t monitorHeight, int multiplier, const std::vector<SCaptureRequest>& requests) {
    SCapturePlan result;
    result.multiplier = std::clamp(multiplier, 1, 16);
    if (monitorWidth == 0 || monitorHeight == 0)
        return result;

    const unsigned __int128 wideBudget = static_cast<unsigned __int128>(result.multiplier) * monitorWidth * monitorHeight;
    result.budgetPixels = wideBudget > std::numeric_limits<uint64_t>::max() ? std::numeric_limits<uint64_t>::max() : static_cast<uint64_t>(wideBudget);

    struct SCapped {
        uint64_t token = 0;
        double width = 0.0;
        double height = 0.0;
    };
    std::vector<SCapped> capped;
    capped.reserve(requests.size());
    long double desiredPixels = 0.0L;
    for (const auto& request : requests) {
        if (request.width == 0 || request.height == 0) {
            capped.push_back({.token = request.token});
            continue;
        }
        const double capScale = std::min({1.0, static_cast<double>(monitorWidth) / request.width, static_cast<double>(monitorHeight) / request.height});
        const double width = request.width * capScale;
        const double height = request.height * capScale;
        capped.push_back({.token = request.token, .width = width, .height = height});
        desiredPixels += static_cast<long double>(width) * height;
    }

    result.scale = desiredPixels <= static_cast<long double>(result.budgetPixels) || desiredPixels == 0.0L ? 1.0 : std::sqrt(static_cast<double>(result.budgetPixels / desiredPixels));
    result.allocations.reserve(capped.size());
    for (const auto& request : capped) {
        const auto width = static_cast<uint32_t>(std::floor(request.width * result.scale));
        const auto height = static_cast<uint32_t>(std::floor(request.height * result.scale));
        if (width < 16 || height < 16)
            result.allocations.push_back({.token = request.token});
        else
            result.allocations.push_back({.token = request.token, .width = width, .height = height, .capture = true});
    }
    result.valid = true;
    return result;
}

}
