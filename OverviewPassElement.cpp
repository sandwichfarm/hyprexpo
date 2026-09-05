#include "OverviewPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include "Overview.hpp"

COverviewPassElement::COverviewPassElement(PHLMONITOR monitor) : m_monitor(monitor) {
    ;
}

COverview* COverviewPassElement::overview() const {
    return overviewForMonitor(m_monitor.lock());
}

std::vector<UP<IPassElement>> COverviewPassElement::draw() {
    if (auto* const OV = overview())
        OV->fullRender();

    return {};
}

bool COverviewPassElement::needsLiveBlur() {
    return false;
}

bool COverviewPassElement::needsPrecomputeBlur() {
    return false;
}

std::optional<CBox> COverviewPassElement::boundingBox() {
    const auto MON = m_monitor.lock();
    if (!overview() || !MON)
        return std::nullopt;

    return CBox{{}, MON->m_size};
}

CRegion COverviewPassElement::opaqueRegion() {
    const auto MON = m_monitor.lock();
    if (!overview() || !MON)
        return CRegion{};

    return CBox{{}, MON->m_size};
}

ePassElementType COverviewPassElement::type() {
    return EK_CUSTOM;
}
