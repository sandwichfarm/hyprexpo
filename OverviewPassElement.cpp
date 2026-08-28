#include "OverviewPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include "IOverviewSession.hpp"

COverviewPassElement::COverviewPassElement(uint64_t sessionGeneration) : m_sessionGeneration(sessionGeneration) {}

std::vector<UP<IPassElement>> COverviewPassElement::draw() {
    if (!g_pOverview || g_pOverview->sessionGeneration() != m_sessionGeneration)
        return {};

    g_pOverview->fullRender();
    return {};
}

bool COverviewPassElement::needsLiveBlur() {
    return false;
}

bool COverviewPassElement::needsPrecomputeBlur() {
    return false;
}

std::optional<CBox> COverviewPassElement::boundingBox() {
    if (!g_pOverview || g_pOverview->sessionGeneration() != m_sessionGeneration)
        return std::nullopt;

    const auto MON = g_pOverview->monitor();
    if (!MON)
        return std::nullopt;

    return CBox{{}, MON->m_size};
}

CRegion COverviewPassElement::opaqueRegion() {
    if (!g_pOverview || g_pOverview->sessionGeneration() != m_sessionGeneration)
        return CRegion{};

    const auto MON = g_pOverview->monitor();
    if (!MON)
        return CRegion{};

    return CBox{{}, MON->m_size};
}

ePassElementType COverviewPassElement::type() {
    return EK_CUSTOM;
}
