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
#define protected public
#include <hyprland/src/render/Renderer.hpp>
#undef protected
#include <hyprland/src/version.h>
#include <hyprland/src/debug/log/Logger.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::string_view EXPECTED_VERSION = "0.56.1";
constexpr std::string_view EXPECTED_HASH    = "5c9377c15f85c50648f35ca5a213754f95b93ca0";
constexpr std::string_view DISPATCHER       = "hyprexpo-scroll-probe:inspect";
constexpr std::string_view PATH_PREFIX      = "/tmp/hyprexpo-scroll-probe-";

HANDLE   g_handle            = nullptr;
uint64_t g_sessionGeneration = 0;

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

class CSha256 {
  public:
    void update(const uint8_t* data, const size_t size) {
        for (size_t i = 0; i < size; ++i) {
            m_block[m_blockSize++] = data[i];
            m_totalBits += 8;
            if (m_blockSize == m_block.size()) {
                transform();
                m_blockSize = 0;
            }
        }
    }

    std::string finish() {
        const auto originalBits = m_totalBits;
        m_block[m_blockSize++]  = 0x80;
        if (m_blockSize > 56) {
            while (m_blockSize < m_block.size())
                m_block[m_blockSize++] = 0;
            transform();
            m_blockSize = 0;
        }
        while (m_blockSize < 56)
            m_block[m_blockSize++] = 0;
        for (int shift = 56; shift >= 0; shift -= 8)
            m_block[m_blockSize++] = static_cast<uint8_t>(originalBits >> shift);
        transform();

        std::ostringstream output;
        output << std::hex << std::setfill('0');
        for (const auto word : m_state)
            output << std::setw(8) << word;
        return output.str();
    }

  private:
    static constexpr std::array<uint32_t, 64> K = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    };

    static uint32_t rotateRight(const uint32_t value, const int amount) {
        return std::rotr(value, amount);
    }

    void transform() {
        std::array<uint32_t, 64> words{};
        for (size_t i = 0; i < 16; ++i) {
            const auto offset = i * 4;
            words[i] = (static_cast<uint32_t>(m_block[offset]) << 24) | (static_cast<uint32_t>(m_block[offset + 1]) << 16) |
                (static_cast<uint32_t>(m_block[offset + 2]) << 8) | static_cast<uint32_t>(m_block[offset + 3]);
        }
        for (size_t i = 16; i < words.size(); ++i) {
            const auto s0 = rotateRight(words[i - 15], 7) ^ rotateRight(words[i - 15], 18) ^ (words[i - 15] >> 3);
            const auto s1 = rotateRight(words[i - 2], 17) ^ rotateRight(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i]      = words[i - 16] + s0 + words[i - 7] + s1;
        }

        auto a = m_state[0];
        auto b = m_state[1];
        auto c = m_state[2];
        auto d = m_state[3];
        auto e = m_state[4];
        auto f = m_state[5];
        auto g = m_state[6];
        auto h = m_state[7];
        for (size_t i = 0; i < words.size(); ++i) {
            const auto sigma1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
            const auto choice = (e & f) ^ (~e & g);
            const auto temp1  = h + sigma1 + choice + K[i] + words[i];
            const auto sigma0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temp2    = sigma0 + majority;
            h                   = g;
            g                   = f;
            f                   = e;
            e                   = d + temp1;
            d                   = c;
            c                   = b;
            b                   = a;
            a                   = temp1 + temp2;
        }
        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
        m_state[5] += f;
        m_state[6] += g;
        m_state[7] += h;
    }

    std::array<uint32_t, 8> m_state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<uint8_t, 64> m_block{};
    size_t                  m_blockSize = 0;
    uint64_t                m_totalBits = 0;
};

std::string sha256(const std::vector<uint8_t>& bytes) {
    CSha256 hash;
    hash.update(bytes.data(), bytes.size());
    return hash.finish();
}

struct SCaptureEvidence {
    int         width                  = 0;
    int         height                 = 0;
    size_t      nontransparentPixels   = 0;
    CBox        nonBackgroundBounds    = {};
    std::string sha256;
    std::string imagePath;
    std::string error;
    bool        rendererStateRestored  = false;
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
    PHLWORKSPACE                                  workspace;
    PHLMONITOR                                    monitor;
    Layout::Tiled::CScrollingAlgorithm*           algorithm = nullptr;
    SP<Layout::Tiled::SScrollingData>              data;
    SP<Layout::ITarget>                            captureTarget;
    SP<Layout::Tiled::SScrollingTargetData>        captureData;
    Layout::Tiled::eScrollDirection                direction = Layout::Tiled::SCROLL_DIR_RIGHT;
    double                                         offset    = 0;
    std::string                                    canonical;
    std::string                                    json;
    std::string                                    error;
};

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

    const auto focusedWindow = Desktop::focusState()->window();
    if (focusedWindow && focusedWindow->m_workspace == result.workspace) {
        const auto target = focusedWindow->layoutTarget();
        const auto data   = target ? result.algorithm->dataFor(target) : SP<Layout::Tiled::SScrollingTargetData>{};
        if (target && data) {
            result.captureTarget = target;
            result.captureData   = data;
        }
    }

    for (const auto& targetRef : result.workspace->m_space->targets()) {
        const auto target = targetRef.lock();
        if (!target || target->floating())
            continue;
        const auto data = result.algorithm->dataFor(target);
        if (!data)
            continue;
        if (!result.captureTarget) {
            result.captureTarget = target;
            result.captureData   = data;
        }
        const auto column = data->column.lock();
        if (column) {
            result.data = column->scrollingData.lock();
            if (result.data)
                break;
        }
    }

    if (!result.captureTarget || !result.captureData || !result.data || !result.data->controller) {
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

        columns << "{\"index\":" << columnIndex << ",\"identity\":" << quoted(identity(column)) << ",\"width\":" << number(strip.size)
                << ",\"calculatedStart\":" << number(controller.calculateStripStart(columnIndex, result.algorithm->usableArea()))
                << ",\"calculatedSize\":" << number(controller.calculateStripSize(columnIndex, result.algorithm->usableArea())) << ",\"targets\":[";
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
    result.json      = json.str();
    result.canonical = result.json;
    return result;
}

bool writePpm(const std::string& path, const int width, const int height, const std::vector<uint8_t>& pixels) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
        return false;
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            const auto offset = static_cast<size_t>((y * width + x) * 4);
            output.write(reinterpret_cast<const char*>(&pixels[offset]), 3);
        }
    }
    return output.good();
}

SCaptureEvidence captureTarget(const WP<Layout::ITarget>& targetRef, const PHLWINDOWREF& windowRef, const PHLMONITORREF& monitorRef, const std::string& outputPath) {
    SCaptureEvidence evidence;
    evidence.imagePath = outputPath;

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

    framebuffer->bind();
    std::vector<uint8_t> pixels(static_cast<size_t>(evidence.width) * evidence.height * 4);
    while (glGetError() != GL_NO_ERROR) {}
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    if (const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE) {
        evidence.error = "capture framebuffer is incomplete: " + std::to_string(status);
        return evidence;
    }
    GLint readFormat = 0;
    GLint readType   = 0;
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &readFormat);
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &readType);
    if ((readFormat != GL_RGBA && readFormat != GL_BGRA_EXT) || readType != GL_UNSIGNED_BYTE) {
        evidence.error = "unsupported framebuffer read format/type: " + std::to_string(readFormat) + "/" + std::to_string(readType);
        return evidence;
    }
    while (glGetError() != GL_NO_ERROR) {}
    glReadPixels(0, 0, evidence.width, evidence.height, readFormat, readType, pixels.data());
    if (const auto error = glGetError(); error != GL_NO_ERROR) {
        evidence.error = "glReadPixels failed with GL error " + std::to_string(error) + " for format/type " + std::to_string(readFormat) + "/" + std::to_string(readType);
        return evidence;
    }
    if (readFormat == GL_BGRA_EXT) {
        for (size_t pixel = 0; pixel < pixels.size(); pixel += 4)
            std::swap(pixels[pixel], pixels[pixel + 2]);
    }

    renderScope.restore();
    evidence.rendererStateRestored = renderScope.restored();
    if (!evidence.rendererStateRestored) {
        evidence.error = "renderer flags were not restored";
        return evidence;
    }

    for (size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        if (pixels[pixel + 3] != 0)
            ++evidence.nontransparentPixels;
    }

    std::unordered_map<uint32_t, size_t> colorCounts;
    uint32_t                             backgroundKey = 0;
    size_t                               bestCount     = 0;
    for (size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        const auto key = static_cast<uint32_t>(pixels[pixel]) | (static_cast<uint32_t>(pixels[pixel + 1]) << 8) |
            (static_cast<uint32_t>(pixels[pixel + 2]) << 16) | (static_cast<uint32_t>(pixels[pixel + 3]) << 24);
        const auto count = ++colorCounts[key];
        if (count > bestCount) {
            bestCount     = count;
            backgroundKey = key;
        }
    }
    const std::array<uint8_t, 4> background = {static_cast<uint8_t>(backgroundKey), static_cast<uint8_t>(backgroundKey >> 8),
                                                static_cast<uint8_t>(backgroundKey >> 16), static_cast<uint8_t>(backgroundKey >> 24)};

    int minX = evidence.width;
    int minY = evidence.height;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < evidence.height; ++y) {
        for (int x = 0; x < evidence.width; ++x) {
            const auto offset = static_cast<size_t>((y * evidence.width + x) * 4);
            if (std::equal(background.begin(), background.end(), pixels.begin() + offset))
                continue;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }
    if (maxX >= minX && maxY >= minY)
        evidence.nonBackgroundBounds = {static_cast<double>(minX), static_cast<double>(minY), static_cast<double>(maxX - minX + 1), static_cast<double>(maxY - minY + 1)};

    evidence.sha256 = sha256(pixels);
    if (!writePpm(outputPath, evidence.width, evidence.height, pixels))
        evidence.error = "failed to write PPM evidence";
    return evidence;
}

std::optional<std::pair<std::string, std::string>> parseRequest(const std::string& argument) {
    const auto separator = argument.find('|');
    if (separator == std::string::npos || separator == 0 || separator + 1 == argument.size())
        return std::nullopt;
    const auto requestId = argument.substr(0, separator);
    const auto path      = argument.substr(separator + 1);
    if (requestId.size() > 128 || path.size() > 512 || path.find("..") != std::string::npos || !path.starts_with(PATH_PREFIX) || !path.ends_with(".ppm"))
        return std::nullopt;
    return std::pair{requestId, path};
}

SDispatchResult inspect(std::string argument) {
    const auto request = parseRequest(argument);
    if (!request)
        return {.success = false, .error = "expected REQUEST_ID|/tmp/hyprexpo-scroll-probe-*.ppm"};

    const auto generation = ++g_sessionGeneration;
    try {
        const auto before = snapshotTopology();
        if (!before.error.empty())
            throw std::runtime_error(before.error);

        const auto captureWindow = before.captureTarget->window();
        if (!captureWindow)
            throw std::runtime_error("capture target has no live window");
        const auto capture = captureTarget(before.captureTarget, captureWindow, before.monitor, request->second);
        if (!capture.error.empty())
            throw std::runtime_error(capture.error);

        const auto after = snapshotTopology();
        if (!after.error.empty())
            throw std::runtime_error(after.error);

        const bool unchanged = before.canonical == after.canonical;
        std::ostringstream record;
        record << "{\"schema\":1,\"requestId\":" << quoted(request->first) << ",\"sessionGeneration\":" << generation
               << ",\"hyprlandVersion\":" << quoted(EXPECTED_VERSION) << ",\"compileHash\":" << quoted(GIT_COMMIT_HASH)
               << ",\"runtimeHash\":" << quoted(runtimeCommitHash()) << ",\"runtimeAbiHash\":" << quoted(runtimeAbiHash()) << ",\"monitor\":{\"id\":" << before.monitor->m_id
               << ",\"name\":" << quoted(before.monitor->m_name) << ",\"pixelWidth\":" << before.monitor->m_pixelSize.x
               << ",\"pixelHeight\":" << before.monitor->m_pixelSize.y << "},\"workspace\":{\"id\":" << before.workspace->m_id
               << ",\"name\":" << quoted(before.workspace->m_name) << "},\"algorithmIdentity\":" << quoted(identity(before.algorithm))
               << ",\"dataIdentity\":" << quoted(identity(before.data)) << ",\"direction\":" << quoted(directionName(before.direction))
               << ",\"offsetBefore\":" << number(before.offset) << ",\"offsetAfter\":" << number(after.offset)
               << ",\"columnsBefore\":" << before.json << ",\"columnsAfter\":" << after.json << ",\"topologyUnchanged\":" << (unchanged ? "true" : "false")
               << ",\"capture\":{\"targetIdentity\":" << quoted(identity(before.captureTarget)) << ",\"windowIdentity\":" << quoted(identity(captureWindow))
               << ",\"width\":" << capture.width << ",\"height\":" << capture.height << ",\"pixelCount\":" << static_cast<size_t>(capture.width) * capture.height
               << ",\"nontransparentPixels\":" << capture.nontransparentPixels << ",\"nonBackgroundBounds\":" << boxJson(capture.nonBackgroundBounds)
               << ",\"sha256\":" << quoted(capture.sha256) << ",\"imagePath\":" << quoted(capture.imagePath)
               << ",\"rendererStateRestored\":" << (capture.rendererStateRestored ? "true" : "false")
               << "},\"mutationOutcome\":\"not-attempted\",\"rollbackStatus\":\"not-required\",\"status\":" << quoted(unchanged ? "PASS" : "FAIL") << "}";
        Log::logger->log(Log::INFO, "HYPREXPO_SCROLL_PROBE {}", record.str());
        if (!unchanged)
            return {.success = false, .error = "probe changed live scrolling topology or offset"};
        return {};
    } catch (const std::exception& error) {
        std::ostringstream record;
        record << "{\"schema\":1,\"requestId\":" << quoted(request->first) << ",\"sessionGeneration\":" << generation
               << ",\"hyprlandVersion\":" << quoted(EXPECTED_VERSION) << ",\"compileHash\":" << quoted(GIT_COMMIT_HASH)
               << ",\"runtimeHash\":" << quoted(runtimeCommitHash()) << ",\"runtimeAbiHash\":" << quoted(runtimeAbiHash())
               << ",\"mutationOutcome\":\"not-attempted\",\"rollbackStatus\":\"not-required\",\"status\":\"FAIL\",\"error\":"
               << quoted(error.what()) << "}";
        Log::logger->log(Log::INFO, "HYPREXPO_SCROLL_PROBE {}", record.str());
        return {.success = false, .error = error.what()};
    } catch (...) {
        std::ostringstream record;
        record << "{\"schema\":1,\"requestId\":" << quoted(request->first) << ",\"sessionGeneration\":" << generation
               << ",\"hyprlandVersion\":" << quoted(EXPECTED_VERSION) << ",\"compileHash\":" << quoted(GIT_COMMIT_HASH)
               << ",\"runtimeHash\":" << quoted(runtimeCommitHash()) << ",\"runtimeAbiHash\":" << quoted(runtimeAbiHash())
               << ",\"mutationOutcome\":\"not-attempted\",\"rollbackStatus\":\"not-required\",\"status\":\"FAIL\",\"error\":\"unknown exception\"}";
        Log::logger->log(Log::INFO, "HYPREXPO_SCROLL_PROBE {}", record.str());
        return {.success = false, .error = "unknown exception"};
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
    const std::vector<uint8_t> shaSelfTest = {'a', 'b', 'c'};
    if (sha256(shaSelfTest) != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
        throw std::runtime_error("scrolling API probe SHA-256 self-test failed");
    if (!HyprlandAPI::addDispatcherV2(g_handle, std::string{DISPATCHER}, inspect))
        throw std::runtime_error("failed to register scrolling API probe dispatcher");
    return {"hyprexpo-scroll-probe", "Read-only native scrolling topology and tight capture probe", "sandwich", "0.1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    if (g_handle)
        HyprlandAPI::removeDispatcher(g_handle, std::string{DISPATCHER});
    g_handle = nullptr;
}
