// OptionValue.h — type-erased option value for Tk widget/item configuration.
// No external dependencies.
#pragma once

#include <string>
#include <variant>
#include <optional>

class OptionValue {
public:
    OptionValue() = default;
    explicit OptionValue(const std::string& s) : value_(s) {}
    explicit OptionValue(const char* s) : value_(std::string(s)) {}
    explicit OptionValue(double n) : value_(n) {}
    explicit OptionValue(int n) : value_(static_cast<double>(n)) {}
    explicit OptionValue(bool b) : value_(b) {}

    static OptionValue String(const std::string& s) { return OptionValue(s); }
    static OptionValue Number(double n) { return OptionValue(n); }
    static OptionValue Boolean(bool b) { return OptionValue(b); }

    bool IsString() const { return std::holds_alternative<std::string>(value_); }
    bool IsNumber() const { return std::holds_alternative<double>(value_); }
    bool IsBoolean() const { return std::holds_alternative<bool>(value_); }

    const std::string* AsString() const { return std::get_if<std::string>(&value_); }
    const double* AsNumber() const { return std::get_if<double>(&value_); }
    const bool* AsBoolean() const { return std::get_if<bool>(&value_); }

    std::string ToTclString() const;

    std::optional<double> ToNumber() const;
    std::optional<bool> ToBoolean() const;
    std::string ToString() const;

private:
    std::variant<std::string, double, bool> value_ = std::string{};
};
