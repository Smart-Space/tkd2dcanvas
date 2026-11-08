#include "CanvasTkImage.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <utility>

#include "TkBuiltinBitmaps.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <tcl.h>
#include <tk.h>

namespace tkd2dcanvas {

bool TkPhotoToRgba(Tcl_Interp* interp, Tk_Window /*tkwin*/,
                   const std::string& imageName, RgbaImage& out) {
    const char* name = imageName.c_str();
    Tk_PhotoHandle handle = Tk_FindPhoto(interp, name);
    if (handle == nullptr) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf(
            "image \"%s\" doesn't exist or is not a photo image", name));
        return false;
    }

    Tk_PhotoImageBlock block{};
    if (!Tk_PhotoGetImage(handle, &block)) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf(
            "couldn't get the data of photo image \"%s\"", name));
        return false;
    }

    if (block.width <= 0 || block.height <= 0) {
        out.width = 0;
        out.height = 0;
        out.pixels.clear();
        return true;  // empty photo: legal, draws nothing
    }
    if (block.pixelPtr == nullptr || block.pitch <= 0 || block.pixelSize <= 0) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf(
            "photo image \"%s\" reports an unusable in-memory layout", imageName.c_str()));
        return false;
    }

    out.width = block.width;
    out.height = block.height;
    out.pixels.assign(static_cast<std::size_t>(block.width) * static_cast<std::size_t>(block.height) * 4, 0);

    const int redOff = block.offset[0];
    const int greenOff = block.offset[1];
    const int blueOff = block.offset[2];
    const int alphaOff = block.offset[3];
    for (int y = 0; y < block.height; ++y) {
        const unsigned char* row = block.pixelPtr + static_cast<std::size_t>(y) * static_cast<std::size_t>(block.pitch);
        for (int x = 0; x < block.width; ++x) {
            const unsigned char* px = row + static_cast<std::size_t>(x) * static_cast<std::size_t>(block.pixelSize);
            unsigned char* dst = out.pixels.data() + (static_cast<std::size_t>(y) * block.width + x) * 4;
            dst[0] = px[redOff];
            dst[1] = px[greenOff];
            dst[2] = px[blueOff];
            dst[3] = px[alphaOff];
        }
    }
    return true;
}

namespace {

// Parses the body of an XBM `static unsigned char xxx_bits[] = { ... };`
// initializer: comma-separated integer literals (hex, octal, decimal).
bool ParseXbmBits(const std::string& text, std::vector<std::uint8_t>& out, std::string& error) {
    const std::size_t brace = text.find('{');
    const std::size_t end = text.find('}', brace);
    if (brace == std::string::npos || end == std::string::npos || end < brace) {
        error = "malformed bitmap file: no bits initializer found";
        return false;
    }
    out.clear();
    const char* cursor = text.c_str() + brace + 1;
    const char* limit = text.c_str() + end;
    while (cursor < limit) {
        while (cursor < limit && (std::isspace(static_cast<unsigned char>(*cursor)) || *cursor == ',')) ++cursor;
        if (cursor >= limit) break;
        char* parseEnd = nullptr;
        const unsigned long value = std::strtoul(cursor, &parseEnd, 0);
        if (parseEnd == cursor) {
            error = std::string("malformed bitmap file: bad initializer near '") + cursor + "'";
            return false;
        }
        out.push_back(static_cast<std::uint8_t>(value & 0xff));
        cursor = parseEnd;
    }
    return true;
}

// Finds `#define <prefix>_width N` / `#define <prefix>_height N` in an XBM file.
bool ParseXbmDimension(const std::string& text, const char* suffix, int& out) {
    const std::string marker = std::string("_") + suffix;
    std::size_t pos = 0;
    while ((pos = text.find(marker, pos)) != std::string::npos) {
        // The define line looks like "#define info_width 8".
        std::size_t lineStart = text.rfind('\n', pos);
        lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
        const std::string line = text.substr(lineStart, text.find('\n', pos) - lineStart);
        if (line.find("#define") != std::string::npos) {
            const char* cursor = line.c_str() + line.find(marker) + marker.size();
            while (*cursor && std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
            char* parseEnd = nullptr;
            const long value = std::strtol(cursor, &parseEnd, 10);
            if (parseEnd != cursor && value > 0) {
                out = static_cast<int>(value);
                return true;
            }
            return false;
        }
        pos += marker.size();
    }
    return false;
}

bool ParseXbmText(const std::string& text, XbmBitmap& out, std::string& error) {
    if (!ParseXbmDimension(text, "width", out.width) || !ParseXbmDimension(text, "height", out.height)) {
        error = "malformed bitmap file: could not find _width/_height defines";
        return false;
    }
    std::vector<std::uint8_t> bits;
    if (!ParseXbmBits(text, bits, error)) return false;
    const std::size_t expected =
        static_cast<std::size_t>(out.height) * static_cast<std::size_t>((out.width + 7) / 8);
    if (bits.size() != expected) {
        error = "malformed bitmap file: expected " + std::to_string(expected) +
                " data bytes, got " + std::to_string(bits.size());
        return false;
    }
    out.bits = std::move(bits);
    return true;
}

bool ReadFileUtf8(const std::string& path, std::string& out) {
#ifdef _WIN32
    const int wideLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wideLen <= 0) return false;
    std::wstring wide(static_cast<std::size_t>(wideLen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide.data(), wideLen);
    std::ifstream stream(wide, std::ios::binary);
#else
    std::ifstream stream(path, std::ios::binary);
#endif
    if (!stream) return false;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    out = buffer.str();
    return true;
}

}  // namespace

bool TkBitmapToBits(Tcl_Interp* interp, const std::string& bitmapName, XbmBitmap& out) {
    if (bitmapName.empty()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("expected a non-empty bitmap name", -1));
        return false;
    }

    if (bitmapName[0] == '@') {
        const std::string path = bitmapName.substr(1);
        std::string text;
        if (!ReadFileUtf8(path, text)) {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("couldn't read bitmap file \"%s\"", path.c_str()));
            return false;
        }
        std::string error;
        if (!ParseXbmText(text, out, error)) {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s", error.c_str()));
            return false;
        }
        return true;
    }

    // Options are matched case-insensitively in Tk; normalize like the rest
    // of the option parser (lowercase, prefix match happens in Tk for
    // builtins via exact hash lookup, so require the exact name lowercase).
    std::string lower = bitmapName;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (const BuiltinBitmap* builtin = FindBuiltinBitmap(lower)) {
        out.width = builtin->width;
        out.height = builtin->height;
        out.bits.assign(builtin->bits, builtin->bits +
            static_cast<std::size_t>(builtin->height) * static_cast<std::size_t>((builtin->width + 7) / 8));
        return true;
    }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
        "bitmap \"%s\" not defined (supported: @file.xbm or one of Tk's predefined names)",
        bitmapName.c_str()));
    return false;
}

RgbaImage XbmToRgba(const XbmBitmap& bitmap,
                    const std::optional<Color>& foreground,
                    const std::optional<Color>& background) {
    RgbaImage image;
    image.width = bitmap.width;
    image.height = bitmap.height;
    if (bitmap.width <= 0 || bitmap.height <= 0 || bitmap.bits.empty()) return image;

    const std::size_t rowBytes = static_cast<std::size_t>((bitmap.width + 7) / 8);
    image.pixels.assign(static_cast<std::size_t>(bitmap.width) * static_cast<std::size_t>(bitmap.height) * 4, 0);
    for (int y = 0; y < bitmap.height; ++y) {
        for (int x = 0; x < bitmap.width; ++x) {
            const std::uint8_t byte = bitmap.bits[static_cast<std::size_t>(y) * rowBytes +
                                                  static_cast<std::size_t>(x / 8)];
            const bool set = ((byte >> (x % 8)) & 0x01) != 0;
            std::uint8_t* dst = image.pixels.data() +
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(bitmap.width) +
                 static_cast<std::size_t>(x)) * 4;
            if (set) {
                if (foreground) {
                    dst[0] = foreground->r;
                    dst[1] = foreground->g;
                    dst[2] = foreground->b;
                    dst[3] = foreground->a;
                }
            } else if (background) {
                dst[0] = background->r;
                dst[1] = background->g;
                dst[2] = background->b;
                dst[3] = background->a;
            }
        }
    }
    return image;
}

}  // namespace tkd2dcanvas
