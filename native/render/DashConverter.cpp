#include "DashConverter.h"

#include <cstdlib>
#include <sstream>

namespace DashConverter
{

DashPattern FromListString(const std::string& text) {
    DashPattern pattern;
    std::istringstream iss(text);
    std::string token;
    while (iss >> token) {
        char* end = nullptr;
        const double v = std::strtod(token.c_str(), &end);
        if (end == token.c_str()) return DashPattern::Solid();  // malformed
        pattern.dashes.push_back(static_cast<float>(v));
    }
    return pattern;
}

bool FromNamedPattern(char name, DashPattern& out) {
    switch (name) {
        case '.': out = DashPattern::FromList({2.0f, 2.0f}); return true;   // dot
        case '-': out = DashPattern::FromList({6.0f, 4.0f}); return true;   // dash
        default: return false;
    }
}

bool LooksLikeList(const std::string& text) {
    for (char c : text) {
        if (!(c == ' ' || c == '\t' || (c >= '0' && c <= '9') || c == '.' || c == '-')) {
            return false;
        }
    }
    return !text.empty();
}

}  // namespace DashConverter
