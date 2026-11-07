cmake_minimum_required(VERSION 3.26)
project(tkd2dcanvas LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT WIN32)
    message(WARNING "tkd2dcanvas currently targets Windows/Direct2D; building on this platform is unsupported")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/utf-8)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(TKG_BUILD_TESTS "Build native unit tests" ON)
option(TKG_SYNC_PYTHON_PACKAGE "Copy the built DLL/pkgIndex into python/tkd2dcanvas/tcl after build" ON)
option(TKG_BUILD_BENCH "Also build the Direct2D micro-benchmark (tkg_bench)" OFF)
option(TKG_CORE_ONLY "Skip Tcl/Tk-dependent targets (no SDK required)" OFF)

set(TKG_CORE_SOURCES
    native/core/CanvasModel.cpp
    native/core/CanvasItem.cpp
    native/core/ItemFactory.cpp
    native/core/TagsIndex.cpp
    native/core/CanvasView.cpp
    native/core/Geometry.cpp
    native/core/HitTest.cpp
    native/core/TkAreaOverlap.cpp
    native/core/OptionValue.cpp
    native/core/Color.cpp
    native/core/DashPattern.cpp
    native/core/SmoothCurve.cpp
    native/core/Transform2D.cpp
)
set(TKG_RENDER_SOURCES
    native/render/D2DRenderer.cpp
    native/render/D2DContext.cpp
    native/render/PenCache.cpp
    native/render/BrushCache.cpp
    native/render/FontCache.cpp
    native/render/DashConverter.cpp
)

# --- Micro-benchmark (independent of the Tcl/Tk SDK) --------------------------
if(TKG_BUILD_BENCH AND WIN32)
    add_executable(tkg_bench native/bench/BenchRender.cpp ${TKG_CORE_SOURCES} ${TKG_RENDER_SOURCES})
    target_include_directories(tkg_bench PRIVATE native/include native/render)
    target_compile_definitions(tkg_bench PRIVATE UNICODE _UNICODE NOMINMAX)
    target_link_libraries(tkg_bench PRIVATE d2d1 dwrite user32 gdi32)
endif()

if(TKG_CORE_ONLY)
    message(STATUS "TKG_CORE_ONLY=ON: skipping Tcl/Tk-dependent targets (bench + tests only)")
else()
    # Finds Tcl/Tk in third_party/tcltk-sdk first, then TCL_ROOT/TK_ROOT.
    find_package(TclTkDev REQUIRED)

add_library(tkd2dcanvas SHARED
    native/core/CanvasModel.cpp
    native/core/CanvasItem.cpp
    native/core/ItemFactory.cpp
    native/core/TagsIndex.cpp
    native/core/CanvasView.cpp
    native/core/Geometry.cpp
    native/core/HitTest.cpp
    native/core/TkAreaOverlap.cpp
    native/core/OptionValue.cpp
    native/core/Color.cpp
    native/core/DashPattern.cpp
    native/core/SmoothCurve.cpp
    native/core/Transform2D.cpp

    native/items/RectItem.cpp
    native/items/OvalItem.cpp
    native/items/LineItem.cpp
    native/items/PolygonItem.cpp
    native/items/ArcItem.cpp
    native/items/TextItem.cpp
    native/items/ImageItem.cpp
    native/items/BitmapItem.cpp
    native/items/WindowItem.cpp
    native/items/ItemRegistrations.cpp

    native/render/D2DRenderer.cpp
    native/render/D2DContext.cpp
    native/render/PenCache.cpp
    native/render/BrushCache.cpp
    native/render/FontCache.cpp
    native/render/DashConverter.cpp

    native/platform_win/Win32Surface.cpp
    native/platform_win/Win32ChildWindow.cpp
    native/platform_win/Win32Dpi.cpp
    native/platform_win/Win32Input.cpp
    native/platform_win/Win32DoubleBuffer.cpp

    native/tk_glue/Tkd2dcanvasInit.cpp
    native/tk_glue/CanvasCmd.cpp
    native/tk_glue/CanvasWidget.cpp
    native/tk_glue/CanvasOptions.cpp
    native/tk_glue/CanvasEventBridge.cpp
    native/tk_glue/CanvasScrollBridge.cpp
    native/tk_glue/CanvasItemCommand.cpp
    native/tk_glue/CanvasItemOptions.cpp
    native/tk_glue/CanvasTagCommand.cpp
    native/tk_glue/CanvasBindCommand.cpp
    native/tk_glue/CanvasTkColor.cpp
    native/tk_glue/CanvasTkFont.cpp
    native/tk_glue/CanvasTkImage.cpp
    native/tk_glue/CanvasTkWindowItem.cpp
    native/tk_glue/WindowHook.cpp
)

target_include_directories(tkd2dcanvas PRIVATE
    native/include
    native/render
    native/platform_win
    native/tk_glue
    native/items
    ${TCL_INCLUDE_PATH}
    ${TK_INCLUDE_PATH}
)

target_compile_definitions(tkd2dcanvas PRIVATE
    USE_TCL_STUBS
    USE_TK_STUBS
    UNICODE
    _UNICODE
    NOMINMAX
)

target_link_libraries(tkd2dcanvas PRIVATE
    ${TCL_STUB_LIBRARY}
    ${TK_STUB_LIBRARY}
    d2d1
    dwrite
    user32
    gdi32
    comctl32
)

# Tcl package naming convention: <pkg><major><minor>.dll, e.g. tkd2dcanvas01.dll
set_target_properties(tkd2dcanvas PROPERTIES
    PREFIX ""
    OUTPUT_NAME "tkd2dcanvas01"
    SUFFIX ".dll"
)

install(TARGETS tkd2dcanvas
    RUNTIME DESTINATION tkd2dcanvas
)

install(FILES
    tcl/pkgIndex.tcl
    tcl/tkd2dcanvas.tcl
    DESTINATION tkd2dcanvas
)

if(TKG_SYNC_PYTHON_PACKAGE)
    add_custom_command(TARGET tkd2dcanvas POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:tkd2dcanvas>
            "${CMAKE_CURRENT_SOURCE_DIR}/python/tkd2dcanvas/tcl/tkd2dcanvas/tkd2dcanvas01.dll"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/tcl/pkgIndex.tcl"
            "${CMAKE_CURRENT_SOURCE_DIR}/python/tkd2dcanvas/tcl/tkd2dcanvas/pkgIndex.tcl"
        COMMENT "Syncing Tcl package into python/tkd2dcanvas/tcl/tkd2dcanvas"
    )
endif()
endif()  # TKG_CORE_ONLY

if(TKG_BUILD_TESTS AND WIN32)
    enable_testing()
    foreach(test_name IN ITEMS test_canvas_model test_tags test_geometry test_hit_test test_bbox test_smooth_curve test_hit_search)
        add_executable(${test_name} tests/cpp/${test_name}.cpp ${TKG_CORE_SOURCES})
        target_include_directories(${test_name} PRIVATE native/include)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()

    # The item classes are needed to test the hit test of the concrete items.
    set(TKG_ITEM_SOURCES
        native/items/RectItem.cpp
        native/items/OvalItem.cpp
        native/items/LineItem.cpp
        native/items/PolygonItem.cpp
        native/items/ArcItem.cpp
        native/items/ItemRegistrations.cpp
    )
    add_executable(test_item_hit tests/cpp/test_item_hit.cpp ${TKG_CORE_SOURCES} ${TKG_ITEM_SOURCES})
    target_include_directories(test_item_hit PRIVATE native/include native/items)
    add_test(NAME test_item_hit COMMAND test_item_hit)
endif()
