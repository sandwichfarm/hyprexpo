#pragma once

#include "globals.hpp"

#include <hyprland/src/config/ConfigValue.hpp>

#include <string>

namespace CompatHyprlandAPI {
struct SConfigValueCompat {
    Config::INTEGER integer = 0;
    Config::INTEGER* integerPtr = &integer;
    Config::FLOAT floating = 0;
    Config::FLOAT* floatingPtr = &floating;
    Config::STRING string;
    void* ptr = nullptr;

    void* getDataStaticPtr() {
        return ptr;
    }
};

SConfigValueCompat* getConfigValue(HANDLE handle, const std::string& name);

// Backend-independent reads. Both fall back to the compiled-in default when the config manager
// has no value of the expected type, so callers never dereference a null or mistyped pointer.
Config::INTEGER intValue(const std::string& name);
Config::STRING  stringValue(const std::string& name);
}
