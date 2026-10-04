import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class RenderContextTests(unittest.TestCase):
    def compile_and_run(self, source):
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler, "C++ compiler required for the render-context regression")
        with tempfile.TemporaryDirectory(prefix="hyprexpo-render-context-") as directory:
            path = pathlib.Path(directory)
            (path / "test.cpp").write_text(source)
            build = subprocess.run([compiler, "-std=c++23", "-Wall", "-Wextra", "-Werror", str(path / "test.cpp"), "-o", str(path / "test")], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            result = subprocess.run([str(path / "test")], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_capture_restores_context_flags_on_success_and_early_exit(self):
        source = (ROOT / "src/OverviewCapture.cpp").read_text()
        guard = "class CRendererStateGuard" + source.split("class CRendererStateGuard", 1)[1].split("class CMonitorStateGuard", 1)[0]
        fixture = r'''
#include <cassert>
namespace Render {
struct CRenderContext {
    bool m_blockSurfaceFeedback = false, m_renderingSnapshot = false;
    struct { bool blockScreenShader = false; } m_data;
};
struct IHyprRenderer {
    CRenderContext ctx;
    int ends = 0;
    CRenderContext& context() { return ctx; }
    void endRender() { ++ends; ctx = {}; }
};
}
'''
        checks = r'''
int main() {
    for (int flags = 0; flags < 8; ++flags) {
        Render::IHyprRenderer renderer;
        const auto setOriginal = [&] {
            renderer.ctx.m_blockSurfaceFeedback = flags & 1;
            renderer.ctx.m_renderingSnapshot = flags & 2;
            renderer.ctx.m_data.blockScreenShader = flags & 4;
        };
        const auto checkOriginal = [&] {
            assert(renderer.ctx.m_blockSurfaceFeedback == bool(flags & 1));
            assert(renderer.ctx.m_renderingSnapshot == bool(flags & 2));
            assert(renderer.ctx.m_data.blockScreenShader == bool(flags & 4));
        };
        setOriginal();
        {
            CRendererStateGuard guard(&renderer);
            guard.begun();
            renderer.ctx = {};
            guard.finish();
            guard.restore();
            assert(guard.restored());
            checkOriginal();
        }
        assert(renderer.ends == 1);
        checkOriginal();
        {
            CRendererStateGuard guard(&renderer);
            guard.begun();
            renderer.ctx = {};
        }
        assert(renderer.ends == 2);
        checkOriginal();
        {
            CRendererStateGuard guard(&renderer);
            renderer.ctx = {};
        }
        assert(renderer.ends == 2);
        checkOriginal();
    }
}
'''
        self.compile_and_run(fixture + guard + checks)

    def test_exit_removes_plugin_passes_from_root_and_redirected_context(self):
        source = (ROOT / "src/main.cpp").read_text().split("APICALL EXPORT void PLUGIN_EXIT()", 1)[1]
        fixture = r'''
#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <vector>
static bool disabled = false, destroyed = false, reloaded = false;
struct Pass {
    std::vector<std::string> elements = {"COverviewPassElement", "native", "COverviewPassElement"};
    int removals = 0;
    void removeAllOfType(const std::string& name) {
        assert(disabled && destroyed && !reloaded);
        ++removals;
        std::erase(elements, name);
    }
};
struct Context { Pass m_pass; Pass* m_currentPass = nullptr; };
struct Renderer { Context ctx; Context& context() { return ctx; } };
static auto g_pHyprRenderer = std::make_unique<Renderer>();
static void disableExpoGestureRegistration() { disabled = true; }
static void destroyAllOverviews() { assert(disabled); destroyed = true; }
namespace Config {
    struct Manager { void reload() { reloaded = true; } };
    static Manager* mgr() { static Manager instance; return &instance; }
}
static void resetDispatcherRuntime() { assert(reloaded); }
'''
        checks = r'''
int main() {
    Pass redirected;
    g_pHyprRenderer->ctx.m_currentPass = &redirected;
    PLUGIN_EXIT();
    assert(g_pHyprRenderer->ctx.m_pass.elements == std::vector<std::string>{"native"});
    assert(redirected.elements == std::vector<std::string>{"native"});
    assert(redirected.removals == 1);
    reloaded = false;
    g_pHyprRenderer->ctx.m_currentPass = &g_pHyprRenderer->ctx.m_pass;
    PLUGIN_EXIT();
    assert(g_pHyprRenderer->ctx.m_pass.removals == 2);
    reloaded = false;
    g_pHyprRenderer->ctx.m_currentPass = nullptr;
    PLUGIN_EXIT();
    assert(g_pHyprRenderer->ctx.m_pass.removals == 3);
}
'''
        self.compile_and_run(fixture + "void PLUGIN_EXIT()" + source + checks)

    def test_queued_pass_preserves_context_and_rejects_replaced_session(self):
        source = (ROOT / "src/OverviewPassElement.cpp").read_text()
        header = (ROOT / "src/OverviewPassElement.hpp").read_text()
        # Compile the production pass against only the upstream interface it uses.
        source = "\n".join(line for line in source.splitlines() if not line.startswith("#include"))
        header = "\n".join(line for line in header.splitlines() if not line.startswith(("#include", "#pragma")))
        fixture = r'''
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
template<class T> using UP = std::unique_ptr<T>;
struct Vec { double x = 0, y = 0; };
struct CBox { Vec pos, size; };
struct CRegion { CRegion() = default; CRegion(CBox) {} };
struct Monitor { Vec m_size = {1920, 1080}; };
using PHLMONITOR = std::shared_ptr<Monitor>;
using PHLMONITORREF = std::weak_ptr<Monitor>;
namespace Render { struct CRenderContext { int identity; }; }
enum ePassElementType { EK_CUSTOM };
struct IPassElement {
    virtual ~IPassElement() = default;
    virtual std::vector<UP<IPassElement>> draw(Render::CRenderContext&) = 0;
    virtual bool needsLiveBlur(Render::CRenderContext&) = 0;
    virtual bool needsPrecomputeBlur(Render::CRenderContext&) = 0;
    virtual std::optional<CBox> boundingBox(Render::CRenderContext&) = 0;
    virtual CRegion opaqueRegion(Render::CRenderContext&) = 0;
    virtual ePassElementType type() = 0;
    virtual const char* passName() = 0;
};
struct IOverviewSession {
    int draws = 0;
    Render::CRenderContext* lastContext = nullptr;
    void fullRender(Render::CRenderContext& ctx) { ++draws; lastContext = &ctx; }
};
static IOverviewSession* active = nullptr;
static uint64_t generation = 7;
static Monitor* registeredMonitor = nullptr;
static Monitor* overviewMonitorKey(const PHLMONITOR& monitor) { return monitor.get(); }
static IOverviewSession* overviewForSession(Monitor* monitor, uint64_t requested) {
    return monitor && monitor == registeredMonitor && requested == generation ? active : nullptr;
}
'''
        checks = r'''
int main() {
    auto monitor = std::make_shared<Monitor>();
    registeredMonitor = monitor.get();
    IOverviewSession original, replacement;
    active = &original;
    COverviewPassElement pass(monitor, generation);
    Render::CRenderContext primary{1}, isolated{2};
    pass.draw(primary);
    assert(original.draws == 1 && original.lastContext == &primary);
    pass.draw(isolated);
    assert(original.draws == 2 && original.lastContext == &isolated);
    assert(!pass.needsLiveBlur(isolated) && !pass.needsPrecomputeBlur(isolated));
    assert(pass.boundingBox(isolated).has_value());
    active = &replacement;
    ++generation;
    pass.draw(isolated);
    assert(replacement.draws == 0 && original.draws == 2);
    assert(!pass.boundingBox(isolated).has_value());
    COverviewPassElement replacementPass(monitor, generation);
    monitor.reset();
    replacementPass.draw(primary);
    assert(replacement.draws == 0);
    assert(!replacementPass.boundingBox(primary).has_value());
}
'''
        self.compile_and_run(fixture + header + source + checks)


if __name__ == "__main__":
    unittest.main()
