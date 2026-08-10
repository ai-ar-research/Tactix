#!/usr/bin/env python3
import importlib.util
import pathlib
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "compare_reduce_trace", ROOT / "tools/bench/compare_reduce_trace.py")
MOD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MOD)

HEADER = "# tactix-kissat-reduce\t1\n"
COLS = "# solver\tkind\treduction\tconflict\tmode\ttier1\ttier2\tlive_red\tcandidates\tvictims\tref\tsize\tglue\tused\treason\teligibility\trank\tselected\tliterals\n"


def summary(solver, live=2, candidates=1, victims=1, tier2=6):
    return (f"{solver}\tsummary\t1\t1000\tfocused\t2\t{tier2}\t{live}\t"
            f"{candidates}\t{victims}\t-\t-\t-\t-\t-\t-\t-\t-\t-\n")


def clause(solver, ref, lits, glue=7, used=0, eligibility="candidate",
           rank=0, selected=1):
    rank = "-" if rank is None else rank
    return (f"{solver}\tclause\t1\t1000\tfocused\t2\t6\t2\t1\t1\t"
            f"{ref}\t2\t{glue}\t{used}\t0\t{eligibility}\t{rank}\t"
            f"{selected}\t{lits}\n")


def run(left_rows, right_rows):
    with tempfile.TemporaryDirectory() as directory:
        d = pathlib.Path(directory)
        left, right = d / "t.tsv", d / "k.tsv"
        left.write_text(HEADER + COLS + "".join(left_rows))
        right.write_text(HEADER + COLS + "".join(right_rows))
        return MOD.compare(left, right)


base_t = [summary("tactix"), clause("tactix", 1, "-2,1"),
          clause("tactix", 2, "-4,3", glue=2, used=31,
                 eligibility="tier1-used", rank=None, selected=0)]
base_k = [summary("kissat"), clause("kissat", 9, "-2,1"),
          clause("kissat", 10, "-4,3", glue=2, used=31,
                 eligibility="tier1-used", rank=None, selected=0)]
assert run(base_t, base_k)["status"] == "match"

r = run(base_t, [summary("kissat", tier2=7)] + base_k[1:])
assert r["category"] == "tier-threshold-mismatch"
r = run(base_t, [summary("kissat", live=1), base_k[1]])
assert r["category"] == "clause-population-mismatch"
r = run(base_t, [base_k[0], clause("kissat", 9, "-2,1", used=1), base_k[2]])
assert r["category"] == "used-history-mismatch"
r = run(base_t, [base_k[0], clause("kissat", 9, "-2,1", selected=0), base_k[2]])
assert r["category"] == "cutoff-mismatch"

# Duplicate clauses are paired as multisets, not overwritten by literal key.
dup_t = [summary("tactix", live=2, candidates=2, victims=1),
         clause("tactix", 1, "-2,1", glue=8, rank=0),
         clause("tactix", 2, "-2,1", glue=7, rank=1, selected=0)]
dup_k = [summary("kissat", live=2, candidates=2, victims=1),
         clause("kissat", 9, "-2,1", glue=7, rank=1, selected=0),
         clause("kissat", 8, "-2,1", glue=8, rank=0)]
assert run(dup_t, dup_k)["status"] == "match"

print("compare-reduce-trace: ok")
