#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Hyprexpo::Background {

struct SPlacement {
    double x, y, width, height;
};

inline SPlacement placement(double imageWidth, double imageHeight, double width, double height, std::string_view fit) {
    if (imageWidth <= 0 || imageHeight <= 0 || width <= 0 || height <= 0)
        return {};
    if (fit == "stretch")
        return {0, 0, width, height};
    const double scale = fit == "contain" ? std::min(width / imageWidth, height / imageHeight) : std::max(width / imageWidth, height / imageHeight);
    return {(width - imageWidth * scale) / 2, (height - imageHeight * scale) / 2, imageWidth * scale, imageHeight * scale};
}

// Three separable box passes approximate a Gaussian. Work on premultiplied
// channels, clamp edges, and keep cost independent of the configured radius.
inline void blur(uint8_t* pixels, int width, int height, int stride, int radius) {
    radius = std::clamp(radius, 0, 64);
    if (!pixels || width <= 0 || height <= 0 || radius == 0)
        return;
    std::vector<uint8_t> temporary(static_cast<size_t>(width) * height * 4);
    const int divisor = radius * 2 + 1;
    for (int pass = 0; pass < 3; ++pass) {
        for (int axis = 0; axis < 2; ++axis) {
            const int length = axis == 0 ? width : height;
            const int lines = axis == 0 ? height : width;
            for (int line = 0; line < lines; ++line) {
                const auto source = [&](int position) -> const uint8_t* {
                    position = std::clamp(position, 0, length - 1);
                    return axis == 0 ? pixels + static_cast<size_t>(line) * stride + position * 4 :
                                       temporary.data() + (static_cast<size_t>(position) * width + line) * 4;
                };
                std::array<int, 4> sum{};
                for (int offset = -radius; offset <= radius; ++offset) {
                    const auto* pixel = source(offset);
                    for (int channel = 0; channel < 4; ++channel)
                        sum[channel] += pixel[channel];
                }
                for (int position = 0; position < length; ++position) {
                    auto* destination = axis == 0 ? temporary.data() + (static_cast<size_t>(line) * width + position) * 4 :
                                                   pixels + static_cast<size_t>(position) * stride + line * 4;
                    const auto* added = source(position + radius + 1);
                    const auto* removed = source(position - radius);
                    for (int channel = 0; channel < 4; ++channel) {
                        destination[channel] = (sum[channel] + divisor / 2) / divisor;
                        sum[channel] += added[channel] - removed[channel];
                    }
                }
            }
        }
    }
}

}
