#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <cstdint>

class IOverviewSession;

class COverviewPassElement : public IPassElement {
  public:
    // A replacement session on the same monitor must never inherit a queued pass.
    COverviewPassElement(PHLMONITOR monitor, uint64_t sessionGeneration);
    virtual ~COverviewPassElement() = default;

    std::vector<UP<IPassElement>> draw(Render::CRenderContext& ctx) override;
    bool needsLiveBlur(Render::CRenderContext& ctx) override;
    bool needsPrecomputeBlur(Render::CRenderContext& ctx) override;
    std::optional<CBox> boundingBox(Render::CRenderContext& ctx) override;
    CRegion opaqueRegion(Render::CRenderContext& ctx) override;
    ePassElementType type() override;
    const char* passName() override { return "COverviewPassElement"; }

  private:
    IOverviewSession* overview() const;
    PHLMONITORREF m_monitor;
    uint64_t m_sessionGeneration = 0;
};
