#!/usr/bin/env python3
"""Analyze existing controller regressions; never execute a solver or checker."""
import csv
import hashlib
import json
import pathlib
import re
import statistics

ROOT = pathlib.Path(__file__).resolve().parents[3]
OUT = pathlib.Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text())


def statistics_of(row):
    counts, timings, normalized = {}, {}, []
    for line in row.get("solver_statistics", []):
        if not line.startswith(("c timing", "c phasecost")):
            normalized.append(re.sub(r"\d+\.\d+ seconds", "<seconds>", line))
        for name in ("decisions", "conflicts", "propagations", "restarts", "inprocess"):
            match = re.match(r"c " + name + r"\s+(\d+)", line)
            if match:
                counts[name] = int(match[1])
        match = re.match(r"c timing\s+(\d+) ms solve", line)
        if match:
            timings["solve_seconds"] = int(match[1]) / 1000
        match = re.match(r"c timing\s+(\d+) ms \([^)]*\) inprocessing slot, (\d+) ms", line)
        if match:
            timings.update(slot_seconds=int(match[1]) / 1000,
                           search_scaffolding_seconds=int(match[2]) / 1000)
        match = re.match(r"c timing\s+(\S+)\s+(\d+) ms.* in (\d+) applications", line)
        if match:
            timings[match[1]] = {"seconds": int(match[2]) / 1000,
                               "applications": int(match[3])}
    return counts, timings, hashlib.sha256("\n".join(normalized).encode()).hexdigest()


def measurements(row, path):
    counts, timing, work_hash = statistics_of(row)
    perf = {}
    for line in row.get("runtime_context", {}).get("perf_statistics", []):
        fields = line.split("\t")
        if len(fields) > 2:
            try:
                perf[fields[2]] = float(fields[0].replace(",", ""))
            except ValueError:
                pass
    check = row.get("proof_check")
    return dict(source=str(path.relative_to(ROOT)), source_sha256=digest(path),
                solver_sha256=row["binary_sha256"], solve=row["solve"],
                verdict=row["verdict"], node=row["node"], affinity=row["cpu_affinity"],
                counts=counts, timing=timing, normalized_statistics_sha256=work_hash,
                perf=perf, certificate_bytes=row.get("certificate_bytes"),
                certificate_sha256=row.get("certificate_sha256"),
                checker={key: check.get(key) for key in
                         ("result", "wall_seconds", "timed_out", "exit_code", "pipeline")}
                if check else None)


def main():
    bp = ROOT / "benchmarks/baselines/controller-foundations-v1/results.json"
    baseline, rows, summaries, manifests = read(bp), [], {}, {}
    for name, spec in baseline["sets"].items():
        runs = {label: ROOT / f"benchmarks/runs/20261001-controller-regression-{sha}-{name}"
                for label, sha in (("candidate", "12e3c4f0"), ("earlier", "eba47800"))}
        indexed = {}
        for label, run in runs.items():
            mp = run / "manifest.json"
            manifest = read(mp)
            manifests[f"{name}/{label}"] = dict(path=str(mp.relative_to(ROOT)),
                                                sha256=digest(mp), arms=manifest["arms"],
                                                resources=manifest["resource_contract"])
            indexed[label] = {row["instance"]: (path, row)
                              for path in sorted((run / "results").glob("*.json"))
                              for row in [read(path)]}
            assert len(indexed[label]) == spec["expected_rows"]
        start = len(rows)
        for frozen in spec["rows"]:
            path = ROOT / frozen["source"]
            assert digest(path) == frozen["source_sha256"], path
            raw = read(path)
            assert raw["binary_sha256"] == baseline["accepted_solver_sha256"]
            measured = {"baseline": measurements(raw, path)}
            for label in runs:
                path, row = indexed[label][raw["instance"]]
                assert row["binary_sha256"] == manifests[f"{name}/{label}"]["arms"][row["arm"]]["sha256"]
                measured[label] = measurements(row, path)
            b, c = measured["baseline"], measured["candidate"]
            paired = not b["solve"]["timed_out"] and not c["solve"]["timed_out"]
            bt, ct = b["solve"]["wall_seconds"], c["solve"]["wall_seconds"]
            rows.append(dict(set=name, rank=raw["rank"], instance=raw["instance"],
                             paired_solved=paired, delta_seconds=round(ct-bt, 6),
                             ratio=ct/bt if paired and bt else None,
                             normalized_statistics_equal=paired and
                             b["normalized_statistics_sha256"] == c["normalized_statistics_sha256"],
                             certificate_equal=paired and
                             b["certificate_sha256"] == c["certificate_sha256"], **measured))
        paired = [row for row in rows[start:] if row["paired_solved"]]
        summaries[name] = dict(rows=spec["expected_rows"], paired_solved=len(paired),
                              baseline_seconds=round(sum(r["baseline"]["solve"]["wall_seconds"] for r in paired), 3),
                              candidate_seconds=round(sum(r["candidate"]["solve"]["wall_seconds"] for r in paired), 3),
                              median_ratio=statistics.median(r["ratio"] for r in paired),
                              normalized_statistics_equal=sum(r["normalized_statistics_equal"] for r in paired),
                              certificate_equal=sum(r["certificate_equal"] for r in paired),
                              added_seconds=round(sum(max(0, r["delta_seconds"]) for r in paired), 3),
                              saved_seconds=round(sum(max(0, -r["delta_seconds"]) for r in paired), 3),
                              timeout_transitions=sum(r["baseline"]["solve"]["timed_out"] !=
                                                      r["candidate"]["solve"]["timed_out"] for r in rows[start:]))
    result = dict(schema="tactix-controller-instance-analysis-v1", baseline=str(bp.relative_to(ROOT)),
                  baseline_sha256=digest(bp), manifests=manifests, summary=summaries, rows=rows)
    header = json.dumps({key: value for key, value in result.items() if key != "rows"}, indent=2)
    body = ",\n".join("    " + json.dumps(row, separators=(",", ":")) for row in rows)
    (OUT / "analysis.json").write_text(header[:-2] + ",\n  \"rows\": [\n" + body + "\n  ]\n}\n")
    with (OUT / "instances.csv").open("w", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(("set", "rank", "instance", "baseline_seconds", "candidate_seconds",
                         "delta_seconds", "ratio", "paired_solved", "work_equal", "certificate_equal",
                         "baseline_node", "candidate_node"))
        for r in sorted(rows, key=lambda r: r["delta_seconds"], reverse=True):
            writer.writerow((r["set"], r["rank"], r["instance"], r["baseline"]["solve"]["wall_seconds"],
                             r["candidate"]["solve"]["wall_seconds"], r["delta_seconds"], r["ratio"],
                             r["paired_solved"], r["normalized_statistics_equal"], r["certificate_equal"],
                             r["baseline"]["node"], r["candidate"]["node"]))
    print(json.dumps(summaries, indent=2))


if __name__ == "__main__":
    main()
