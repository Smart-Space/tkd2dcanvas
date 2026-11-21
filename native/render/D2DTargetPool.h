// D2DTargetPool.h — the Direct2D render target of the canvas widgets,
// shared by the whole process.
//
// Every canvas widget used to own a render target of its own.  A render
// target is not cheap: it comes with a device, a command queue and a
// shader cache, and Direct2D releases those resources asynchronously, so a
// widget that recreates its target (which happens on every resize) leaves
// the memory of the old target behind for a while.
//
// The pool gives the whole process one render target.  A widget binds it to
// its own back buffer before it paints and lets go of it afterwards; the
// target itself (and the brush that belongs to it) stays alive, so the
// memory no longer grows with the number of canvas widgets and a resize no
// longer throws the device away.
//
// This is the same idea as the GDI back end, where the off-screen buffers
// of the anti-aliasing engine live in a shared pool (GdiScratchPool.h) -
// and the same idea as Tk's own engine, whose off-screen buffer is shared
// by all canvases of the thread (tk/win/tkWinAA.c).
#pragma once

#include <d2d1.h>
#include <windows.h>
#include <wrl/client.h>

#include "D2DTargetKind.h"

class D2DTargetPool {
public:
    // One pool per rasterizer: the hardware and the software target are
    // different objects, so widgets that ask for different ones can live
    // side by side.
    static D2DTargetPool& Instance(D2DTargetKind kind);

    // The render target may only be used while the guard is held: two
    // widgets must never draw into it at the same time.
    class Guard {
    public:
        explicit Guard(D2DTargetPool& pool) : pool_(&pool) {
            EnterCriticalSection(&pool_->lock_);
        }
        ~Guard() { LeaveCriticalSection(&pool_->lock_); }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

    private:
        D2DTargetPool* pool_;
    };

    // Creates the render target if it does not exist yet.
    bool EnsureTarget();

    // Binds the render target to a device context.  The target has to be
    // bound again whenever a different DC is painted into (that is what
    // ID2D1DCRenderTarget::BindDC is for).
    //
    // `owner` identifies the canvas widget that paints: as long as the same
    // widget paints into the same DC, the binding is kept and no BindDC
    // call is needed.
    bool Bind(HDC dc, int width, int height, unsigned owner);

    // Forgets the binding of the given owner: the next Bind() binds again.
    // Used when the back buffer of a widget was replaced or the widget is
    // gone - the DC belongs to the widget and must not be used any more.
    void Unbind(unsigned owner);

    // The device was lost: everything that belongs to the target is
    // dropped, the next EnsureTarget() creates a new one.
    void HandleDeviceLost();

    ID2D1RenderTarget* Target() const { return rt_.Get(); }

    // The brush the shapes are drawn with.  It belongs to the render
    // target, so it is dropped together with it.
    ID2D1SolidColorBrush* Brush();

    // Generation of the render target.  It changes whenever the target is
    // recreated, so that the per-widget caches can drop what they hold.
    unsigned Generation() const { return generation_; }

    // Number of render targets the process has created (for the tests).
    static long long TargetsCreated();

private:
    explicit D2DTargetPool(D2DTargetKind kind);
    ~D2DTargetPool();

    D2DTargetPool(const D2DTargetPool&) = delete;
    D2DTargetPool& operator=(const D2DTargetPool&) = delete;

    D2DTargetKind kind_;
    CRITICAL_SECTION lock_;
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> rt_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    HDC boundDc_ = nullptr;
    int boundWidth_ = 0;
    int boundHeight_ = 0;
    unsigned boundOwner_ = 0;
    unsigned generation_ = 0;
};
