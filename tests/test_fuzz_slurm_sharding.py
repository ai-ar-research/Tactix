#!/usr/bin/env python3
"""The cpu-q fuzz mode must shard one immutable deterministic campaign."""

import importlib.util
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SLURM = ROOT / "tools" / "fuzz" / "slurm.sh"
SHARD = ROOT / "tools" / "fuzz" / "shard.py"

spec = importlib.util.spec_from_file_location("tactix_fuzz_shard", SHARD)
assert spec and spec.loader
shard_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(shard_module)

for total, shards in ((1, 1), (10, 3), (130, 64), (100000, 64)):
    ranges = [shard_module.shard_range(total, shards, shard)
              for shard in range(shards)]
    assert ranges[0][0] == 0
    assert sum(count for _, count in ranges) == total
    for left, right in zip(ranges, ranges[1:]):
        assert left[0] + left[1] == right[0]
    assert ranges[-1][0] + ranges[-1][1] == total
    assert max(count for _, count in ranges) - min(
        count for _, count in ranges) <= 1

for runner in ("fuzz.py", "controller_v5.py", "forced_ttf.py"):
    source = (ROOT / "tools" / "fuzz" / runner).read_text(encoding="utf-8")
    assert "range(args.index_offset, args.index_offset + args.n)" in source


def make_writable(path: Path) -> None:
    for entry in path.rglob("*"):
        entry.chmod(entry.stat().st_mode | stat.S_IWUSR)
    path.chmod(path.stat().st_mode | stat.S_IWUSR)


with tempfile.TemporaryDirectory(prefix="tactix-fuzz-array-") as tmp:
    temp = Path(tmp)
    mock = temp / "sbatch"
    mock.write_text(
        "#!/usr/bin/env python3\n"
        "import os, pathlib, sys\n"
        "root = pathlib.Path(os.environ['MOCK_ROOT'])\n"
        "counter = root / 'counter'\n"
        "index = int(counter.read_text()) if counter.exists() else 0\n"
        "(root / f'batch-{index}.sh').write_text(sys.stdin.read())\n"
        "(root / f'args-{index}.txt').write_text('\\n'.join(sys.argv[1:]))\n"
        "counter.write_text(str(index + 1))\n"
        "print(424242 + index)\n",
        encoding="utf-8",
    )
    mock.chmod(0o750)

    env = os.environ.copy()
    env["PATH"] = f"{temp}:{env['PATH']}"
    env["MOCK_ROOT"] = str(temp)
    result = subprocess.run(
        [
            str(SLURM), "--cpu-q-wide", "--name", "array-test",
            "--time", "00:05:00", "--", "--n", "130", "--solver",
            "build/debug/tactix --pbp",
        ],
        cwd=ROOT,
        env=env,
        check=False,
        text=True,
        capture_output=True,
    )
    assert result.returncode == 0, (result.stdout, result.stderr)

    match = re.search(r"harness: ([0-9a-f]{64}) \(([^)]+)\)", result.stdout)
    assert match, result.stdout
    pin = Path(match.group(2)).resolve()
    try:
        array_args = (temp / "args-0.txt").read_text(encoding="utf-8")
        final_args = (temp / "args-1.txt").read_text(encoding="utf-8")
        array_batch = (temp / "batch-0.sh").read_text(encoding="utf-8")
        final_batch = (temp / "batch-1.sh").read_text(encoding="utf-8")
        subprocess.run(["bash", "-n", str(temp / "batch-0.sh")], check=True)
        subprocess.run(["bash", "-n", str(temp / "batch-1.sh")], check=True)

        assert "--partition=cpu-q" in array_args
        assert "--cpus-per-task=13" in array_args
        assert "--mem=104000M" in array_args
        assert "--array=0-63%16" in array_args
        assert "%A_%a" in array_args
        assert f'python3 "{pin}/fuzz/shard.py" "130" "64"' in array_batch
        assert '--index-offset "$INDEX_OFFSET"' in array_batch
        assert "--defer-requirements" in array_batch
        assert "SHARD_ARGS+=(--skip-corpus --skip-fixed-controls)" in array_batch
        assert f'"{pin}/fuzz/run.sh"' in array_batch

        assert "--dependency=afterany:424242" in final_args
        assert "--cpus-per-task=1" in final_args
        assert "missing shard status" in final_batch
        assert f'python3 "{pin}/fuzz/aggregate.py"' in final_batch
        assert "64/64 shards, 130 deterministic cases" in final_batch
        assert "submitted finalizer: 424243" in result.stdout
        assert "total-eligible-workers=832" in result.stdout
        assert "max-concurrent-workers=208" in result.stdout
    finally:
        if pin.exists():
            make_writable(pin)
            shutil.rmtree(pin)

    over_node = subprocess.run(
        [
            str(SLURM), "--partition", "cpu-q", "--shards", "1",
            "--physical-cores", "53", "--threads-per-core", "1", "--",
            "--n", "10000", "--solver", "build/debug/tactix --pbp",
        ],
        cwd=ROOT,
        env=env,
        check=False,
        text=True,
        capture_output=True,
    )
    assert over_node.returncode == 2
    assert "at most 52 CPUs per node/task" in over_node.stderr

print("fuzz SLURM deterministic sharding: PASS")
