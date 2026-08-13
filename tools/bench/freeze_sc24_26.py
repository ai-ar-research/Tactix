#!/usr/bin/env python3
"""Freeze the content-addressed SC24/25/26 main-track union.

Usage: freeze_sc24_26.py META.sqlite OUTPUT_DIRECTORY

The output is deterministic and this command refuses to replace a differing
file.  GBD hashes are the instance identities; filenames are presentation
metadata only.
"""

import hashlib
import json
import sqlite3
import sys
from pathlib import Path

TRACKS = ("main_2024", "main_2025", "main_2026")


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def install(path, data):
    encoded = data.encode()
    if path.exists():
        if path.read_bytes() != encoded:
            raise SystemExit(f"refusing to replace differing frozen file: {path}")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(f".{path.name}.tmp")
    tmp.write_bytes(encoded)
    tmp.replace(path)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    database, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    if not database.is_file():
        raise SystemExit(f"missing GBD database: {database}")
    con = sqlite3.connect(database)
    marks = ",".join("?" for _ in TRACKS)
    rows = con.execute(
        f"""
        SELECT t.hash, MIN(n.value), x.result,
               MAX(t.value='main_2024'), MAX(t.value='main_2025'),
               MAX(t.value='main_2026')
          FROM track t
          JOIN filename n ON n.hash=t.hash
          JOIN features x ON x.hash=t.hash
         WHERE t.value IN ({marks})
         GROUP BY t.hash, x.result
         ORDER BY t.hash
        """, TRACKS).fetchall()
    if len(rows) != 1166 or sum(sum(r[3:]) for r in rows) != 1200:
        raise SystemExit("GBD selection is not the expected 1166/1200 union")

    sources, instances, membership = [], [], []
    status_counts = {"SAT": 0, "UNSAT": 0, "UNKNOWN": 0}
    year_counts = {"2024": 0, "2025": 0, "2026": 0}
    for h, filename, raw_status, y24, y25, y26 in rows:
        status = {"sat": "SAT", "unsat": "UNSAT", "unknown": "UNKNOWN"}[raw_status]
        stored = f"{h}-{filename}"
        sources.append(f"{h} {stored}")
        instances.append(f"tierD24-26/{stored} {status} 0.0")
        membership.append(
            f"{h}\t{filename}\t{status}\t{int(y24)}\t{int(y25)}\t{int(y26)}")
        status_counts[status] += 1
        for year, present in zip(year_counts, (y24, y25, y26)):
            year_counts[year] += int(present)

    header = (
        "# Frozen SC24/SC25/SC26 Main Track union; generated from GBD track table.\n"
        "# 1166 unique content hashes; 1200 annual memberships. Do not edit.\n"
    )
    install(out / "instances.sources", header + "\n".join(sources) + "\n")
    install(out / "instances.list", header + "\n".join(instances) + "\n")
    install(out / "year-membership.tsv",
            "hash\tfilename\tstatus\tSC2024\tSC2025\tSC2026\n"
            + "\n".join(membership) + "\n")
    metadata = {
        "schema": 1,
        "selection": list(TRACKS),
        "identity": "GBD content hash",
        "unique_instances": len(rows),
        "year_memberships": sum(year_counts.values()),
        "year_counts": year_counts,
        "status_counts": status_counts,
        "gbd_database_sha256": digest(database),
        "files": {},
    }
    for name in ("instances.sources", "instances.list", "year-membership.tsv"):
        metadata["files"][name] = digest(out / name)
    install(out / "dataset.json", json.dumps(metadata, indent=2, sort_keys=True) + "\n")
    print(json.dumps(metadata, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
