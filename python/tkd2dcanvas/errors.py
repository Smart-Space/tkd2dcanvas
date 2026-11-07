"""Exceptions raised by tkd2dcanvas."""


class Tkd2dcanvasError(Exception):
    """Base class for all tkd2dcanvas errors."""


class LoadError(Tkd2dcanvasError):
    """The bundled Tcl package could not be loaded into the interpreter."""


class CommandError(Tkd2dcanvasError):
    """A Tcl command issued by the wrapper failed."""
