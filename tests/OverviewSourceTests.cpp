#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string& label) {
    if (condition)
        return;

    ++failures;
    std::cerr << "FAIL: " << label << '\n';
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file)
        return {};

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string extractFunction(const std::string& source, const std::string& signature) {
    const auto start = source.find(signature);
    if (start == std::string::npos)
        return {};

    const auto open = source.find('{', start);
    if (open == std::string::npos)
        return {};

    int depth = 0;
    for (size_t i = open; i < source.size(); ++i) {
        if (source[i] == '{')
            ++depth;
        else if (source[i] == '}') {
            --depth;
            if (depth == 0)
                return source.substr(start, i - start + 1);
        }
    }

    return {};
}

}

int main() {
    const auto source = readFile("Overview.cpp");
    expect(!source.empty(), "Overview.cpp can be read from repo root");

    const auto function = extractFunction(source, "void removeOverview(");
    expect(!function.empty(), "removeOverview function exists");

    const auto lockPos     = function.find("const auto MON = g_pOverview->pMonitor.lock();");
    const auto resetPos    = function.find("g_pOverview.reset();");
    const auto damagePos   = function.find("g_pHyprRenderer->damageMonitor(MON);");
    const auto schedulePos = function.find("g_pCompositor->scheduleFrameForMonitor(MON);");

    expect(lockPos != std::string::npos, "removeOverview captures monitor before teardown");
    expect(resetPos != std::string::npos, "removeOverview destroys active overview");
    expect(damagePos != std::string::npos, "removeOverview damages monitor after teardown");
    expect(schedulePos != std::string::npos, "removeOverview schedules a frame after teardown");
    expect(lockPos < resetPos, "monitor is captured before overview reset");
    expect(resetPos < damagePos, "monitor damage happens after overview reset");
    expect(damagePos < schedulePos, "frame scheduling follows monitor damage");

    if (failures != 0)
        return 1;

    std::cout << "OverviewSourceTests passed\n";
    return 0;
}
