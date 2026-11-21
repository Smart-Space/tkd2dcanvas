// D2DTargetKind.h — which Direct2D rasterizer a canvas uses.
//
// Hardware: the shapes are rasterized by the GPU.  The canvas draws into a
// memory DC (the double buffer) and blits that to the window, so every
// frame has to be copied back from the GPU - and the D3D device keeps the
// memory of those copies in its own cache, which grows in big steps (see
// docs/gdi_antialias.md 8.14).
//
// Software: the shapes are rasterized by the CPU straight into the memory
// DC.  No copy, no device cache - and for a canvas that is double buffered
// in a DC that is the faster way round.
#pragma once

enum class D2DTargetKind {
    Hardware,  // D2D1_RENDER_TARGET_TYPE_DEFAULT (GPU, WARP fallback)
    Software,  // D2D1_RENDER_TARGET_TYPE_SOFTWARE (CPU)
};
