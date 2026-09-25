from pathlib import Path
import tarfile

root = Path(__file__).resolve().parents[3]
archive = root / "benchmarks/probes/am437-h2-unit-publication/artifacts.tar.gz"
with tarfile.open(archive) as tar:
    original = tar.extractfile("./baseline.dsr").read().splitlines(keepends=True)
assert original[6] == b"d -18 19 0\n"
rewritten = original[6] + b"".join(original[:6] + original[7:])
target = Path(__file__).with_name("early-delete.dsr")
assert target.read_bytes() == rewritten
