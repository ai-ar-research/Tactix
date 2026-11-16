"""Resolve the source snapshot containing a documentation checker."""
from pathlib import Path
import re


def source_root(script):
    root = Path(script).resolve().parents[2]
    if not (root / "CMakeLists.txt").is_file() or not (root / "src").is_dir():
        raise RuntimeError("checker is outside a complete source snapshot")
    return str(root)


def default_config_source(root):
    """Expand the scalar field lists for source-conformance checks."""
    directory = Path(root) / "src/policies"
    schema = (directory / "default_setting_fields.hpp").read_text()
    groups = {}
    for match in re.finditer(r"^#define TACTIX_DEFAULT_(\w+)\(FIELD, Context\)(.*?)(?=\n\n|\Z)",
                             schema, re.M | re.S):
        body = match[2].replace("\\\n", "\n")
        body = re.sub(r"/\*([^\n]*?)\*/", r"//\1", body)
        body = re.sub(r"FIELD\(Context,\s*(\w+),\s*(\w+),\s*([^,\n]+),\s*(?:IDENTITY|UNSIGNED)\)",
                      lambda row: f"{row[1]} {row[2]} = {row[3]};", body)
        if "FIELD(" in body:
            raise ValueError(f"unrecognized default-setting row in {match[1]}")
        groups[match[1]] = body
    return re.sub(r"TACTIX_DECLARE_DEFAULT_SETTINGS\((\w+)\)",
                  lambda match: groups[match[1]],
                  (directory / "default_strategy_config.hpp").read_text())
