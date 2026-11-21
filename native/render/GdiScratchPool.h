// GdiScratchPool.h — the off-screen buffers of the anti-aliasing engine,
// shared by every canvas widget of the process.
//
// Tk does the same on its side: tk/win/tkWinAA.c keeps the scratch buffer
// of the anti-aliasing engine in a state that all canvases of the thread
// share, not in the canvas itself.  Here the buffers live in a pool that
// the whole process shares, so the memory they need does not grow with the
// number of canvas widgets: it is one set of buffers for the application
// instead of one set per widget (which was "number of widgets x 1 MB").
//
// Drawing always happens inside one thread (Tk's UI thread), but the pool
// is guarded by a critical section, so the sharing stays safe when several
// threads paint.
#pragma once

#include <windows.h>

#include <vector>

namespace tkd2dcanvas {

// The color the shape is drawn with inside the off-screen buffer.  The
// engine compares the samples against it to find out which ones are
// covered, so it only has to differ from the color the buffer is cleared
// with.
inline constexpr COLORREF kAaMaskColor = RGB(1, 2, 3);

class GdiScratchPool {
public:
    static GdiScratchPool& Instance();

    // The buffers may only be used while the guard is held: two shapes
    // must never be rendered into the same buffer at the same time.
    class Guard {
    public:
        Guard() : pool_(&Instance()) { EnterCriticalSection(&pool_->lock_); }
        ~Guard() { LeaveCriticalSection(&pool_->lock_); }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

    private:
        GdiScratchPool* pool_;
    };

    // How much the pool may keep between two shapes.  A single big shape
    // needs a big buffer, but there is no reason to hold on to it: the
    // buffers would otherwise grow to the biggest shape that was ever
    // drawn and stay that big for the whole life of the process.
    static constexpr long long kRetainSamples = 4LL * 1024 * 1024;
    static constexpr long long kRetainPixels = 1024LL * 1024;

    // Makes sure the scratch buffer can hold width x height samples.
    bool EnsureScratch(int width, int height);

    // Makes sure the read-back buffer can hold width x height pixels.
    bool EnsureReadBack(int width, int height);

    // Gives back the buffers that grew beyond the retention limits.
    void Trim();

    HDC ScratchDC() const { return scratchDC_; }
    void* ScratchBits() const { return scratchBits_; }
    int ScratchWidth() const { return scratchWidth_; }
    int ScratchHeight() const { return scratchHeight_; }

    HDC ReadBackDC() const { return readBackDC_; }
    void* ReadBackBits() const { return readBackBits_; }
    int ReadBackWidth() const { return readBackWidth_; }
    int ReadBackHeight() const { return readBackHeight_; }

    // The pen used to stroke a shape in the scratch buffer.  Pens are
    // cached: creating one per shape would churn GDI objects, and GDI's
    // handle table grows with the churn.
    HPEN MaskPen(int widthHi, DWORD capStyle, DWORD joinStyle);

    // The brush used to fill a shape in the scratch buffer.  It always has
    // the mask color, so a single brush serves every shape.
    HBRUSH MaskBrush();

    // Size of the off-screen buffers the pool holds (bytes).
    long long Bytes() const;

    // Number of off-screen buffers the pool holds.
    int BufferCount() const;

private:
    GdiScratchPool();
    ~GdiScratchPool();

    GdiScratchPool(const GdiScratchPool&) = delete;
    GdiScratchPool& operator=(const GdiScratchPool&) = delete;

    void ReleaseBuffers();

    CRITICAL_SECTION lock_;

    // Off-screen buffer the shape is drawn into (scale times the device
    // resolution), 32 bit, top-down.
    HDC scratchDC_ = nullptr;
    HBITMAP scratchBitmap_ = nullptr;
    void* scratchBits_ = nullptr;
    int scratchWidth_ = 0;   // in samples
    int scratchHeight_ = 0;

    // Copy of the destination region that is modified (device resolution).
    HDC readBackDC_ = nullptr;
    HBITMAP readBackBitmap_ = nullptr;
    void* readBackBits_ = nullptr;
    int readBackWidth_ = 0;
    int readBackHeight_ = 0;

    // The brush used to fill a shape in the scratch buffer.
    HBRUSH maskBrush_ = nullptr;

    // Cached pens for the scratch buffer.
    struct CachedPen {
        int widthHi;
        DWORD capStyle;
        DWORD joinStyle;
        HPEN pen;
    };
    std::vector<CachedPen> penCache_;
};

}  // namespace tkd2dcanvas
