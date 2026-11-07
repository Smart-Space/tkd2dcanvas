#include "tkd2dcanvas/DashPattern.h"

// Dash pattern utilities are header-only for now. Tk -dash string parsing
// lives in native/render/DashConverter.cpp by design (render layer owns
// Tk-media conversion); this translation unit exists so the core library
// builds a consistent object set.
