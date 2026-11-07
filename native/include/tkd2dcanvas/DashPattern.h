// DashPattern.h — stroke dash pattern (Tk -dash option compatible shape).
#pragma once

#include <vector>
#include <string>

struct DashPattern {
    // Dash lengths in canvas units. Empty means solid.
    std::vector<float> dashes;
    float offset = 0.0f;

    bool IsSolid() const { return dashes.empty(); }

    // Tk accepts "-dash {4 2}" or the named patterns dot/dash/... ; conversion
    // lives in native/render/DashConverter.cpp.
    static DashPattern Solid() { return DashPattern{}; }
    static DashPattern FromList(const std::vector<float>& values) {
        DashPattern p;
        p.dashes = values;
        return p;
    }
};
