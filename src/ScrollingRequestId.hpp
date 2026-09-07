#pragma once

#include <string_view>

namespace Hyprexpo::Scrolling {

inline bool validRequestID(std::string_view value) {
    if (value.empty() || value.size() > 64)
        return false;

    for (const unsigned char c : value) {
        const bool asciiLetter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        const bool asciiDigit  = c >= '0' && c <= '9';
        const bool separator   = c == '.' || c == '_' || c == '-';
        if (!asciiLetter && !asciiDigit && !separator)
            return false;
    }
    return true;
}

}
