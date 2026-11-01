#include "OverviewPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include "IOverviewSession.hpp"

COverviewPassElement::COverviewPassElement(PHLMONITOR monitor, uint64_t sessionGeneration) :
    m_monitor(monitor), m_sessionGeneration(sessionGeneration) {}

IOverviewSession* COverviewPassElement::overview() const {
    return overviewForSession(overviewMonitorKey(m_monitor.lock()), m_sessionGeneration);
}

std::vector<UP<IPassElement>> COverviewPassElement::draw(Render::CRenderContext& ctx) {
    if (auto* const OV = overview())
        OV->fullRender(ctx);

    return {};
}

bool COverviewPassElement::needsLiveBlur(Render::CRenderContext&) {
    return false;
}

bool COverviewPassElement::needsPrecomputeBlur(Render::CRenderContext&) {
    return false;
}

std::optional<CBox> COverviewPassElement::boundingBox(Render::CRenderContext&) {
    const auto MON = m_monitor.lock();
    if (!overview() || !MON)
        return std::nullopt;

    return CBox{{}, MON->m_size};
}

CRegion COverviewPassElement::opaqueRegion(Render::CRenderContext&) {
    const auto MON = m_monitor.lock();
    if (!overview() || !MON)
        return CRegion{};

    return CBox{{}, MON->m_size};
}

ePassElementType COverviewPassElement::type() {
    return EK_CUSTOM;
}
