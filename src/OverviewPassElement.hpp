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

    std::vector<UP<IPassElement>> draw() override;
    bool needsLiveBlur() override;
    bool needsPrecomputeBlur() override;
    std::optional<CBox> boundingBox() override;
    CRegion opaqueRegion() override;
    ePassElementType type() override;
    const char* passName() override { return "COverviewPassElement"; }

  private:
    IOverviewSession* overview() const;
    PHLMONITORREF m_monitor;
    uint64_t m_sessionGeneration = 0;
};
