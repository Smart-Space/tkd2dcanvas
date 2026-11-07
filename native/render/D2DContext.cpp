#include "D2DContext.h"

namespace {
ID2D1Factory* g_factory = nullptr;
IDWriteFactory* g_writeFactory = nullptr;
bool g_initialized = false;
}

bool D2DContext::EnsureInitialized() {
    if (g_initialized) return true;

    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory),
        nullptr,
        reinterpret_cast<void**>(&g_factory));
    if (FAILED(hr) || !g_factory) {
        g_factory = nullptr;
        return false;
    }

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&g_writeFactory));
    if (FAILED(hr) || !g_writeFactory) {
        g_writeFactory = nullptr;
        g_factory->Release();
        g_factory = nullptr;
        return false;
    }

    g_initialized = true;
    return true;
}

ID2D1Factory* D2DContext::Factory() { return g_factory; }
IDWriteFactory* D2DContext::WriteFactory() { return g_writeFactory; }
bool D2DContext::Ok() { return g_initialized; }

void D2DContext::Shutdown() {
    if (g_writeFactory) {
        g_writeFactory->Release();
        g_writeFactory = nullptr;
    }
    if (g_factory) {
        g_factory->Release();
        g_factory = nullptr;
    }
    g_initialized = false;
}
