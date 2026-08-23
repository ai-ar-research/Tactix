#!/usr/bin/env python3
"""Guard the Tier-D debug reservation and independent binary pin."""

import importlib.util
from pathlib import Path
import stat
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "tactix_tierd", ROOT / "tools/bench/tactix_tierd.py")
TIERD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TIERD)

assert TIERD.slurm_array_spec(1166, 89) == "0-1165%89"
assert TIERD.slurm_array_spec(1166, None) == "0-1165"

with tempfile.TemporaryDirectory(prefix="tactix-tierd-pin-") as tmp:
    base = Path(tmp)
    source = base / "build/tactix"
    target = base / "campaign/pins/tactix"
    source.parent.mkdir()
    source.write_bytes(b"first release binary")
    source.chmod(source.stat().st_mode | stat.S_IXUSR)
    TIERD.copy_frozen_binary(source, target)

    assert source.stat().st_ino != target.stat().st_ino
    assert target.stat().st_mode & 0o222 == 0
    assert target.read_bytes() == b"first release binary"
    source.write_bytes(b"later debug rebuild")
    assert target.read_bytes() == b"first release binary"

print("Tier-D campaign pinning: PASS")
