"""Exceptions raised by tkd2dcanvas."""


class Tkd2dcanvasError(Exception):
    """Base class for all tkd2dcanvas errors."""


class CommandError(Tkd2dcanvasError):
    """A Tcl command issued by the wrapper failed."""


class LoadError(CommandError):
    """The bundled Tcl package could not be loaded into the interpreter.

    It derives from :class:`CommandError` so that code which only cares
    about "the widget could not be used" can catch a single exception type.
    """
