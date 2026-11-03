#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Hyprexpo {

// Fixed first grids may explicitly include IDs owned by other monitors; -1 is padding.
std::vector<int64_t> absoluteWorkspaceIDs(int64_t first, size_t count, int64_t cap);

// Monitor damage has no window identity; pinned content is shared by source tiles.
bool shouldRefreshSourceWorkspace(bool belongsToSource, bool visible, bool wasActive, bool hasPinned, bool hadPinned);

struct SColorRGBA {
    float r = 0.F;
    float g = 0.F;
    float b = 0.F;
    float a = 1.F;
};

struct SGradientSpec {
    SColorRGBA c1;
    SColorRGBA c2;
    float      angleDeg = 0.F;
    bool       valid    = false;
};

enum class EWorkspaceMethodMode {
    Center,
    First,
};

enum class ENumberKeyMode {
    Workspace,
    Index,
    Passthrough,
};

enum class EOverviewModePreference {
    Auto,
    Grid,
    Windows,
};

// What the triggering workspace's tiling layout is, as far as the overview cares.
// A new layout that breaks the workspace grid's assumptions adds a value here.
enum class EWorkspaceLayoutKind {
    Classic,
    Scrolling,
};

// Which session implementation the factory instantiates. Dispatchers reason
// about kinds, never about concrete session classes.
enum class EOverviewSessionKind {
    WorkspaceGrid,
    ScrollingTape,
    ScrollingWindowGrid,
};

enum class EExpoNextAction {
    OpenOverview,
    EscalateWindowGrids,
    NoOp,
};

struct SWorkspaceMethodSpec {
    bool                 valid = false;
    EWorkspaceMethodMode mode  = EWorkspaceMethodMode::Center;
    std::string          workspace;
    std::string          error;
};

// Numeric workspace IDs a single workspace rule reserves: "11" or "r[11-20]".
struct SWorkspaceIDRange {
    int64_t first = 0;
    int64_t last  = 0;

    bool    operator==(const SWorkspaceIDRange&) const = default;
};

// Result of stripping an "all monitors" qualifier off an expo dispatcher arg.
struct SExpoCommand {
    std::string command;            // the arg with the qualifier removed
    bool        allMonitors = false;
};

// Split "toggle all", "on all" or a bare "all" (which means "toggle all") into
// the underlying command plus the all-monitors flag. Args without the
// qualifier come back unchanged with allMonitors = false.
SExpoCommand parseExpoCommand(const std::string& arg);

struct SPoint {
    double x = 0.0;
    double y = 0.0;
};

struct SSize {
    double w = 0.0;
    double h = 0.0;
};

struct SRect {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
};

enum class EDirection {
    Left,
    Right,
    Up,
    Down,
};

struct SGlobalTile {
    uint64_t overviewKey = 0;
    int      tileIndex   = -1;
    SRect    overviewGlobal;
    SRect    tileGlobal;
};

struct STileTarget {
    uint64_t overviewKey = 0;
    int      tileIndex   = -1;
};

struct STileHit {
    uint64_t overviewKey = 0;
    int      tileIndex   = -1;
    SPoint   pointLocal;
};

enum class EOverviewDragEventType {
    Press,
    Move,
    Target,
    Release,
    Cancel,
    MonitorDestroyed,
    AllClose,
};

struct SOverviewDragState {
    bool                  active             = false;
    bool                  moved              = false;
    uint64_t              sourceMonitorKey   = 0;
    int                   sourceTileIndex    = -1;
    uint64_t              targetMonitorKey   = 0;
    int                   targetTileIndex    = -1;
    uint64_t              windowKey          = 0;
    std::vector<uint64_t> affectedMonitorKeys;
};

struct SOverviewDragEvent {
    EOverviewDragEventType type       = EOverviewDragEventType::Move;
    uint64_t               monitorKey = 0;
    int                    tileIndex  = -1;
    uint64_t               windowKey  = 0;
};

struct SOverviewDropIntent {
    uint64_t sourceMonitorKey = 0;
    int      sourceTileIndex  = -1;
    uint64_t targetMonitorKey = 0;
    int      targetTileIndex  = -1;
    uint64_t windowKey        = 0;
};

struct SOverviewDragTransition {
    SOverviewDragState                next;
    std::optional<SOverviewDropIntent> drop;
    std::vector<uint64_t>             cleanupMonitorKeys;
    bool                              accepted = false;
    bool                              cleanup  = false;
};

struct SGridShape {
    int cols = 1;
    int rows = 1;
};

struct STileLayout {
    SRect box;
    int   row = -1;
    int   col = -1;
};

struct SDropIntentInput {
    bool   targetValid     = false;
    SPoint pointerLocal    = {};
    SRect  targetTileLocal = {};
    SSize  workspaceSize   = {};
    SSize  windowSize      = {};
    SPoint grabOffset      = {};
    double minProxySize    = 24.0;
};

struct SDropIntentGeometry {
    bool   valid               = false;
    SPoint targetWorkspacePoint = {};
    SRect  targetProxyLocal     = {};
};

struct SGestureConfig {
    int         fingers = 0;
    std::string direction;
    bool        directionValid = false;
};

struct SGestureSyncDecision {
    bool        registerGesture = false;
    std::string error;
};

std::string trimString(std::string value);
std::string lowerString(std::string value);
std::vector<std::string> splitCommaList(const std::string& value);

SGridShape               computeDynamicGridShape(int visibleCount);
SGridShape               computeFixedGridShape(int64_t columns, int64_t rows);
std::optional<std::vector<int64_t>> expandDynamicWorkspaceIDs(const std::vector<int64_t>& workspaceIDs, bool fillGaps, std::size_t maxExpandedWorkspaces);
SSize                    aspectCorrectTileSize(double screenW, double screenH, int cols, int rows, double gap);
STileLayout              computeTileLayout(int index, int visibleCount, SGridShape shape, SSize total, double gap, bool centerPartialRows);
int                      tileIndexAtPoint(double x, double y, int visibleCount, SGridShape shape, SSize total, double gap, bool centerPartialRows);
std::optional<STileTarget> selectDirectionalTile(const SRect& source, EDirection direction, const std::vector<SGlobalTile>& candidates);
std::optional<STileHit>    hitTestGlobalTile(const SPoint& point, const std::vector<SGlobalTile>& tiles);
SOverviewDragTransition    transitionOverviewDrag(const SOverviewDragState& state, const SOverviewDragEvent& event, const std::vector<uint64_t>& liveMonitorKeys);

int                      clampGridColumns(int64_t columns);
int                      gridColumnsToIncludeWorkspace(int configuredColumns, int firstWorkspaceID, int activeWorkspaceID, int maxColumns, int fixedRows = 0);
std::size_t              centeredWorkspaceBacktrack(std::size_t tileCount, int64_t activeWorkspaceID, std::optional<int64_t> lowestExistingID,
                                                    std::optional<int64_t> highestExistingID);
std::optional<SWorkspaceIDRange> workspaceRuleIDRange(const std::string& workspaceString);
int                      tileIndexFromPoint(double x, double y, double width, double height, int sideLength);
int                      numberKeyToVisibleIndex(int number);
ENumberKeyMode           numberKeyModeFromString(const std::string& mode);
EOverviewModePreference  overviewModePreferenceFromString(const std::string& mode);
// Single policy for session selection: preference x detected layout x explicit grid override.
EOverviewSessionKind     resolveOverviewSessionKind(EOverviewModePreference preference, EWorkspaceLayoutKind layout, bool forceWorkspaceGrid);
bool                     sessionKindIsScrolling(EOverviewSessionKind kind);
bool                     sessionKindEscalatesOnNext(EOverviewSessionKind kind);
EExpoNextAction          expoNextAction(bool overviewOpen, bool anyEscalatingSession);
// Raw digits pick visible positions in index mode, and in any non-passthrough mode on a window grid.
bool                     numberKeySelectsVisibleIndex(ENumberKeyMode mode, EOverviewSessionKind kind);
std::string_view         overviewSessionKindName(EOverviewSessionKind kind);
bool                     shouldAbortOverviewCloseForWorkspaceMove(bool windowPinned, bool movedOnOverviewMonitor);
SDropIntentGeometry      computeDropIntentGeometry(const SDropIntentInput& input);

SGestureSyncDecision     evaluateGestureSync(const SGestureConfig& config);

std::string              decodeConfigString(const void* dataptr, bool underlyingIsStdString, const std::string& fallback);

std::string              fallbackTokenForVisibleIndex(int visibleIndex);
int                      fallbackTokenToVisibleIndex(const std::string& token);

bool                     parseHexRGBA8(const std::string& value, SColorRGBA& out);
bool                     parseSolidColorSpec(const std::string& value, SColorRGBA& out);
SGradientSpec            parseGradientSpec(const std::string& value);

// Corner radii shared by the workspace grid and the scrolling views. Inputs are logical
// pixels from tile_rounding*; a negative variant inherits the base. Outputs are scaled.
struct STileRounding {
    int   base    = 0;
    int   focus   = 0;
    int   current = 0;
    int   hover   = 0;
    float power   = 2.F;
};
STileRounding resolveTileRounding(int64_t base, int64_t focus, int64_t current, int64_t hover, float power, double scale);
int           clampTileRound(int round, double width, double height);
bool                     isGradientBorderSpec(const std::string& value);
bool                     shouldShowWorkspaceLabel(bool labelEnabled, const std::string& labelShow, bool isHovered, bool isFocused, bool isCurrent);
std::string              resolveBorderSpec(const std::string& modernSpec, const std::string& legacySpec);
std::string              resolveLabelPosition(const std::string& modernValue, bool modernSetByUser, const std::string& legacyValue, bool legacySetByUser);
int                      resolveLabelFontSize(int modernValue, bool modernSetByUser, int legacyValue, bool legacySetByUser);

SWorkspaceMethodSpec     parseWorkspaceMethodSpec(const std::string& method);
SWorkspaceMethodSpec     resolveWorkspaceMethodForMonitor(const std::string& config, const std::string& monitorName);

}
