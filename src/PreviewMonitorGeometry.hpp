#pragma once

namespace Hyprexpo::Capture {

template <typename Monitor, typename Transform, typename Size>
void setMonitorGeometry(Monitor& monitor, Transform transform, const Size& pixelSize, const Size& transformedSize) {
    const bool changed = monitor.m_transform != transform || monitor.m_pixelSize != pixelSize || monitor.m_transformedSize != transformedSize;
    monitor.m_transform = transform;
    monitor.m_pixelSize = pixelSize;
    monitor.m_transformedSize = transformedSize;
    // Drawing uses cached matrices while clipping reads the geometry directly.
    if (changed)
        monitor.updateMatrix();
}

}
