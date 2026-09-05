#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>

class COverview;

class COverviewPassElement : public IPassElement {
  public:
    // The pass element can outlive its overview, so it stores the monitor and
    // resolves the owner per call instead of holding a dangling pointer.
    explicit COverviewPassElement(PHLMONITOR monitor);
    virtual ~COverviewPassElement() = default;

    virtual std::vector<UP<IPassElement>> draw();
    virtual bool                          needsLiveBlur();
    virtual bool                          needsPrecomputeBlur();
    virtual std::optional<CBox>           boundingBox();
    virtual CRegion                       opaqueRegion();
    virtual ePassElementType              type();

    virtual const char*                   passName() {
        return "COverviewPassElement";
    }

  private:
    COverview*    overview() const;

    PHLMONITORREF m_monitor;
};
