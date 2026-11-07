# WindowsToolchain.cmake
#
# Optional toolchain file for explicit Windows MSVC builds.
# Usage: cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/WindowsToolchain.cmake
#
# Not required for normal usage (CMake auto-detects Visual Studio).

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# Prefer x64 and modern Windows APIs.
set(CMAKE_GENERATOR_PLATFORM x64 CACHE STRING "" FORCE)

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

# Consistent UTF-8 source handling for mixed Chinese/English sources.
if(MSVC)
    add_compile_options(/utf-8 /permissive-)
endif()
