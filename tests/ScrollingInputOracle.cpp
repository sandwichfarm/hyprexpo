#include "../ScrollingInputState.hpp"
#include "../ScrollingOverviewLogic.hpp"

#include <iostream>

using namespace Hyprexpo;
using namespace Hyprexpo::Scrolling;

namespace {

STapeSpec tape() {
    return {
        .direction = EDirection::Right,
        .columns = {
            {.token = 10, .extent = 300.0, .targets = {{.token = 101, .proportion = 1.0}}},
            {.token = 20, .extent = 200.0, .targets = {{.token = 201, .proportion = 1.0}, {.token = 202, .proportion = 3.0}}},
            {.token = 30, .extent = 400.0, .targets = {{.token = 301, .proportion = 1.0}}},
        },
    };
}

SScene fixtureScene() {
    return buildScene(
        {
            {.workspaceID = 1, .kind = EWorkspaceKind::Scrolling, .tape = tape()},
            {.workspaceID = 2, .kind = EWorkspaceKind::Empty, .tape = {}},
            {.workspaceID = 3, .kind = EWorkspaceKind::Mixed, .tape = {}},
        },
        1,
        {.viewportWidth = 1000.0, .viewportHeight = 500.0, .rowHeight = 150.0, .rowGap = 10.0, .columnGap = 10.0, .terminalWorkspaceID = 4});
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "expected one requestId|event... sequence\n";
        return 2;
    }

    const auto parsed = parseInputSequence(argv[1]);
    if (!parsed.valid) {
        std::cerr << parsed.error << '\n';
        return 2;
    }

    const auto scene = fixtureScene();
    SInputState state;
    SInputContext context{
        .scene = &scene,
        .monitor = {.position = {}, .logicalSize = {1000.0, 500.0}, .pixelSize = {1000.0, 500.0}, .scale = 1.0},
        .pan = 0.0,
        .viewportHeight = 500.0,
        .dragThreshold = 12.0,
    };
    bool hasDropIntent = false;
    std::vector<SInputDiagnosticRecord> records;
    records.reserve(parsed.steps.size());
    for (const auto& step : parsed.steps) {
        const auto transition = step.reset ? resetInput(state, *step.reset) : transitionInput(state, *step.event, context);
        state = transition.state;
        context.pan += transition.effects.panDelta;
        if (transition.effects.dropIntent)
            hasDropIntent = true;
        if (transition.effects.cancelDrag || step.reset)
            hasDropIntent = false;
        records.push_back({.state = state, .effects = transition.effects, .pan = context.pan});
    }

    std::cout << inputDiagnosticJson(parsed.requestId, records, state, hasDropIntent) << '\n';
    return 0;
}
