# Tcl package index for tkd2dcanvas.
# Installed alongside tkd2dcanvas01.dll.
package ifneeded tkd2dcanvas 0.1 \
    [list load [file join $dir tkd2dcanvas01.dll] tkd2dcanvas]
