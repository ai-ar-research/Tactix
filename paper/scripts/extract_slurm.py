#!/usr/bin/env python3
"""Extract the SLURM scripts of the paper's runs from the archived campaigns.

    python3 paper/scripts/extract_slurm.py

Writes paper/slurm/<experiment>/<campaign>/*.sbatch (the scripts exactly as
submitted) and paper/slurm/campaigns.md, a table of each campaign's
resources (partition, cores, memory, time, array size and concurrency) and
the harness entry point it ran.
"""
from __future__ import annotations

import json
import re
import tarfile
from pathlib import Path

PAPER = Path(__file__).resolve().parents[1]
OUT = PAPER / "slurm"


def directives(text: str) -> dict[str, str]:
    found = {}
    for line in text.splitlines():
        if not line.startswith("#SBATCH"):
            continue
        for key, value in re.findall(r"--([\w-]+)(?:=(\S+))?", line):
            found[key] = value
        for key, value in re.findall(r"(?:^|\s)-(\w)\s+(\S+)", line[len("#SBATCH"):]):
            found[{"p": "partition", "c": "cpus-per-task", "t": "time", "a": "array"}.get(key, key)] = value
    if "mem-per-cpu" in found and "mem" not in found and found.get("cpus-per-task"):
        number = re.match(r"(\d+)(\D*)", found["mem-per-cpu"])
        found["mem"] = f"{int(number.group(1)) * int(found['cpus-per-task'])}{number.group(2)}"
    return found


def entry(text: str) -> str:
    for line in text.splitlines():
        if line.startswith("exec "):
            match = re.search(r"((?:tools/bench|pins|runner)/[\w./-]+\.py)(\s+--?[\w-]+)?", line)
            if match:
                script = re.sub(r"^.*?(pins|runner)/", r"\1/", match.group(1))
                return script + (match.group(2) or "")
    return ""


def main() -> None:
    index = json.loads((PAPER / "logs/index.json").read_text())
    rows = []
    for entry_ in index:
        archive = PAPER / entry_["archive"]
        if not archive.name.endswith(".tar.xz"):
            continue
        with tarfile.open(archive) as tar:
            for member in tar:
                name = member.name.split("/", 1)[-1]
                if not member.isfile() or "/" in name or not name.endswith(".sbatch"):
                    continue
                text = tar.extractfile(member).read().decode()
                campaign = archive.name.removesuffix(".tar.xz")
                target = OUT / entry_["experiment"] / campaign / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(text)
                d = directives(text)
                rows.append([entry_["experiment"], campaign, name.removesuffix(".sbatch"),
                             d.get("partition", ""), d.get("cpus-per-task", ""), d.get("mem", ""),
                             d.get("time", ""), d.get("array", ""),
                             f"`{entry(text)}`" if entry(text) else ""])
    header = ["Experiment", "Campaign", "Script", "Partition", "Cores", "Memory", "Time", "Array", "Runs"]
    lines = ["# SLURM scripts of the paper's runs", "",
             "Extracted from the archived campaigns (`paper/logs/*/raw/`) by",
             "`paper/scripts/extract_slurm.py`; each script is in `<experiment>/<campaign>/`.",
             "`array` runs one task per (configuration, formula); `validate` and `aggregate`",
             "check the campaign's records and collect them. `pins/X.py` is the campaign's",
             "frozen copy of `tools/bench/X.py`; `runner/X.py` likewise.", "",
             "| " + " | ".join(header) + " |", "|" + "---|" * len(header)]
    lines += ["| " + " | ".join(r) + " |" for r in rows]
    (OUT / "campaigns.md").write_text("\n".join(lines) + "\n")
    print(f"{len(rows)} scripts -> {OUT}")


if __name__ == "__main__":
    main()
