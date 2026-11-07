// DashConverter.h — converts Tk -dash option values into DashPattern.
// Tk accepts: "-dash {4 2}", "-dash .", "-dash -", and named patterns
// (dot/dash/...). Pixels are in canvas units.
#pragma once

#include <string>
#include <vector>

#include "tkd2dcanvas/DashPattern.h"

namespace DashConverter
{
    // Parses a Tcl-list-like string such as "4 2 1 2". Returns solid pattern
    // on empty/invalid input (Tk silently ignores malformed -dash values).
    DashPattern FromListString(const std::string& text);

    // Named patterns used by Tk's "-dash" shorthand (single char: . and -).
    bool FromNamedPattern(char name, DashPattern& out);

    // True when the string looks like a numeric Tcl list.
    bool LooksLikeList(const std::string& text);
}
