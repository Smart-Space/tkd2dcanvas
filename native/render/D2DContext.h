// D2DContext.h — process-lifetime Direct2D / DirectWrite factories.
// Unlike GDI+, D2D needs no explicit startup token: creating the factory
// initializes what is needed. Shutdown() simply releases the factories.
#pragma once

#include <d2d1.h>
#include <dwrite.h>

class D2DContext {
public:
    // Creates the shared factories (thread-safe enough for Tk's UI thread).
    // Falls back to software factory on machines without D2D support.
    static bool EnsureInitialized();
    static void Shutdown();

    static ID2D1Factory* Factory();
    static IDWriteFactory* WriteFactory();
    static bool Ok();

private:
    D2DContext() = delete;
};
