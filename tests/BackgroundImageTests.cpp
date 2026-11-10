#include "../src/BackgroundImageLogic.hpp"

#include <cmath>
#include <iostream>
#include <vector>

int main() {
    using namespace Hyprexpo::Background;
    int failures = 0;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    };
    const auto cover = placement(400, 200, 100, 100, "cover");
    expect(cover.x == -50 && cover.y == 0 && cover.width == 200 && cover.height == 100, "cover crops centrally without distortion");
    const auto contain = placement(400, 200, 100, 100, "contain");
    expect(contain.x == 0 && contain.y == 25 && contain.width == 100 && contain.height == 50, "contain centers letterboxed image");
    const auto portrait = placement(200, 400, 100, 100, "cover");
    expect(portrait.x == 0 && portrait.y == -50 && portrait.width == 100 && portrait.height == 200, "portrait cover crops vertically");
    const auto stretch = placement(400, 200, 100, 100, "stretch");
    expect(stretch.x == 0 && stretch.y == 0 && stretch.width == 100 && stretch.height == 100, "stretch fills both axes");
    expect(placement(400, 200, 100, 100, "invalid").x == cover.x, "invalid fit falls back to cover");
    expect(placement(0, 200, 100, 100, "cover").width == 0, "empty input is rejected");

    // Padded rows must remain untouched, including at a radius larger than the image.
    for (int radius : {0, 1, 64, 100}) {
        std::vector<uint8_t> pixels(3 * 16, 77);
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x) {
                pixels[y * 16 + x * 4] = 20;
                pixels[y * 16 + x * 4 + 1] = 40;
                pixels[y * 16 + x * 4 + 2] = 60;
                pixels[y * 16 + x * 4 + 3] = 255;
            }
        const auto original = pixels;
        blur(pixels.data(), 3, 3, 16, radius);
        expect(pixels == original, "blur preserves flat colors, opacity, and row padding");
    }
    std::vector<uint8_t> impulse(7 * 7 * 4, 0);
    for (int i = 0; i < 49; ++i)
        impulse[i * 4 + 3] = 255;
    impulse[(3 * 7 + 3) * 4] = 255;
    auto disabled = impulse;
    blur(disabled.data(), 7, 7, 28, 0);
    expect(disabled == impulse, "zero blur leaves the image unchanged");
    blur(impulse.data(), 7, 7, 28, 1);
    const auto red = [&](int x, int y) { return impulse[(y * 7 + x) * 4]; };
    expect(red(3, 3) > 0 && red(3, 3) < 255 && red(2, 3) > 0 && red(3, 2) > 0, "blur spreads an impulse across both axes");
    expect(red(2, 3) == red(4, 3) && red(3, 2) == red(3, 4), "blur stays symmetric");
    for (int i = 0; i < 49; ++i)
        expect(impulse[i * 4 + 3] == 255, "blur preserves opaque alpha");
    uint8_t pixel[] = {7, 11, 13, 17};
    blur(pixel, 1, 1, 4, 64);
    expect(pixel[0] == 7 && pixel[3] == 17, "one-pixel images clamp edges safely");
    blur(nullptr, 0, 0, 0, 1);
    if (!failures)
        std::cout << "Background image tests passed\n";
    return failures ? 1 : 0;
}
