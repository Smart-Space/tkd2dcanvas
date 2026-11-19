// SurfaceRenderer.h — a Renderer that paints into a device context.
//
// The canvas widget paints into a memory DC (its double buffered back
// buffer).  The renderer has to be bound to that DC for the duration of a
// frame, which is what this interface adds to the plain Renderer.
#pragma once

#include "tkd2dcanvas/Renderer.h"

class SurfaceRenderer : public Renderer {
public:
    // Binds the renderer to a device context.  `width`/`height` are the
    // pixel size of the surface.  Returns false when the renderer cannot
    // paint into the given context.
    virtual bool AttachToHdc(void* hdc, int width, int height) = 0;

    // Ends the current frame.
    virtual void Detach() = 0;

    // Drops the binding (e.g. after the surface was resized).
    virtual void ResetBinding() = 0;
};
