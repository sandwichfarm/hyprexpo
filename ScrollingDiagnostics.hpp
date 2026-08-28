#pragma once

#include "ScrollingLayoutAdapter.hpp"

#include <string>

namespace Hyprexpo::Scrolling {

struct SDiagnosticRequest {
    bool        valid = false;
    std::string requestID;
    std::string selector;
    std::string error;
};

struct SDiagnosticEmission {
    bool        validRequest = false;
    bool        success = false;
    std::string requestID;
    std::string error;
    std::string json;
};

SDiagnosticRequest  parseDiagnosticRequest(const std::string& argument);
bool                snapshotsEquivalent(const SWorkspaceSnapshot& before, const SWorkspaceSnapshot& after);
SDiagnosticEmission buildReadDiagnostic(const std::string& argument);

}
