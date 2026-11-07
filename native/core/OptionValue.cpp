#include "tkd2dcanvas/OptionValue.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <charconv>

std::string OptionValue::ToTclString() const {
    if (const auto* s = std::get_if<std::string>(&value_)) return *s;
    if (const auto* n = std::get_if<double>(&value_)) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%g", *n);
        return std::string(buf);
    }
    return std::get<bool>(value_) ? "1" : "0";
}

std::optional<double> OptionValue::ToNumber() const {
    if (const auto* n = std::get_if<double>(&value_)) return *n;
    if (const auto* b = std::get_if<bool>(&value_)) return *b ? 1.0 : 0.0;
    const auto* s = std::get_if<std::string>(&value_);
    if (!s) return std::nullopt;
    char* end = nullptr;
    errno = 0;
    const double v = std::strtod(s->c_str(), &end);
    if (errno != 0 || end == s->c_str()) return std::nullopt;
    return v;
}

std::optional<bool> OptionValue::ToBoolean() const {
    if (const auto* b = std::get_if<bool>(&value_)) return *b;
    if (const auto* n = std::get_if<double>(&value_)) return *n != 0.0;
    const auto* s = std::get_if<std::string>(&value_);
    if (!s) return std::nullopt;
    if (*s == "1" || *s == "true" || *s == "True" || *s == "TRUE" || *s == "yes" || *s == "on") return true;
    if (*s == "0" || *s == "false" || *s == "False" || *s == "FALSE" || *s == "no" || *s == "off") return false;
    return std::nullopt;
}

std::string OptionValue::ToString() const {
    return ToTclString();
}
