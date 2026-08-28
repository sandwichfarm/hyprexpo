#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>
#include <cstdint>

class COverviewPassElement : public IPassElement {
  public:
    explicit COverviewPassElement(uint64_t sessionGeneration);
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
    uint64_t m_sessionGeneration = 0;
};
