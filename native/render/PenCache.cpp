#include "PenCache.h"

#include <sstream>
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
    return BuildKey(style);
}

std::string PenCache::BuildKey(const PenStyle& style) {
    std::ostringstream oss;
    oss << style.width << '@';
    for (float d : style.dash.dashes) oss << d << ',';
    oss << style.dash.offset << '|' << static_cast<int>(style.cap)
        << static_cast<int>(style.join) << '|' << (style.antialias ? 'A' : 'L');
    return oss.str();
}

Microsoft::WRL::ComPtr<ID2D1StrokeStyle> PenCache::GetStrokeStyle(ID2D1Factory* factory,
                                                                  const PenStyle& style) {
    if (!factory) return nullptr;

    const std::string key = BuildKey(style);
    auto it = styles_.find(key);
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

    styles_.emplace(key, strokeStyle);
    return strokeStyle;
}
