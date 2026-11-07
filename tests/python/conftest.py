"""Make the in-tree Python package importable during tests.

The package lives in ``python/tkd2dcanvas`` (not installed), so tests add
that directory to ``sys.path``. A real installation (wheel) makes this a
no-op.
"""

import sys
from pathlib import Path

_PKG_ROOT = Path(__file__).resolve().parent.parent.parent / "python"

if str(_PKG_ROOT) not in sys.path:
    sys.path.insert(0, str(_PKG_ROOT))
