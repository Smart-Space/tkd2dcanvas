#include "PenCache.h"

#include <cstdio>
#include <vector>

namespace {
D2D1_CAP_STYLE ToD2DCap(CapStyle cap) {
    switch (cap) {
        case CapStyle::Round: return D2D1_CAP_STYLE_ROUND;
        case CapStyle::Square: return D2D1_CAP_STYLE_SQUARE;
        default: return D2D1_CAP_STYLE_FLAT;
    }
}

D2D1_LINE_JOIN ToD2DJoin(JoinStyle join) {
    switch (join) {
        case JoinStyle::Round: return D2D1_LINE_JOIN_ROUND;
        case JoinStyle::Bevel: return D2D1_LINE_JOIN_BEVEL;
        default: return D2D1_LINE_JOIN_MITER;
    }
}
}  // namespace

PenCache& PenCache::Instance() {
    static PenCache cache;
    return cache;
}

std::string PenCache::KeyOf(const PenStyle& style) const {
    std::string key;
    BuildKey(style, key);
    return key;
}

void PenCache::BuildKey(const PenStyle& style, std::string& out) {
    // The key is built by hand: a string stream would allocate for every
    // stroke, and a repaint draws a lot of strokes.
    out.clear();
    char buffer[64];
    int written = std::snprintf(buffer, sizeof(buffer), "%g@",
                                static_cast<double>(style.width));
    if (written > 0) out.append(buffer, static_cast<std::size_t>(written));
    for (float d : style.dash.dashes) {
        written = std::snprintf(buffer, sizeof(buffer), "%g,",
                                static_cast<double>(d));
        if (written > 0) out.append(buffer, static_cast<std::size_t>(written));
    }
    written = std::snprintf(buffer, sizeof(buffer), "%g|%d|%d|%c",
                            static_cast<double>(style.dash.offset),
                            static_cast<int>(style.cap),
                            static_cast<int>(style.join),
                            style.antialias ? 'A' : 'L');
    if (written > 0) out.append(buffer, static_cast<std::size_t>(written));
}

Microsoft::WRL::ComPtr<ID2D1StrokeStyle> PenCache::GetStrokeStyle(ID2D1Factory* factory,
                                                                  const PenStyle& style) {
    if (!factory) return nullptr;

    // The key buffer is a member: looking a stroke style up must not
    // allocate, otherwise every stroke of every repaint would.
    BuildKey(style, key_);
    auto it = styles_.find(key_);
    if (it != styles_.end()) return it->second;

    const D2D1_CAP_STYLE cap = ToD2DCap(style.cap);

    const double width = style.width > 0.0 ? style.width : 1.0;
    std::vector<float> d2dDashes;
    if (!style.dash.IsSolid()) {
        d2dDashes.reserve(style.dash.dashes.size());
        for (float d : style.dash.dashes) {
            d2dDashes.push_back(static_cast<float>(d / width));
        }
    }
    const float d2dOffset = static_cast<float>(style.dash.offset / width);

    // StrokeStyleProperties parameter order (MSDN):
    //   startCap, endCap, dashCap, lineJoin, miterLimit, dashStyle, dashOffset
    const D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties(
        cap, cap, cap,
        ToD2DJoin(style.join),
        10.0f,
        d2dDashes.empty() ? D2D1_DASH_STYLE_SOLID : D2D1_DASH_STYLE_CUSTOM,
        d2dOffset);

    const float* dashes = d2dDashes.empty() ? nullptr : d2dDashes.data();
    const UINT32 dashCount = static_cast<UINT32>(d2dDashes.size());

    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> strokeStyle;
    const HRESULT hr = factory->CreateStrokeStyle(props, dashes, dashCount, &strokeStyle);
    if (FAILED(hr)) return nullptr;

    styles_.emplace(key_, strokeStyle);
    return strokeStyle;
}
