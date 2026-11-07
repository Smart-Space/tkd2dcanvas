"""Compatibility layer tests."""

import tkd2dcanvas.compat as compat
from tkd2dcanvas import Canvas as BaseCanvas


def test_compat_canvas_is_subclass():
    assert issubclass(compat.Canvas, BaseCanvas)


def test_install_is_callable():
    assert callable(compat.install)
