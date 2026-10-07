"""Resolve the source snapshot containing a documentation checker."""
from pathlib import Path


def source_root(script):
    root = Path(script).resolve().parents[2]
    if not (root / "CMakeLists.txt").is_file() or not (root / "src").is_dir():
        raise RuntimeError("checker is outside a complete source snapshot")
    return str(root)
