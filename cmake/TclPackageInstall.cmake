# TclPackageInstall.cmake
#
# Helper for installing a Tcl package (DLL + pkgIndex.tcl [+ helper .tcl])
# into an install prefix. Kept as a module so multiple targets / packaging
# flows can reuse it.

# tkc_install_tcl_package(TARGET <target>
#                         NAMESPACE_DIR <dir-name>
#                         [TCL_FILES <file>...])
function(tkc_install_tcl_package)
    set(options)
    set(oneValueArgs TARGET NAMESPACE_DIR)
    set(multiValueArgs TCL_FILES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_TARGET OR NOT ARG_NAMESPACE_DIR)
        message(FATAL_ERROR "tkc_install_tcl_package: TARGET and NAMESPACE_DIR are required")
    endif()

    install(TARGETS ${ARG_TARGET}
        RUNTIME DESTINATION ${ARG_NAMESPACE_DIR}
    )
    install(FILES tcl/pkgIndex.tcl ${ARG_TCL_FILES}
        DESTINATION ${ARG_NAMESPACE_DIR}
    )
endfunction()

# tkc_write_pkg_index(<output-file> <package-name> <version> <lib-name>)
# Regenerates pkgIndex.tcl in exactly the format Tcl expects.
function(tkc_write_pkg_index output_file package_name version lib_name)
    file(WRITE "${output_file}"
"package ifneeded ${package_name} ${version} \\\n    [list load [file join \$dir ${lib_name}] ${package_name}]\n")
endfunction()
