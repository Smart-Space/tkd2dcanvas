// TkBuiltinBitmaps.h — the predefined XBM bitmaps Tk registers by name
// (tkBitmap.c: "error", "gray12", "gray25", "gray50", "gray75", "hourglass",
// "info", "questhead", "question", "warning").
//
// The arrays are copied verbatim from the Tk 8.6 source tree
// (third_party/tcltk-src/tk/bitmaps/*.xbm) so `create_bitmap -bitmap info`
// works without depending on the toolkit's runtime internals. Licensed
// under Tk's license (see COPYING).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace tkd2dcanvas {

struct BuiltinBitmap {
    const char* name;
    int width;
    int height;
    const std::uint8_t* bits;
};

// Returns nullptr when `name` (already normalized to lowercase) is not a
// bitmap Tk predefines.
const BuiltinBitmap* FindBuiltinBitmap(const std::string& name);

}  // namespace tkd2dcanvas
