#!/usr/bin/env python3
"""Summarize the `c slot` ledger lines of an audit-dosing probe directory.

Per slot entry, across every instance in the directory: occasions, admits, the
decline histogram, and — the reading the audit is for — spent/handed and
completed/admitted. `handed 0` for an admitted entry means the entry has NO
budget telemetry at all, which is itself the finding.

Usage: audit_dosing_summarize.py <probe-dir> [<probe-dir> ...]
"""
import re
import sys
from collections import defaultdict

FIELDS = ("dis", "cap", "pre", "grd", "bud", "dly", "mat", "warm", "dns",
          "one", "nv")
LINE = re.compile(
    r"^c slot (?P<name>\S+)\s+occ\s+(?P<occ>\d+) adm\s+(?P<adm>\d+) "
    r"ok\s+(?P<ok>\d+) \| dec (?P<dec>.*?) \| handed (?P<handed>\d+) "
    r"spent (?P<spent>\d+) completed (?P<completed>\d+) \| "
    r"maxeff (?P<maxeff>\d+) clamped (?P<clamped>\d+) floored (?P<floored>\d+) "
    r"wrapped (?P<wrapped>\d+)")


def main(dirs):
    agg = defaultdict(lambda: defaultdict(int))
    order = []
    insts = defaultdict(set)
    worst = {}
    for d in dirs:
        import pathlib
        for log in sorted(pathlib.Path(d).glob("*.log")):
            for line in log.read_text(errors="replace").splitlines():
                m = LINE.match(line)
                if not m:
                    continue
                name = m.group("name")
                if name not in agg:
                    order.append(name)
                a = agg[name]
                insts[name].add(log.name)
                for k in ("occ", "adm", "ok", "handed", "spent", "completed",
                          "clamped", "floored", "wrapped"):
                    a[k] += int(m.group(k))
                a["maxeff"] = int(m.group("maxeff"))
                toks = m.group("dec").split()
                for i in range(0, len(toks) - 1, 2):
                    a["dec_" + toks[i]] += int(toks[i + 1])
                if int(m.group("adm")) and not int(m.group("handed")):
                    a["no_budget_telemetry"] += 1
                h, s = int(m.group("handed")), int(m.group("spent"))
                if h and s / h > worst.get(name, (0, ""))[0]:
                    worst[name] = (s / h, log.name)
    hdr = ("entry", "insts", "occ", "adm", "ok", "cmpl", "handed", "spent",
           "sp/hd", "cmpl/adm", "top declines", "wrapped")
    print("%-18s %5s %6s %6s %6s %5s %14s %14s %7s %8s  %-34s %s" % hdr)
    for name in order:
        a = agg[name]
        sp = ("%.3f" % (a["spent"] / a["handed"])) if a["handed"] else "-"
        ca = ("%.2f" % (a["completed"] / a["adm"])) if a["adm"] else "-"
        decs = sorted(((v, k[4:]) for k, v in a.items()
                       if k.startswith("dec_") and v), reverse=True)
        dtxt = " ".join("%s %d" % (k, v) for v, k in decs[:4]) or "(none)"
        print("%-18s %5d %6d %6d %6d %5d %14d %14d %7s %8s  %-34s %d" %
              (name, len(insts[name]), a["occ"], a["adm"], a["ok"],
               a["completed"], a["handed"], a["spent"], sp, ca, dtxt,
               a["wrapped"]))
    print()
    print("worst single-instance spent/handed")
    for name in order:
        if name in worst:
            print("  %-18s %8.2fx  %s" % (name, worst[name][0], worst[name][1]))
    print()
    for name in order:
        if agg[name]["no_budget_telemetry"]:
            print("NO BUDGET TELEMETRY: %-18s %d instances with adm>0 and "
                  "handed==0" % (name, agg[name]["no_budget_telemetry"]))


if __name__ == "__main__":
    main(sys.argv[1:] or ["."])
