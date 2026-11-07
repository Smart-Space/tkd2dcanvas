# FindTclTkDev.cmake
#
# Locates Tcl/Tk development files (headers + stub libraries):
#   TCL_INCLUDE_PATH, TK_INCLUDE_PATH, TCL_STUB_LIBRARY, TK_STUB_LIBRARY
#
# Search order (highest priority first):
#   1. Cache variables: -D TCL_ROOT=... -D TK_ROOT=... (or a single -D TCLTK_ROOT=...)
#   2. Environment variables: TCL_ROOT / TK_ROOT / TCLTK_ROOT
#   3. The in-repo SDK at <source>/third_party/tcltk-sdk
#   4. Well-known Windows locations
#
# If nothing is found, a helpful error is raised pointing at
# third_party/tcltk-sdk/README.md.

function(_tkc_search roots out_inc out_lib name_stubs)
    set(inc "")
    set(lib "")

    foreach(root IN LISTS roots)
        if(NOT root OR NOT EXISTS "${root}")
            continue()
        endif()

        if(EXISTS "${root}/tcl.h" AND EXISTS "${root}/tk.h")
            set(inc "${root}")
        elseif(EXISTS "${root}/include/tcl.h" AND EXISTS "${root}/include/tk.h")
            set(inc "${root}/include")
        endif()

        foreach(stub IN LISTS name_stubs)
            if(EXISTS "${root}/${stub}")
                set(lib "${root}/${stub}")
                break()
            elseif(EXISTS "${root}/lib/${stub}")
                set(lib "${root}/lib/${stub}")
                break()
            endif()
        endforeach()

        if(inc AND lib)
            set(${out_inc} "${inc}" PARENT_SCOPE)
            set(${out_lib} "${lib}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

get_filename_component(_tkc_repo_sdk "${CMAKE_CURRENT_LIST_DIR}/../third_party/tcltk-sdk" ABSOLUTE)
file(REAL_PATH "${_tkc_repo_sdk}" _tkc_repo_sdk)

set(TCLTK_CANDIDATES
    "${TCLTK_ROOT}"
    "${TCL_ROOT}"
    "${TK_ROOT}"
    "$ENV{TCLTK_ROOT}"
    "$ENV{TCL_ROOT}"
    "$ENV{TK_ROOT}"
    "${_tkc_repo_sdk}"
    "C:/Tcl"
    "C:/tcl"
    "$ENV{ProgramFiles}/Tcl"
)
if(DEFINED ENV{ProgramFiles\(x86\)})
    list(APPEND TCLTK_CANDIDATES "$ENV{ProgramFiles\(x86\)}/Tcl")
endif()

set(TCL_STUB_NAMES "tclstub90.lib;tclstub87.lib;tclstub86.lib;tcl86.lib")
set(TK_STUB_NAMES  "tkstub90.lib;tkstub87.lib;tkstub86.lib;tk86.lib")

# Tcl and Tk usually live in the same prefix; also allow separate ones.
_tkc_search("${TCLTK_CANDIDATES}" TCL_INCLUDE_PATH TCL_STUB_LIBRARY "${TCL_STUB_NAMES}")
if(NOT TCL_INCLUDE_PATH OR NOT TCL_STUB_LIBRARY)
    set(TCLTK_TCL_CANDIDATES "${TCL_ROOT}" "$ENV{TCL_ROOT}" "$ENV{TCLTK_ROOT}" "${_tkc_repo_sdk}")
    _tkc_search("${TCLTK_TCL_CANDIDATES}" TCL_INCLUDE_PATH TCL_STUB_LIBRARY "${TCL_STUB_NAMES}")
endif()

_tkc_search("${TCLTK_CANDIDATES}" TK_INCLUDE_PATH TK_STUB_LIBRARY "${TK_STUB_NAMES}")
if(NOT TK_INCLUDE_PATH OR NOT TK_STUB_LIBRARY)
    set(TCLTK_TK_CANDIDATES "${TK_ROOT}" "$ENV{TK_ROOT}" "$ENV{TCLTK_ROOT}" "${_tkc_repo_sdk}")
    _tkc_search("${TCLTK_TK_CANDIDATES}" TK_INCLUDE_PATH TK_STUB_LIBRARY "${TK_STUB_NAMES}")
endif()

set(TCL_FOUND ${TCL_INCLUDE_PATH} AND ${TCL_STUB_LIBRARY})
set(TK_FOUND ${TK_INCLUDE_PATH} AND ${TK_STUB_LIBRARY})

if(TCL_INCLUDE_PATH AND TCL_STUB_LIBRARY AND TK_INCLUDE_PATH AND TK_STUB_LIBRARY)
    set(TclTkDev_FOUND TRUE)
    message(STATUS "Tcl include: ${TCL_INCLUDE_PATH}")
    message(STATUS "Tk  include: ${TK_INCLUDE_PATH}")
    message(STATUS "Tcl stub lib: ${TCL_STUB_LIBRARY}")
    message(STATUS "Tk  stub lib: ${TK_STUB_LIBRARY}")
else()
    set(TclTkDev_FOUND FALSE)
    set(_tkc_fail "Tcl/Tk development files not found.\n")
    string(APPEND _tkc_fail
        "Place headers and stub libraries in third_party/tcltk-sdk (see third_party/tcltk-sdk/README.md),\n")
    string(APPEND _tkc_fail
        "or pass -DTCL_ROOT=<prefix> -DTK_ROOT=<prefix> (prefix containing include/ and lib/, or tcl.h/tk.h directly).\n")
    string(APPEND _tkc_fail
        "You can also run tools/find_tcltk.py and tools/check_tcltk_dev.py to locate/verify an SDK.\n")
    message(FATAL_ERROR "${_tkc_fail}")
endif()

mark_as_advanced(TCL_INCLUDE_PATH TK_INCLUDE_PATH TCL_STUB_LIBRARY TK_STUB_LIBRARY)
