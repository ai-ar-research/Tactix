#!/usr/bin/env python3
"""The fuzz batch must execute an immutable harness snapshot, never live code."""

import hashlib
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SLURM = ROOT / "tools" / "fuzz" / "slurm.sh"


def make_writable(path: Path) -> None:
    for entry in path.rglob("*"):
        entry.chmod(entry.stat().st_mode | stat.S_IWUSR)
    path.chmod(path.stat().st_mode | stat.S_IWUSR)


with tempfile.TemporaryDirectory(prefix="tactix-fuzz-sbatch-") as tmp:
    temp = Path(tmp)
    batch_file = temp / "batch.sh"
    args_file = temp / "args.txt"
    mock = temp / "sbatch"
    mock.write_text(
        "#!/usr/bin/env python3\n"
        "import os, pathlib, sys\n"
        "pathlib.Path(os.environ['MOCK_BATCH']).write_text(sys.stdin.read())\n"
        "pathlib.Path(os.environ['MOCK_ARGS']).write_text('\\n'.join(sys.argv[1:]))\n"
        "print('424242')\n",
        encoding="utf-8",
    )
    mock.chmod(0o750)

    env = os.environ.copy()
    env["PATH"] = f"{temp}:{env['PATH']}"
    env["MOCK_BATCH"] = str(batch_file)
    env["MOCK_ARGS"] = str(args_file)
    result = subprocess.run(
        [
            str(SLURM), "--name", "snapshot-test", "--physical-cores", "3",
            "--time", "00:05:00", "--", "--quick", "--solver",
            "build/debug/tactix --lrat",
        ],
        cwd=ROOT,
        env=env,
        check=True,
        text=True,
        capture_output=True,
    )

    match = re.search(r"harness: ([0-9a-f]{64}) \(([^)]+)\)", result.stdout)
    assert match, result.stdout
    reported_hash, pin_text = match.groups()
    pin = Path(pin_text).resolve()
    expected_parent = (ROOT / "build").resolve()
    assert pin.parent == expected_parent
    assert pin.name.startswith("pin-fuzz-harness-")

    try:
        pinned_fuzz = pin / "fuzz"
        pinned_run = pinned_fuzz / "run.sh"
        assert pinned_run.is_file() and os.access(pinned_run, os.X_OK)
        assert pinned_run.stat().st_mode & 0o222 == 0

        batch = batch_file.read_text(encoding="utf-8")
        assert f'"{pinned_run}"' in batch
        assert f'"{ROOT}/tools/fuzz/run.sh"' not in batch
        assert f'TACTIX_REPO_ROOT="{ROOT}"' in batch
        assert f'TACTIX_FUZZ_OUTPUT_ROOT="{ROOT}/tools/fuzz/failures"' in batch
        assert "harness-sha256=" + reported_hash in batch
        assert '--jobs "3"' in batch

        args = args_file.read_text(encoding="utf-8")
        assert "--cpus-per-task=6" in args
        assert "--mem=24000M" in args

        digest_lines = bytearray()
        for entry in sorted(p for p in pinned_fuzz.rglob("*") if p.is_file()):
            relative = "./" + entry.relative_to(pinned_fuzz).as_posix()
            digest = hashlib.sha256(entry.read_bytes()).hexdigest()
            digest_lines.extend(f"{digest}  {relative}\n".encode())
        actual_hash = hashlib.sha256(digest_lines).hexdigest()
        assert actual_hash == reported_hash
    finally:
        if pin.exists():
            make_writable(pin)
            shutil.rmtree(pin)

print("fuzz SLURM immutable harness snapshot: PASS")
