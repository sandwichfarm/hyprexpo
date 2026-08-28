#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view PROBE_PATH = "spikes/scrolling-api/ScrollingApiProbe.cpp";

std::string readFile(const std::string_view path) {
    std::ifstream input{std::string{path}};
    if (!input)
        return {};

    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void requireContains(const std::string& source, const std::string_view token) {
    if (source.find(token) != std::string::npos)
        return;

    std::cerr << "missing required probe contract token: " << token << '\n';
    std::exit(1);
}

void requireAbsent(const std::string& source, const std::string_view token) {
    if (source.find(token) == std::string::npos)
        return;

    std::cerr << "forbidden mutating probe token: " << token << '\n';
    std::exit(1);
}

} // namespace

int main() {
    const auto source = readFile(PROBE_PATH);
    if (source.empty()) {
        std::cerr << "probe source is missing: " << PROBE_PATH << '\n';
        return 1;
    }

    for (const auto token : std::vector<std::string_view>{
             "5c9377c15f85c50648f35ca5a213754f95b93ca0",
             "HYPREXPO_SCROLL_PROBE",
             "algoMatcher()->getNameForTiledAlgo",
             "dynamic_cast<Layout::Tiled::CScrollingAlgorithm*>",
             "dataFor(target)",
             "stripCount()",
             "getDirection()",
             "getOffset()",
             "getStrip(",
             "calculateStripStart(",
             "calculateStripSize(",
             "beginFullFakeRender(",
             "renderWindow(",
             "endRender()",
             "glReadPixels(",
             "CRenderScope",
             "offsetBefore",
             "offsetAfter",
             "columnsBefore",
             "columnsAfter",
             "nontransparentPixels",
             "nonBackgroundBounds",
             "sha256",
             "mutationOutcome",
             "rollbackStatus",
         })
        requireContains(source, token);

    for (const auto token : std::vector<std::string_view>{
             ".moveTape(",
             ".moveTapeNormalized(",
             ".snapToGrid(",
             ".snapToProjectedOffset(",
             ".focusColumn(",
             ".centerCol(",
             ".fitCol(",
             ".centerOrFitCol(",
             ".setOffset(",
             ".adjustOffset(",
             ".addStrip(",
             ".insertStrip(",
             ".removeStrip(",
             ".swapStrips(",
             ".setColumnWidth(",
             ".setTargetSize(",
             ".recalculate(",
         })
        requireAbsent(source, token);

    std::cout << "Scrolling probe source contract PASS\n";
    return 0;
}
