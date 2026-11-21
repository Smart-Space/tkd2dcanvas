// d2d_alloc_probe.cpp -- what does the Direct2D back end allocate, and when?
//
// The TinUI demo grows by ~28 MB in a single scroll cycle with the
// Direct2D back end, and the growth is in the process heap (private
// read/write pages), not in DIB sections and not in module images.
// Hiding the text items makes the growth disappear - but a scene with
// thousands of distinct glyphs does not reproduce it, so it is not the
// glyph atlas.
//
// This probe drives the Direct2D/DirectWrite APIs directly (no Tk, no
// window) and reports the private commit while it repeats one operation:
//
//   text         -- create a text layout (Latin font) and draw it
//   text-cjk     -- the same with a CJK font
//   text-latin   -- Latin text, Latin font
//   layout-only  -- create a text layout, never draw it
//   draw-only    -- draw the same text layout again and again
//   geometry-only-- create a path geometry, never draw it
//   path         -- create a path geometry and fill it
//   path-reuse   -- draw the same path geometry again and again
//   many-draws   -- several draw calls per frame
//   bitmap       -- create a bitmap and draw it
//
// Usage:  d2d_alloc_probe.exe [mode] [iterations] [draws per frame]

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <psapi.h>

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../native/render/D2DContext.h"
#include "../../native/render/D2DTargetPool.h"

using Microsoft::WRL::ComPtr;

namespace {

constexpr int kWidth = 800;
constexpr int kHeight = 600;

HDC gDc = nullptr;
HBITMAP gBitmap = nullptr;
ComPtr<IDWriteTextLayout> gLayout;
ComPtr<ID2D1PathGeometry> gGeometry;
ComPtr<ID2D1SolidColorBrush> gBrush;

long long PrivateBytes() {
    PROCESS_MEMORY_COUNTERS_EX counters;
    std::memset(&counters, 0, sizeof(counters));
    counters.cb = sizeof(counters);
    GetProcessMemoryInfo(GetCurrentProcess(),
                         reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                         sizeof(counters));
    return static_cast<long long>(counters.PrivateUsage);
}

// Committed private read/write pages (the heap of every module).
SIZE_T PrivateHeapBytes() {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T total = 0;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uintptr_t addr = 0;
    const uintptr_t limit = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
    while (addr < limit) {
        if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == 0) {
            break;
        }
        if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE &&
            (mbi.Protect & 0xFF) == PAGE_READWRITE) {
            total += mbi.RegionSize;
        }
        const uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (next <= addr) break;
        addr = next;
    }
    return total;
}

void CreateDestination() {
    BITMAPINFO info;
    std::memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = kWidth;
    info.bmiHeader.biHeight = -kHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    gDc = CreateCompatibleDC(screen);
    gBitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, nullptr, nullptr, 0);
    ReleaseDC(nullptr, screen);
    SelectObject(gDc, gBitmap);
}

ComPtr<IDWriteTextFormat> MakeFormat(IDWriteFactory* factory,
                                      const wchar_t* family, float size) {
    ComPtr<IDWriteTextFormat> format;
    if (FAILED(factory->CreateTextFormat(family, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                         size, L"zh-cn", format.GetAddressOf()))) {
        return nullptr;
    }
    return format;
}

std::wstring Widen(const char* text) {
    int n = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, &out[0], n);
    if (!out.empty()) out.resize(out.size() - 1);
    return out;
}

// A path with 40 points, built into the given geometry.
void FillGeometry(ID2D1PathGeometry* geometry) {
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(&sink))) return;
    sink->BeginFigure(D2D1::Point2F(20.0f, 20.0f), D2D1_FIGURE_BEGIN_FILLED);
    for (int k = 0; k < 40; ++k) {
        sink->AddLine(D2D1::Point2F(20.0f + k * 4.0f,
                                    20.0f + 40.0f * std::sin(k * 0.3f)));
    }
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "text";
    const int iterations = argc > 2 ? std::atoi(argv[2]) : 2000;
    const int drawsPerFrame = argc > 3 ? std::atoi(argv[3]) : 1;
    // "sw": use a software render target (no D3D device at all) instead of
    // the pool's hardware one.
    const bool software = mode.rfind("sw-", 0) == 0;
    const std::string kind = software ? mode.substr(3) : mode;

    CreateDestination();
    if (!D2DContext::EnsureInitialized()) {
        std::fprintf(stderr, "no D2D\n");
        return 1;
    }
    IDWriteFactory* write = D2DContext::WriteFactory();

    D2DTargetPool& pool = D2DTargetPool::Instance(D2DTargetKind::Hardware);
    ComPtr<ID2D1DCRenderTarget> ownTarget;
    if (software) {
        const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0f, 96.0f);
        if (FAILED(D2DContext::Factory()->CreateDCRenderTarget(&props, &ownTarget))) {
            std::fprintf(stderr, "no software render target\n");
            return 1;
        }
        const RECT rc{0, 0, kWidth, kHeight};
        ownTarget->BindDC(gDc, &rc);
    } else {
        D2DTargetPool::Guard guard(pool);
        if (!pool.EnsureTarget()) {
            std::fprintf(stderr, "no render target\n");
            return 1;
        }
        if (!pool.Bind(gDc, kWidth, kHeight, 1)) {
            std::fprintf(stderr, "bind failed\n");
            return 1;
        }
    }
    ID2D1RenderTarget* target = software ? ownTarget.Get() : pool.Target();

    ComPtr<IDWriteTextFormat> latin = MakeFormat(write, L"Segoe UI", 12.0f);
    ComPtr<IDWriteTextFormat> cjk = MakeFormat(write, L"Microsoft YaHei", 12.0f);
    target->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), &gBrush);

    const std::wstring sample = Widen(u8"内存增长测试条目 12345");
    const std::wstring latinSample = Widen("plain latin text 12345");

    const long long basePriv = PrivateBytes();
    const SIZE_T baseHeap = PrivateHeapBytes();
    std::printf("mode=%s iterations=%d draws/frame=%d  start priv=%lld KB heap=%llu KB\n",
                mode.c_str(), iterations, drawsPerFrame, basePriv / 1024,
                static_cast<unsigned long long>(baseHeap / 1024));
    std::fflush(stdout);

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    LARGE_INTEGER start;
    QueryPerformanceCounter(&start);

    for (int i = 1; i <= iterations; ++i) {
        {
            D2DTargetPool::Guard guard(pool);
            target->BeginDraw();

            for (int d = 0; d < drawsPerFrame; ++d) {
                if (kind == "text" || kind == "text-cjk" || kind == "text-latin" ||
                    kind == "layout-only" || kind == "draw-only") {
                    if (kind == "draw-only") {
                        if (!gLayout) {
                            write->CreateTextLayout(
                                sample.c_str(), static_cast<UINT32>(sample.size()),
                                latin.Get(), 1.0e6f, 1.0e4f, &gLayout);
                        }
                    } else {
                        ComPtr<IDWriteTextLayout> layout;
                        write->CreateTextLayout(
                            sample.c_str(), static_cast<UINT32>(sample.size()),
                            (kind == "text-cjk") ? cjk.Get() : latin.Get(),
                            1.0e6f, 1.0e4f, &layout);
                        if (mode != "layout-only" && layout) {
                            gLayout = layout;
                        }
                    }
                    if (mode != "layout-only" && gLayout) {
                        target->DrawTextLayout(D2D1::Point2F(20.0f, 20.0f),
                                                      gLayout.Get(), gBrush.Get(),
                                                      D2D1_DRAW_TEXT_OPTIONS_NONE);
                    }
                } else if (kind == "geometry-only" || kind == "path" ||
                           kind == "path-reuse") {
                    ComPtr<ID2D1PathGeometry> geometry;
                    if (kind == "path-reuse") {
                        if (!gGeometry) {
                            D2DContext::Factory()->CreatePathGeometry(&gGeometry);
                            FillGeometry(gGeometry.Get());
                        }
                        geometry = gGeometry;
                    } else {
                        D2DContext::Factory()->CreatePathGeometry(&geometry);
                        FillGeometry(geometry.Get());
                    }
                    if (mode != "geometry-only") {
                        target->FillGeometry(geometry.Get(), gBrush.Get());
                    }
                } else if (kind == "bitmap") {
                    ComPtr<ID2D1Bitmap> bitmap;
                    target->CreateBitmap(
                        D2D1::SizeU(256, 256), nullptr, 256 * 4,
                        D2D1::BitmapProperties(
                            D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM,
                                              D2D1_ALPHA_MODE_PREMULTIPLIED),
                            96.0f, 96.0f),
                        &bitmap);
                    if (bitmap) {
                        target->DrawBitmap(bitmap.Get(),
                                                 D2D1::RectF(0.0f, 0.0f, 256.0f, 256.0f));
                    }
                }
            }

            target->EndDraw();
        }

        if (i % 100 == 0 || i == 1) {
            const long long priv = PrivateBytes();
            const SIZE_T heap = PrivateHeapBytes();
            std::printf("iter %5d  priv=%8lld KB d=%+8lld KB   heap=%8llu KB "
                        "d=%+8lld KB\n",
                        i, priv / 1024, (priv - basePriv) / 1024,
                        static_cast<unsigned long long>(heap / 1024),
                        (static_cast<long long>(heap) - static_cast<long long>(baseHeap)) / 1024);
            std::fflush(stdout);
        }
    }
    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);
    const double ms = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 /
                      static_cast<double>(freq.QuadPart);
    std::printf("total %8.1f ms  (%6.3f ms per frame)\n", ms,
                iterations > 0 ? ms / iterations : 0.0);
    std::fflush(stdout);
    return 0;
}
