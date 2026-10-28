#!/usr/bin/env python3
"""Write an independently counted daily TeX/PDF checkpoint for a Tier-D run."""

import argparse
import datetime as dt
import json
import os
import subprocess
import time
from collections import Counter
from pathlib import Path

ROOT = Path(os.environ.get("TACTIX_REPO_ROOT", Path(__file__).resolve().parents[2])).resolve()


def sha256(path):
    import hashlib
    return hashlib.sha256(path.read_bytes()).hexdigest()


def merged_campaign_state(campaign, repair_campaigns):
    campaigns = [campaign, *repair_campaigns]
    manifest = json.loads((campaign / "manifest.json").read_text())
    expected = len(manifest["tasks"])
    results = {}
    pending = []
    claims = []
    launches = []
    monitor_records = []
    for source in campaigns:
        source_manifest = json.loads((source / "manifest.json").read_text())
        if len(source_manifest.get("tasks", [])) != expected:
            raise RuntimeError("report campaign task dimensions disagree")
        if source != campaign:
            declared = source_manifest.get("repair_of", {})
            if (declared.get("campaign") != campaign.name or
                    declared.get("manifest_sha256") !=
                    sha256(campaign / "manifest.json")):
                raise RuntimeError("report repair does not bind the base campaign")
        for path in sorted((source / "results").glob("*.json")):
            index = int(path.stem)
            if index in results:
                raise RuntimeError(f"duplicate merged result row {index}")
            results[index] = path
        pending.extend((source / "pending").glob("*.json"))
        claims.extend((source / "claims").glob("*.json"))
        launch_paths = [source / "launch.json"]
        launch_paths.extend(sorted((source / "rounds").glob("*/launch.json")))
        for launch_path in launch_paths:
            if launch_path.is_file():
                launches.append(json.loads(launch_path.read_text())["array_job_id"])
        monitor_logs = [source / "governor/governor.log.jsonl"]
        monitor_logs.extend(sorted((source / "rounds").glob(
            "*/governor/governor.log.jsonl")))
        for log_path in monitor_logs:
            if log_path.is_file() and log_path.stat().st_size:
                monitor_records.append(json.loads(
                    log_path.read_text().splitlines()[-1]))
    if any(index < 0 or index >= expected for index in results):
        raise RuntimeError("merged result index outside base campaign")
    return manifest, expected, results, pending, claims, launches, monitor_records


def snapshot(campaign, repair_campaigns, output_root):
    (manifest, expected, results, pending, claims,
     launches, monitor_records) = merged_campaign_state(campaign, repair_campaigns)
    counts = Counter()
    bad_cleanup = []
    for path in results.values():
        item = json.loads(path.read_text())
        if item.get("valid"):
            counts["verified SAT" if item.get("verdict") == "SAT" else "verified UNSAT"] += 1
        else:
            counts["unverified or no answer"] += 1
            if item.get("solve", {}).get("exit_code") == 139:
                counts["solver exit 139"] += 1
        if item.get("cleanup_pending") or not item.get("scratch_deleted") or not item.get("artifact_deleted"):
            bad_cleanup.append(path.name)
    now = dt.datetime.now(dt.timezone.utc)
    directory = output_root / (now.strftime("%Y%m%dT%H%M%SZ") + "-cms-tierd-daily")
    directory.mkdir(parents=True, exist_ok=False)
    last_monitor = max(monitor_records, key=lambda item: item.get("time", ""),
                       default={})
    array_job = ", ".join(map(str, launches)) or "none"
    usage = last_monitor.get("usage")
    usage_text = "unavailable" if usage is None else f"{usage:.1f} GiB"
    tex = r"""\documentclass[11pt]{article}
\usepackage[margin=1in]{geometry}
\usepackage[T1]{fontenc}
\title{CryptoMiniSat Tier D: Daily Campaign Checkpoint}
\author{Tactix development report for Andrew}
\date{%s UTC}
\begin{document}
\maketitle
\section*{Measured campaign state}
Arrays %s run pinned CryptoMiniSat 5.14.7 on cpu-q against the frozen
1,166-instance Tier D union. This snapshot counted %d of %d final result
records, %d claims, and %d cleanup journals. Of the final results, %d are
independently verified SAT, %d are independently verified UNSAT, and %d have
no accepted answer; %d of the latter report solver exit 139. %d final records
show an incomplete cleanup flag. These counts are a point-in-time view, not
a CG selection or candidate acceptance.

The last storage-monitor check reported %s and action \texttt{%s}. The
companion job checks every 600 seconds, holds pending array tasks above
750 GiB, and releases only its own holds below 750 GiB. Running tasks are
untouched. Provenance, checker stages, times, memory, and cleanup flags live
in the campaign's pinned manifest and per-instance JSON records.

\section*{Open gates and next work}
Only a complete, independently checked CryptoMiniSat rerun can establish
exclusive and tenfold wins and freeze CG. The Tactix objective remains one
DefaultPolicy strategy within twice CryptoMiniSat on each CG row, subject to
N1--N4, fuzz, full Debug, and regression gates. A public GF(3) nonroot
vertical and a public C v17 cardinality search action have focused proof checks;
cardinality discovery, default scheduling, and acceptance remain open. This
automatic checkpoint does not infer acceptance from Slurm completion or a
solver answer alone.
\end{document}
""" % (now.strftime("%d %B %Y, %H:%M"), array_job, len(results), expected, len(claims),
       len(pending), counts["verified SAT"], counts["verified UNSAT"],
       counts["unverified or no answer"], counts["solver exit 139"],
       len(bad_cleanup), usage_text, last_monitor.get("action", "unknown"))
    (directory / "report.tex").write_text(tex)
    with (directory / "build.log").open("w") as log:
        for _ in range(2):
            subprocess.run(["pdflatex", "-interaction=nonstopmode", "-halt-on-error",
                            "report.tex"], cwd=directory, stdout=log,
                           stderr=subprocess.STDOUT, check=True, timeout=180)
    pdf = directory / "report.pdf"
    if not pdf.is_file() or pdf.read_bytes()[:5] != b"%PDF-":
        raise RuntimeError("daily report PDF missing or invalid")
    (directory / "snapshot.json").write_text(json.dumps({
        "campaign": str(campaign.relative_to(ROOT)),
        "repair_campaigns": [str(path.relative_to(ROOT))
                             for path in repair_campaigns],
        "array_jobs": launches, "time_utc": now.isoformat(),
        "result_count": len(results), "expected_count": expected,
        "counts": dict(counts), "pending_cleanup": len(pending),
        "bad_final_cleanup": bad_cleanup, "monitor": last_monitor}, indent=2) + "\n")
    print(pdf, flush=True)
    return len(results) == expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--campaign", type=Path, required=True)
    parser.add_argument("--repair-campaign", type=Path, action="append", default=[])
    parser.add_argument("--interval", type=int, default=86400)
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--resubmit-batch", type=Path)
    parser.add_argument("--next-delay", default="6hours")
    args = parser.parse_args()
    campaign = args.campaign.resolve()
    repairs = [path.resolve() for path in args.repair_campaign]
    while True:
        if not args.once:
            time.sleep(args.interval)
        complete = snapshot(campaign, repairs, ROOT / "report-to-andrew")
        if args.resubmit_batch and not complete:
            command = ["sbatch", "--parsable", f"--begin=now+{args.next_delay}",
                       str(args.resubmit_batch.resolve())]
            next_job = subprocess.run(command, capture_output=True, text=True,
                                      check=True, timeout=120).stdout.strip().split(";")[0]
            with (args.resubmit_batch.parent / "submissions.jsonl").open("a") as log:
                log.write(json.dumps({"scheduled_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
                                      "job_id": next_job, "delay": args.next_delay}) + "\n")
                log.flush()
        if complete or args.once:
            return


if __name__ == "__main__":
    main()
