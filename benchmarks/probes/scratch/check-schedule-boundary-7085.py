#!/usr/bin/env python3
"""One-shot actual-code diagnostic; never executes a real-instance solver."""
import hashlib
import itertools
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "benchmarks/runs/20260910-7085-schedule-boundary"
OUT.mkdir(exist_ok=False)
SOURCE = ROOT / ".claude/worktrees/runtime-collect-retry-7030"
BUILD = ROOT / "build/pin-7030-collect-retry-release/tests"
FIXTURE = ROOT / "benchmarks/probes/scratch/schedule-boundary-7085"
hashes = {}
commands = []

def bind(path):
    path = Path(path).resolve()
    value = hashlib.sha256(path.read_bytes()).hexdigest()
    hashes[str(path.relative_to(ROOT))] = value
    return value

def run(args, **kwargs):
    commands.append([str(arg) for arg in args])
    return subprocess.run(args, cwd=ROOT, text=True, capture_output=True,
                          check=True, **kwargs).stdout

manifest_path = ROOT / "benchmarks/runs/20260910-7030-collect-retry-build/manifest.json"
manifest = json.loads(manifest_path.read_text())
bind(manifest_path)
for path, expected in manifest["files"].items():
    if bind(SOURCE / path) != expected:
        raise RuntimeError(f"source changed: {path}")
link_path = BUILD / "CMakeFiles/test_scheduling.dir/link.txt"
bind(link_path)
link = shlex.split(link_path.read_text())
compiler = link[0]
for suffix in (".c", ".cpp"):
    bind(FIXTURE.with_suffix(suffix))
bind(__file__)
candidate_object = OUT / "fixture.o"
run([compiler, "-O3", "-DNDEBUG", "-std=c++20", "-fno-exceptions", "-fno-rtti",
     "-I" + str(SOURCE / "src"), "-I" + str(SOURCE / "include"), "-c",
     str(FIXTURE.with_suffix(".cpp")), "-o", str(candidate_object)])
objects = []
for arg in link[1:]:
    if arg.endswith(".o") and "test_scheduling.cpp.o" not in arg:
        obj = (BUILD / arg).resolve()
        bind(obj)
        objects.append(str(obj))
if len(objects) < 30:
    raise RuntimeError("unexpected candidate link inventory")
run([compiler, "-O3", "-DNDEBUG", str(candidate_object), *objects,
     "-o", str(OUT / "candidate")])
native = ROOT / "kissat"
for path in sorted((native / "src").glob("*.h")):
    bind(path)
native_objects = []
for source in (native / "src/probe.c", native / "src/eliminate.c", FIXTURE.with_suffix(".c")):
    bind(source)
    obj = OUT / (source.stem + ".native.o")
    run(["gcc", "-O3", "-DNDEBUG", "-ffunction-sections", "-fdata-sections",
         "-I" + str(native / "src"), "-I" + str(native / "build"),
         "-c", str(source), "-o", str(obj)])
    native_objects.append(str(obj))
run(["gcc", "-Wl,--gc-sections", *native_objects, "-o", str(OUT / "native")])
rows = {}
for arm in ("candidate", "native"):
    bind(OUT / arm)
    output = run([str(OUT / arm)])
    (OUT / (arm + ".tsv")).write_text(output)
    bind(OUT / (arm + ".tsv"))
    rows[arm] = [list(map(int, line.split("\t"))) for line in output.splitlines()]
    expected = []
    for c, same, material, reduced in itertools.product((99, 100, 101), (0, 1), (0, 1), (0, 1)):
        if arm == "candidate":
            probe, elim = c > 100 and reduced, c >= 100 and material
        else:
            probe, elim = c > 100 and not same, c >= 100 and material and not same
        expected.append([c, same, material, reduced, int(probe), int(elim)])
    if rows[arm] != expected:
        raise RuntimeError(f"unexpected actual-code result: {arm}")
trace = ROOT / "benchmarks/runs/20260910-7064-rank31-full-boundaries/traces/candidate.macro.tsv"
bind(trace)
phase_starts = same_conflict = 0
last_reduce = None
with trace.open() as stream:
    if next(stream).rstrip("\n") != "# tactix-macro-boundary\t1":
        raise RuntimeError("wrong macro version")
    header = next(stream).rstrip("\n").removeprefix("# ").split("\t")
    previous_seq = -1
    for line in stream:
        fields = line.rstrip("\n").split("\t")
        if len(fields) != len(header):
            raise RuntimeError("truncated macro row")
        row = dict(zip(header, fields))
        seq, conflicts = int(row["seq"]), int(row["conflicts"])
        if seq <= previous_seq:
            raise RuntimeError("non-increasing macro sequence")
        previous_seq = seq
        if row["event"] == "reduce_end":
            last_reduce = conflicts
        elif row["event"] in ("probe_begin", "eliminate_begin"):
            phase_starts += 1
            same_conflict += conflicts == last_reduce
summary = {
    "scope": "posed invitation predicates, not full reachable solver states or performance",
    "rows_per_arm": 24, "predicate_checks": 96,
    "mismatches": [{"candidate": a, "native": b} for a, b in zip(rows["candidate"], rows["native"]) if a != b],
    "rank31_trace": {"phase_starts": phase_starts, "same_conflict_after_reduce": same_conflict},
    "hashes": hashes, "commands": commands,
}
(OUT / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
print(json.dumps({k: v for k, v in summary.items() if k not in ("hashes", "commands", "mismatches")}))
