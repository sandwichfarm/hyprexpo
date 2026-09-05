#define WLR_USE_UNSTABLE

#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/helpers/Format.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/layout/supplementary/WorkspaceAlgoMatcher.hpp>
#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/TexPassElement.hpp>
#define protected public
#include <hyprland/src/render/Renderer.hpp>
#undef protected
#include <hyprland/src/version.h>
#include <hyprland/src/debug/log/Logger.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view EXPECTED_VERSION = "0.56.1";
constexpr std::string_view EXPECTED_HASH    = "5c9377c15f85c50648f35ca5a213754f95b93ca0";
constexpr std::string_view PRESENT_DISPATCHER = "hyprexpo-scroll-probe:inspect";
constexpr std::string_view ACK_DISPATCHER     = "hyprexpo-scroll-probe:ack";

HANDLE         g_handle              = nullptr;
uint64_t       g_sessionGeneration   = 0;
CFunctionHook* g_pRenderWorkspaceHook = nullptr;
using origRenderWorkspace = void (*)(void*, PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&, const CBox&);

std::string jsonEscape(const std::string_view value) {
    std::ostringstream escaped;
    for (const auto c : value) {
        switch (c) {
            case '"': escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                    escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned int>(static_cast<unsigned char>(c)) << std::dec;
                else
                    escaped << c;
        }
    }
    return escaped.str();
}

std::string quoted(const std::string_view value) {
    return "\"" + jsonEscape(value) + "\"";
}

std::string runtimeAbiHash() {
    return __hyprland_api_get_hash();
}

std::string runtimeCommitHash() {
    const auto abi       = runtimeAbiHash();
    const auto separator = abi.find('_');
    return abi.substr(0, separator);
}

template <typename T>
std::string identity(const SP<T>& pointer) {
    std::ostringstream value;
    value << "0x" << std::hex << reinterpret_cast<uintptr_t>(pointer.get());
    return value.str();
}

template <typename T>
std::string identity(const T* pointer) {
    std::ostringstream value;
    value << "0x" << std::hex << reinterpret_cast<uintptr_t>(pointer);
    return value.str();
}

std::string number(const double value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(17) << value;
    return output.str();
}

std::string boxJson(const CBox& box) {
    return "{\"x\":" + number(box.x) + ",\"y\":" + number(box.y) + ",\"w\":" + number(box.w) + ",\"h\":" + number(box.h) + "}";
}

std::string directionName(const Layout::Tiled::eScrollDirection direction) {
    switch (direction) {
        case Layout::Tiled::SCROLL_DIR_RIGHT: return "right";
        case Layout::Tiled::SCROLL_DIR_LEFT: return "left";
        case Layout::Tiled::SCROLL_DIR_DOWN: return "down";
        case Layout::Tiled::SCROLL_DIR_UP: return "up";
    }
    return "unknown";
}

struct SCaptureEvidence {
    int                      width                 = 0;
    int                      height                = 0;
    SP<Render::IFramebuffer> framebuffer;
    SP<Render::ITexture>     texture;
    std::string              error;
    bool                     rendererStateRestored = false;
};

class CRenderScope {
  public:
    explicit CRenderScope(Render::IHyprRenderer* renderer) : m_renderer(renderer), m_blockSurfaceFeedback(renderer->m_bBlockSurfaceFeedback),
                                                             m_renderingSnapshot(renderer->m_bRenderingSnapshot),
                                                             m_blockScreenShader(renderer->m_renderData.blockScreenShader) {}

    ~CRenderScope() {
        if (m_begun) {
            try {
                m_renderer->endRender();
            } catch (...) {}
        }
        restore();
    }

    void begun() {
        m_begun = true;
    }

    void finish() {
        if (!m_begun)
            return;
        m_renderer->endRender();
        m_begun = false;
    }

    bool restored() const {
        return m_renderer->m_bBlockSurfaceFeedback == m_blockSurfaceFeedback && m_renderer->m_bRenderingSnapshot == m_renderingSnapshot &&
            m_renderer->m_renderData.blockScreenShader == m_blockScreenShader;
    }

    void restore() {
        m_renderer->m_bBlockSurfaceFeedback          = m_blockSurfaceFeedback;
        m_renderer->m_bRenderingSnapshot             = m_renderingSnapshot;
        m_renderer->m_renderData.blockScreenShader   = m_blockScreenShader;
    }

  private:
    Render::IHyprRenderer* m_renderer;
    bool                   m_blockSurfaceFeedback;
    bool                   m_renderingSnapshot;
    bool                   m_blockScreenShader;
    bool                   m_begun = false;
};

struct STopology {
    struct SCandidate {
        WP<Layout::ITarget> target;
        PHLWINDOWREF        window;
        size_t              column = 0;
        size_t              row    = 0;
        double              primaryStart = 0;
        double              primarySize  = 0;
        bool                visible      = false;
        std::string         windowIdentity;
        std::string         title;
    };

    PHLWORKSPACE                        workspace;
    PHLMONITOR                          monitor;
    Layout::Tiled::CScrollingAlgorithm* algorithm = nullptr;
    SP<Layout::Tiled::SScrollingData>    data;
    Layout::Tiled::eScrollDirection      direction = Layout::Tiled::SCROLL_DIR_RIGHT;
    double                               offset    = 0;
    std::vector<SCandidate>              candidates;
    std::string                          activeWorkspace;
    std::string                          focusedWindow;
    std::string                          canonical;
    std::string                          json;
    std::string                          error;
};

struct SPendingRequest {
    uint64_t                   pendingGeneration = 0;
    std::string                requestId;
    PHLMONITORREF              monitor;
    SP<Render::IFramebuffer>   pendingFramebuffer;
    SP<Render::ITexture>       pendingTexture;
    CBox                       physicalTextureBox;
    CBox                       logicalCropBox;
    CBox                       markerBox;
    CHyprColor                 markerColor;
    std::string                markerHex;
    std::string                targetIdentity;
    std::string                windowIdentity;
    std::string                targetTitle;
    std::string                targetVisibility;
    size_t                     column = 0;
    size_t                     row    = 0;
    std::string                topologyBefore;
    std::string                topologyBeforeJson;
    std::string                activeWorkspaceBefore;
    std::string                focusedWindowBefore;
    bool                       pendingOverlay = false;
    size_t                     pendingPassCount = 0;
};

struct SCleanupHandshake {
    uint64_t      generation = 0;
    std::string   requestId;
    PHLMONITORREF monitor;
    std::string   topologyBefore;
    std::string   topologyBeforeJson;
    std::string   activeWorkspaceBefore;
    std::string   focusedWindowBefore;
};

std::optional<SPendingRequest>   g_pending;
std::optional<SCleanupHandshake> g_cleanupHandshake;

std::string targetJson(const SP<Layout::ITarget>& target, const CBox& layoutBox, const size_t row, const float proportion) {
    const auto window = target ? target->window() : PHLWINDOW{};
    const auto group  = target && target->type() == Layout::TARGET_TYPE_GROUP;
    const auto fullscreen = window && Fullscreen::controller()->isFullscreen(window);

    std::ostringstream output;
    output << "{\"row\":" << row << ",\"proportion\":" << number(proportion) << ",\"targetIdentity\":" << quoted(identity(target))
           << ",\"windowIdentity\":" << quoted(window ? identity(window) : "0x0") << ",\"windowStableId\":" << (window ? window->m_stableID : 0)
           << ",\"class\":" << quoted(window ? window->m_class : "") << ",\"title\":" << quoted(window ? window->m_title : "")
           << ",\"layoutBox\":" << boxJson(layoutBox) << ",\"group\":" << (group ? "true" : "false")
           << ",\"floating\":" << (target && target->floating() ? "true" : "false") << ",\"fullscreen\":" << (fullscreen ? "true" : "false")
           << ",\"pinned\":" << (window && window->m_pinned ? "true" : "false") << "}";
    return output.str();
}

STopology snapshotTopology() {
    STopology result;
    result.monitor   = Desktop::focusState()->monitor();
    result.workspace = result.monitor ? result.monitor->m_activeWorkspace : PHLWORKSPACE{};
    if (!result.monitor || !result.workspace || !result.workspace->m_space) {
        result.error = "focused monitor/workspace/space is unavailable";
        return result;
    }

    const auto algorithmOwner = result.workspace->m_space->algorithm();
    if (!algorithmOwner || !algorithmOwner->tiledAlgo()) {
        result.error = "workspace tiled algorithm is unavailable";
        return result;
    }

    auto* const tiled = algorithmOwner->tiledAlgo().get();
    if (Layout::Supplementary::algoMatcher()->getNameForTiledAlgo(tiled) != "scrolling") {
        result.error = "active workspace does not use the scrolling algorithm";
        return result;
    }

    result.algorithm = dynamic_cast<Layout::Tiled::CScrollingAlgorithm*>(tiled);
    if (!result.algorithm) {
        result.error = "scrolling matcher succeeded but dynamic_cast failed";
        return result;
    }

    for (const auto& targetRef : result.workspace->m_space->targets()) {
        const auto target = targetRef.lock();
        if (!target || target->floating())
            continue;
        const auto data = result.algorithm->dataFor(target);
        if (!data)
            continue;
        const auto column = data->column.lock();
        if (column) {
            result.data = column->scrollingData.lock();
            if (result.data)
                break;
        }
    }

    if (!result.data || !result.data->controller) {
        result.error = "no live scrolling target/data/controller is available";
        return result;
    }

    const auto& controller = *result.data->controller;
    if (result.data->columns.size() != controller.stripCount()) {
        result.error = "column/controller strip cardinality mismatch";
        return result;
    }
    result.direction = controller.getDirection();
    result.offset    = controller.getOffset();

    std::ostringstream columns;
    columns << '[';
    for (size_t columnIndex = 0; columnIndex < result.data->columns.size(); ++columnIndex) {
        if (columnIndex)
            columns << ',';
        const auto& column = result.data->columns[columnIndex];
        const auto& strip  = controller.getStrip(columnIndex);
        if (!column || strip.targetSizes.size() != column->targetDatas.size()) {
            result.error = "column target/controller row cardinality mismatch";
            return result;
        }

        const auto primaryStart = controller.calculateStripStart(columnIndex, result.algorithm->usableArea());
        const auto primarySize  = controller.calculateStripSize(columnIndex, result.algorithm->usableArea());
        const auto horizontal   = result.direction == Layout::Tiled::SCROLL_DIR_RIGHT || result.direction == Layout::Tiled::SCROLL_DIR_LEFT;
        const auto usable       = result.algorithm->usableArea();
        const auto viewportStart = horizontal ? usable.x : usable.y;
        const auto viewportEnd   = viewportStart + (horizontal ? usable.w : usable.h);
        const auto visible       = primaryStart < viewportEnd && primaryStart + primarySize > viewportStart;

        columns << "{\"index\":" << columnIndex << ",\"identity\":" << quoted(identity(column)) << ",\"width\":" << number(strip.size)
                << ",\"calculatedStart\":" << number(primaryStart) << ",\"calculatedSize\":" << number(primarySize)
                << ",\"visible\":" << (visible ? "true" : "false") << ",\"targets\":[";
        for (size_t row = 0; row < column->targetDatas.size(); ++row) {
            if (row)
                columns << ',';
            const auto& targetData = column->targetDatas[row];
            const auto target      = targetData ? targetData->target.lock() : SP<Layout::ITarget>{};
            if (!target) {
                result.error = "scrolling target expired while copying topology";
                return result;
            }
            columns << targetJson(target, targetData->layoutBox, row, strip.targetSizes[row]);
            const auto window = target->window();
            if (window) {
                result.candidates.push_back({.target = target,
                                             .window = window,
                                             .column = columnIndex,
                                             .row = row,
                                             .primaryStart = primaryStart,
                                             .primarySize = primarySize,
                                             .visible = visible,
                                             .windowIdentity = identity(window),
                                             .title = window->m_title});
            }
        }
        columns << "]}";
    }
    columns << ']';

    std::ostringstream layoutTargets;
    layoutTargets << '[';
    bool first = true;
    for (const auto& targetRef : result.workspace->m_space->targets()) {
        const auto target = targetRef.lock();
        if (!target)
            continue;
        if (!first)
            layoutTargets << ',';
        first = false;
        layoutTargets << targetJson(target, target->position(), 0, 0.F);
    }
    layoutTargets << ']';

    std::ostringstream json;
    json << "{\"algorithmIdentity\":" << quoted(identity(result.algorithm)) << ",\"dataIdentity\":" << quoted(identity(result.data))
         << ",\"direction\":" << quoted(directionName(result.direction)) << ",\"offset\":" << number(result.offset)
         << ",\"columns\":" << columns.str() << ",\"layoutTargets\":" << layoutTargets.str() << '}';
    const auto focusedWindow = Desktop::focusState()->window();
    result.activeWorkspace = std::to_string(result.workspace->m_id) + ":" + result.workspace->m_name;
    result.focusedWindow   = focusedWindow ? identity(focusedWindow) : "0x0";
    result.json            = json.str();
    result.canonical       = result.json + "|active=" + result.activeWorkspace + "|focus=" + result.focusedWindow;
    return result;
}

SCaptureEvidence captureTarget(const WP<Layout::ITarget>& targetRef, const PHLWINDOWREF& windowRef, const PHLMONITORREF& monitorRef) {
    SCaptureEvidence evidence;

    auto target  = targetRef.lock();
    auto window  = windowRef.lock();
    auto monitor = monitorRef.lock();
    if (!target || !window || !monitor || !monitor->m_output) {
        evidence.error = "target/window/monitor expired before capture";
        return evidence;
    }

    const auto logicalSize = window->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    evidence.width  = std::clamp(static_cast<int>(std::ceil(logicalSize.x * monitor->m_scale)), 1, static_cast<int>(monitor->m_pixelSize.x));
    evidence.height = std::clamp(static_cast<int>(std::ceil(logicalSize.y * monitor->m_scale)), 1, static_cast<int>(monitor->m_pixelSize.y));
    if (evidence.width <= 0 || evidence.height <= 0) {
        evidence.error = "target capture dimensions are invalid";
        return evidence;
    }

    Render::GL::g_pHyprOpenGL->makeEGLCurrent();
    const auto framebuffer = g_pHyprRenderer->createFB("hyprexpo scrolling API probe");
    if (!framebuffer || !framebuffer->alloc(evidence.width, evidence.height, DRM_FORMAT_ABGR8888)) {
        evidence.error = "target-sized framebuffer allocation failed";
        return evidence;
    }
    framebuffer->setImageDescription(monitor->workBufferImageDescription());

    CRegion fakeDamage{0, 0, evidence.width, evidence.height};
    CRenderScope renderScope{g_pHyprRenderer.get()};
    if (!g_pHyprRenderer->beginFullFakeRender(monitor, fakeDamage, framebuffer)) {
        renderScope.restore();
        evidence.rendererStateRestored = renderScope.restored();
        evidence.error = "beginFullFakeRender failed";
        return evidence;
    }
    renderScope.begun();
    g_pHyprRenderer->m_bBlockSurfaceFeedback = true;
    g_pHyprRenderer->m_bRenderingSnapshot    = true;
    glClearColor(0.F, 0.F, 0.F, 0.F);
    glClear(GL_COLOR_BUFFER_BIT);
    g_pHyprRenderer->startRenderPass();

    target = targetRef.lock();
    window = windowRef.lock();
    if (!target || !window) {
        evidence.error = "target/window expired after render begin";
        return evidence;
    }

    g_pHyprRenderer->renderWindow(window, monitor, Time::steadyNow(), false, Render::RENDER_PASS_ALL, true, true);
    g_pHyprRenderer->m_renderData.blockScreenShader = true;
    renderScope.finish();

    renderScope.restore();
    evidence.rendererStateRestored = renderScope.restored();
    if (!evidence.rendererStateRestored) {
        evidence.error = "renderer flags were not restored";
        return evidence;
    }

    evidence.framebuffer = framebuffer;
    evidence.texture     = framebuffer->getTexture();
    if (!evidence.texture)
        evidence.error = "completed framebuffer did not expose a texture";
    return evidence;
}

std::vector<std::string> splitRequest(const std::string& argument) {
    std::vector<std::string> fields;
    size_t                   start = 0;
    while (start <= argument.size()) {
        const auto separator = argument.find('|', start);
        fields.emplace_back(argument.substr(start, separator == std::string::npos ? std::string::npos : separator - start));
        if (separator == std::string::npos)
            break;
        start = separator + 1;
    }
    return fields;
}

bool validRequestId(const std::string& requestId) {
    return !requestId.empty() && requestId.size() <= 128 &&
        std::ranges::all_of(requestId, [](const unsigned char c) { return std::isalnum(c) || c == '.' || c == '_' || c == '-'; });
}

std::optional<STopology::SCandidate> selectCandidate(const STopology& topology, const std::string& selector) {
    if (selector == "visible") {
        const auto focused         = Desktop::focusState()->window();
        const auto focusedIdentity = focused ? identity(focused) : "";
        const auto selected        = std::ranges::find_if(topology.candidates, [&](const auto& candidate) {
            return candidate.visible && candidate.windowIdentity == focusedIdentity;
        });
        if (selected != topology.candidates.end())
            return *selected;
        const auto firstVisible = std::ranges::find_if(topology.candidates, [](const auto& candidate) { return candidate.visible; });
        return firstVisible == topology.candidates.end() ? std::nullopt : std::optional{*firstVisible};
    }
    if (selector == "offscreen") {
        const auto selected = std::ranges::find_if(topology.candidates, [](const auto& candidate) { return !candidate.visible; });
        return selected == topology.candidates.end() ? std::nullopt : std::optional{*selected};
    }
    const auto selected = std::ranges::find_if(topology.candidates, [&](const auto& candidate) {
        return candidate.windowIdentity == selector || candidate.title == selector;
    });
    return selected == topology.candidates.end() ? std::nullopt : std::optional{*selected};
}

std::pair<CHyprColor, std::string> markerColor(const uint64_t generation, const std::string& requestId) {
    uint32_t seed = static_cast<uint32_t>(generation * 2654435761ULL);
    for (const unsigned char c : requestId)
        seed = (seed * 33U) ^ c;
    const uint8_t red   = static_cast<uint8_t>(96U + (seed & 0x7FU));
    const uint8_t green = static_cast<uint8_t>(96U + ((seed >> 8U) & 0x7FU));
    const uint8_t blue  = static_cast<uint8_t>(96U + ((seed >> 16U) & 0x7FU));
    std::ostringstream hex;
    hex << '#' << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(red) << std::setw(2) << static_cast<unsigned int>(green)
        << std::setw(2) << static_cast<unsigned int>(blue);
    return {CHyprColor{red / 255.F, green / 255.F, blue / 255.F, 1.F}, hex.str()};
}

void clearPendingState() {
    if (!g_pending)
        return;
    g_pending->pendingOverlay      = false;
    g_pending->pendingPassCount    = 0;
    g_pending->pendingTexture      = {};
    g_pending->pendingFramebuffer  = {};
    g_pending.reset();
}

void emitFailure(const std::string& requestId, const uint64_t generation, const std::string& stage, const std::string& error) {
    std::ostringstream record;
    record << "{\"schema\":2,\"stage\":" << quoted(stage) << ",\"requestId\":" << quoted(requestId) << ",\"sessionGeneration\":" << generation
           << ",\"hyprlandVersion\":" << quoted(EXPECTED_VERSION) << ",\"compileHash\":" << quoted(GIT_COMMIT_HASH)
           << ",\"runtimeHash\":" << quoted(runtimeCommitHash()) << ",\"runtimeAbiHash\":" << quoted(runtimeAbiHash())
           << ",\"mutationOutcome\":\"not-attempted\",\"rollbackStatus\":\"not-required\",\"status\":\"FAIL\",\"error\":" << quoted(error) << '}';
    Log::logger->log(Log::INFO, "HYPREXPO_SCROLL_PROBE {}", record.str());
}

SDispatchResult present(std::string argument) {
    const auto fields = splitRequest(argument);
    if (fields.size() != 3 || fields[0] != "present" || !validRequestId(fields[1]) || fields[2].empty())
        return {.success = false, .error = "expected present|REQUEST_ID|SELECTOR"};
    if (g_pending)
        return {.success = false, .error = "a diagnostic presentation is already pending acknowledgement"};

    const auto generation = ++g_sessionGeneration;
    const auto& requestId  = fields[1];
    try {
        const auto topologyBefore = snapshotTopology();
        if (!topologyBefore.error.empty())
            throw std::runtime_error(topologyBefore.error);
        const auto candidate = selectCandidate(topologyBefore, fields[2]);
        if (!candidate)
            throw std::runtime_error(fields[2] == "offscreen" ? "no controller-classified offscreen target is available" : "requested target is unavailable");
        const auto window = candidate->window.lock();
        if (!window)
            throw std::runtime_error("selected target window expired before capture");
        const auto capture = captureTarget(candidate->target, candidate->window, topologyBefore.monitor);
        if (!capture.error.empty())
            throw std::runtime_error(capture.error);

        const auto monitorWidth  = static_cast<int>(topologyBefore.monitor->m_pixelSize.x);
        const auto monitorHeight = static_cast<int>(topologyBefore.monitor->m_pixelSize.y);
        const auto textureX      = std::floor((monitorWidth - capture.width) / 2.0);
        const auto textureY      = std::floor((monitorHeight - capture.height) / 2.0);
        if (textureX < 56 || capture.width > monitorWidth || capture.height > monitorHeight)
            throw std::runtime_error("target texture leaves no isolated marker margin on the recovery output");
        const auto [color, colorHex] = markerColor(generation, requestId);
        const CBox physicalTextureBox{textureX, textureY, static_cast<double>(capture.width), static_cast<double>(capture.height)};
        const CBox logicalCropBox{topologyBefore.monitor->m_position.x + physicalTextureBox.x / topologyBefore.monitor->m_scale,
                                  topologyBefore.monitor->m_position.y + physicalTextureBox.y / topologyBefore.monitor->m_scale,
                                  physicalTextureBox.w / topologyBefore.monitor->m_scale, physicalTextureBox.h / topologyBefore.monitor->m_scale};
        g_pending = SPendingRequest{.pendingGeneration = generation,
                                    .requestId = requestId,
                                    .monitor = topologyBefore.monitor,
                                    .pendingFramebuffer = capture.framebuffer,
                                    .pendingTexture = capture.texture,
                                    .physicalTextureBox = physicalTextureBox,
                                    .logicalCropBox = logicalCropBox,
                                    .markerBox = {12, 12, 28, 28},
                                    .markerColor = color,
                                    .markerHex = colorHex,
                                    .targetIdentity = identity(candidate->target.lock()),
                                    .windowIdentity = candidate->windowIdentity,
                                    .targetTitle = candidate->title,
                                    .targetVisibility = candidate->visible ? "visible" : "offscreen",
                                    .column = candidate->column,
                                    .row = candidate->row,
                                    .topologyBefore = topologyBefore.canonical,
                                    .topologyBeforeJson = topologyBefore.json,
                                    .activeWorkspaceBefore = topologyBefore.activeWorkspace,
                                    .focusedWindowBefore = topologyBefore.focusedWindow};
        g_pHyprRenderer->damageMonitor(topologyBefore.monitor);

        std::ostringstream record;
        record << "{\"schema\":2,\"stage\":\"READY\",\"requestId\":" << quoted(requestId) << ",\"sessionGeneration\":" << generation
               << ",\"hyprlandVersion\":" << quoted(EXPECTED_VERSION) << ",\"compileHash\":" << quoted(GIT_COMMIT_HASH)
               << ",\"runtimeHash\":" << quoted(runtimeCommitHash()) << ",\"runtimeAbiHash\":" << quoted(runtimeAbiHash())
               << ",\"monitor\":{\"id\":" << topologyBefore.monitor->m_id << ",\"name\":" << quoted(topologyBefore.monitor->m_name)
               << ",\"pixelWidth\":" << monitorWidth << ",\"pixelHeight\":" << monitorHeight << ",\"scale\":" << number(topologyBefore.monitor->m_scale)
               << "},\"target\":{\"targetIdentity\":" << quoted(g_pending->targetIdentity) << ",\"windowIdentity\":" << quoted(candidate->windowIdentity)
               << ",\"title\":" << quoted(candidate->title) << ",\"column\":" << candidate->column << ",\"row\":" << candidate->row
               << ",\"visibility\":" << quoted(g_pending->targetVisibility) << ",\"primaryStart\":" << number(candidate->primaryStart)
               << ",\"primarySize\":" << number(candidate->primarySize) << "},\"capture\":{\"width\":" << capture.width << ",\"height\":" << capture.height
               << ",\"physicalTextureBox\":" << boxJson(physicalTextureBox) << ",\"logicalCropBox\":" << boxJson(logicalCropBox)
               << ",\"rendererStateRestored\":" << (capture.rendererStateRestored ? "true" : "false") << "},\"marker\":{\"color\":" << quoted(colorHex)
               << ",\"box\":" << boxJson(g_pending->markerBox) << "},\"topologyBefore\":" << topologyBefore.json
               << ",\"activeWorkspaceBefore\":" << quoted(topologyBefore.activeWorkspace) << ",\"focusedWindowBefore\":" << quoted(topologyBefore.focusedWindow)
               << ",\"pendingGeneration\":" << generation << ",\"pendingFramebuffer\":true,\"pendingTexture\":true,\"pendingOverlay\":false"
               << ",\"mutationOutcome\":\"not-attempted\",\"rollbackStatus\":\"not-required\",\"status\":\"READY\"}";
        Log::logger->log(Log::INFO, "HYPREXPO_SCROLL_PROBE {}", record.str());
        return {};
    } catch (const std::exception& error) {
        clearPendingState();
        emitFailure(requestId, generation, "READY", error.what());
        return {.success = false, .error = error.what()};
    } catch (...) {
        clearPendingState();
        emitFailure(requestId, generation, "READY", "unknown exception");
        return {.success = false, .error = "unknown exception"};
    }
}

SDispatchResult acknowledge(std::string argument) {
    const auto fields = splitRequest(argument);
    if (fields.size() != 3 || fields[0] != "ack" || !validRequestId(fields[1]))
        return {.success = false, .error = "expected ack|REQUEST_ID|GENERATION"};
    uint64_t generation = 0;
    try {
        size_t consumed = 0;
        generation      = std::stoull(fields[2], &consumed);
        if (consumed != fields[2].size())
            return {.success = false, .error = "ack generation is invalid"};
    } catch (...) {
        return {.success = false, .error = "ack generation is invalid"};
    }
    if (!g_pending || g_pending->requestId != fields[1] || g_pending->pendingGeneration != generation)
        return {.success = false, .error = "ack does not match the current pending request"};

    const auto monitor = g_pending->monitor.lock();
    g_cleanupHandshake = SCleanupHandshake{.generation = generation,
                                            .requestId = fields[1],
                                            .monitor = g_pending->monitor,
                                            .topologyBefore = g_pending->topologyBefore,
                                            .topologyBeforeJson = g_pending->topologyBeforeJson,
                                            .activeWorkspaceBefore = g_pending->activeWorkspaceBefore,
                                            .focusedWindowBefore = g_pending->focusedWindowBefore};
    clearPendingState();
    if (!monitor) {
        g_cleanupHandshake.reset();
        return {.success = false, .error = "pending monitor expired during acknowledgement"};
    }
    g_pHyprRenderer->damageMonitor(monitor);
    return {};
}

void emitCleanupDone() {
    if (!g_cleanupHandshake)
        return;
    const auto topologyAfter = snapshotTopology();
    const auto unchanged = topologyAfter.error.empty() && topologyAfter.canonical == g_cleanupHandshake->topologyBefore &&
        topologyAfter.activeWorkspace == g_cleanupHandshake->activeWorkspaceBefore && topologyAfter.focusedWindow == g_cleanupHandshake->focusedWindowBefore;
    std::ostringstream record;
    record << "{\"schema\":2,\"stage\":\"DONE\",\"requestId\":" << quoted(g_cleanupHandshake->requestId)
           << ",\"sessionGeneration\":" << g_cleanupHandshake->generation << ",\"topologyBefore\":" << g_cleanupHandshake->topologyBeforeJson
           << ",\"topologyAfter\":" << (topologyAfter.error.empty() ? topologyAfter.json : "null")
           << ",\"topologyUnchanged\":" << (unchanged ? "true" : "false") << ",\"activeWorkspaceBefore\":" << quoted(g_cleanupHandshake->activeWorkspaceBefore)
           << ",\"activeWorkspaceAfter\":" << quoted(topologyAfter.activeWorkspace) << ",\"focusedWindowBefore\":" << quoted(g_cleanupHandshake->focusedWindowBefore)
           << ",\"focusedWindowAfter\":" << quoted(topologyAfter.focusedWindow)
           << ",\"pendingGeneration\":null,\"pendingFramebuffer\":false,\"pendingTexture\":false,\"pendingOverlay\":false,\"pendingPassCount\":0"
           << ",\"markerAbsentFromQueuedPass\":true,\"mutationOutcome\":\"not-attempted\",\"rollbackStatus\":\"not-required\",\"status\":"
           << quoted(unchanged ? "PASS" : "FAIL");
    if (!topologyAfter.error.empty())
        record << ",\"error\":" << quoted(topologyAfter.error);
    record << '}';
    Log::logger->log(Log::INFO, "HYPREXPO_SCROLL_PROBE {}", record.str());
    g_cleanupHandshake.reset();
}

void hkRenderWorkspace(void* thisptr, PHLMONITOR monitor, PHLWORKSPACE workspace, const Time::steady_tp& now, const CBox& geometry) {
    ((origRenderWorkspace)g_pRenderWorkspaceHook->m_original)(thisptr, monitor, workspace, now, geometry);
    try {
        if (g_pending) {
            const auto pendingMonitor = g_pending->monitor.lock();
            if (!pendingMonitor || pendingMonitor != monitor || !g_pending->pendingTexture)
                return;
            const CBox outputBox{0, 0, pendingMonitor->m_pixelSize.x, pendingMonitor->m_pixelSize.y};
            auto backdropData  = CRectPassElement::SRectData{};
            backdropData.box   = outputBox;
            backdropData.color = CHyprColor{0.02, 0.02, 0.025, 1.0};
            g_pHyprRenderer->currentPass().add(makeUnique<CRectPassElement>(backdropData));
            auto markerBorderData  = CRectPassElement::SRectData{};
            markerBorderData.box   = {8, 8, 36, 36};
            markerBorderData.color = CHyprColor{0, 0, 0, 1};
            g_pHyprRenderer->currentPass().add(makeUnique<CRectPassElement>(markerBorderData));
            auto markerData  = CRectPassElement::SRectData{};
            markerData.box   = g_pending->markerBox;
            markerData.color = g_pending->markerColor;
            g_pHyprRenderer->currentPass().add(makeUnique<CRectPassElement>(markerData));
            auto textureData   = CTexPassElement::SRenderData{};
            textureData.tex    = g_pending->pendingTexture;
            textureData.box    = g_pending->physicalTextureBox;
            textureData.damage = CRegion{outputBox};
            g_pHyprRenderer->currentPass().add(makeUnique<CTexPassElement>(std::move(textureData)));
            g_pending->pendingOverlay   = true;
            g_pending->pendingPassCount = 4;
            return;
        }
        if (g_cleanupHandshake && g_cleanupHandshake->monitor.lock() == monitor)
            emitCleanupDone();
    } catch (const std::exception& error) {
        const auto requestId  = g_pending ? g_pending->requestId : (g_cleanupHandshake ? g_cleanupHandshake->requestId : "render-hook");
        const auto generation = g_pending ? g_pending->pendingGeneration : (g_cleanupHandshake ? g_cleanupHandshake->generation : 0);
        emitFailure(requestId, generation, "RENDER", error.what());
    } catch (...) {
        const auto requestId  = g_pending ? g_pending->requestId : (g_cleanupHandshake ? g_cleanupHandshake->requestId : "render-hook");
        const auto generation = g_pending ? g_pending->pendingGeneration : (g_cleanupHandshake ? g_cleanupHandshake->generation : 0);
        emitFailure(requestId, generation, "RENDER", "unknown exception");
    }
}

} // namespace

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    g_handle = handle;
    const std::string runtimeAbi = __hyprland_api_get_hash();
    const std::string clientAbi  = __hyprland_api_get_client_hash();
    if (std::string_view{GIT_TAG}.substr(1) != EXPECTED_VERSION || std::string_view{GIT_COMMIT_HASH} != EXPECTED_HASH || runtimeAbi != clientAbi ||
        !runtimeAbi.starts_with(EXPECTED_HASH))
        throw std::runtime_error("scrolling API probe requires exact Hyprland 0.56.1 / 5c9377c ABI");
    if (!HyprlandAPI::addDispatcherV2(g_handle, std::string{PRESENT_DISPATCHER}, present) ||
        !HyprlandAPI::addDispatcherV2(g_handle, std::string{ACK_DISPATCHER}, acknowledge))
        throw std::runtime_error("failed to register scrolling API probe dispatchers");

    const auto functions = HyprlandAPI::findFunctionsByName(g_handle, "renderWorkspace");
    if (functions.empty())
        throw std::runtime_error("failed to resolve exact renderWorkspace hook target");
    g_pRenderWorkspaceHook = HyprlandAPI::createFunctionHook(g_handle, functions[0].address, (void*)hkRenderWorkspace);
    if (!g_pRenderWorkspaceHook || !g_pRenderWorkspaceHook->hook())
        throw std::runtime_error("failed to install exact renderWorkspace hook");
    return {"hyprexpo-scroll-probe", "GPU-only native scrolling target presentation probe", "sandwich", "0.2.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    const auto monitor = g_pending ? g_pending->monitor.lock() : PHLMONITOR{};
    clearPendingState();
    g_cleanupHandshake.reset();
    if (monitor)
        g_pHyprRenderer->damageMonitor(monitor);
    if (g_pRenderWorkspaceHook) {
        g_pRenderWorkspaceHook->unhook();
        if (g_handle)
            HyprlandAPI::removeFunctionHook(g_handle, g_pRenderWorkspaceHook);
        g_pRenderWorkspaceHook = nullptr;
    }
    if (g_handle) {
        HyprlandAPI::removeDispatcher(g_handle, std::string{ACK_DISPATCHER});
        HyprlandAPI::removeDispatcher(g_handle, std::string{PRESENT_DISPATCHER});
    }
    g_handle = nullptr;
}
