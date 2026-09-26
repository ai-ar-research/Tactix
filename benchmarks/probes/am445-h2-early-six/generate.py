from pathlib import Path
import hashlib
import sys
import tarfile

probe = Path(__file__).resolve().parent
root = probe.parents[2]
sys.path.insert(0, str(root / "tools/bench"))
import proof_oracle

archive = root / "benchmarks/probes/am423-h2-current/job-1722271.tar.gz"
with tarfile.open(archive) as tar:
    binary = tar.extractfile("out/1722271/current.dsr").read()
    cnf = tar.extractfile("out/1722271/checker.cnf").read()
assert hashlib.sha256(binary).hexdigest() == (
    "1327bb08a0fbde95a662bbb299921d87549546e309f47c9aa54e4b4a46f46889"
)
(probe / "original-binary.dsr").write_bytes(binary)
(probe / "checker.cnf").write_bytes(cnf)
lines = []
for kind, tokens in proof_oracle._dsr_lines(probe / "original-binary.dsr", True):
    literals = [(-1 if token & 1 else 1) * (token >> 1) for token in tokens]
    lines.append(("d " if kind == "d" else "") + " ".join(map(str, literals)) + " 0\n")
assert len(lines) == 134 and lines[0] == "17 0\n"
(probe / "baseline-ascii.dsr").write_text("".join(lines))
move = {19, 21, 23, 28, 29, 30}
assert all(lines[index - 1].startswith("d ") for index in move)
(probe / "early-six.dsr").write_text(
    "".join(line for index, line in enumerate(lines, 1) if index in move)
    + "".join(line for index, line in enumerate(lines, 1) if index not in move)
)
