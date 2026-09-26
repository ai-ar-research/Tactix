#!/usr/bin/env python3
"""Find support dependencies and single-resolvent bridges without a checker oracle."""
from pathlib import Path

root = Path(__file__).resolve().parents[4]
d1 = root / "benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/handoffs/D1-dsr-probe-cce-unit-rejection/min.cnf"
h2 = root / "benchmarks/probes/am436-h2-shrink/minimal.cnf"


def read_cnf(path):
    return [tuple(map(int, line.split()[:-1])) for line in path.read_text().splitlines()
            if line and line[0] not in "cp"]


def rup_support(clauses, lemma):
    values = {-lemma: 0}  # the decision falsifies the proposed unit
    conflict = None
    while conflict is None:
        changed = False
        for idx, clause in enumerate(clauses):
            if any(lit in values for lit in clause):
                continue
            open_lits = [lit for lit in clause if -lit not in values]
            if not open_lits:
                conflict = idx
                break
            if len(open_lits) == 1:
                values[open_lits[0]] = idx + 1
                changed = True
        if not changed and conflict is None:
            return None
    support = set()
    pending = [conflict]
    while pending:
        idx = pending.pop()
        if idx in support:
            continue
        support.add(idx)
        for lit in clauses[idx]:
            reason = values.get(-lit)
            if reason:
                pending.append(reason - 1)
    return support


def root_reasons(clauses, unit):
    """Return the first-clause propagation reasons after publishing one unit."""
    values = {unit}
    reasons = {unit: None}
    while True:
        changed = False
        for idx, clause in enumerate(clauses):
            if any(lit in values for lit in clause):
                continue
            open_lits = [lit for lit in clause if -lit not in values]
            if not open_lits:
                return values, reasons, True
            if len(open_lits) == 1:
                values.add(open_lits[0])
                reasons[open_lits[0]] = idx
                changed = True
        if not changed:
            return values, reasons, False


def resolvents(target, partner):
    for lit in target:
        if -lit not in partner:
            continue
        candidate = tuple(sorted(set(target + partner) - {lit, -lit}))
        if all(-other not in candidate for other in candidate):
            yield candidate


if __name__ == "__main__":
    for name, path, unit, target in (
        ("D1", d1, -14, (21, 10)),
        ("H2", h2, 17, (-18, 19)),
    ):
        clauses = read_cnf(path)
        support = rup_support(clauses, unit)
        assert support is not None
        target_idx = next(i for i, clause in enumerate(clauses) if set(clause) == set(target))
        print(name, "unit", unit, "support_ids", [i + 1 for i in sorted(support)],
              "target_id", target_idx + 1, "target_in_support", target_idx in support)
        if target_idx not in support:
            assert rup_support(clauses[:target_idx] + clauses[target_idx + 1:], unit) is not None
            continue
        bridges = []
        for partner_idx, partner in enumerate(clauses):
            if partner_idx == target_idx:
                continue
            for bridge in resolvents(target, partner):
                without = clauses[:target_idx] + clauses[target_idx + 1:] + [bridge]
                if rup_support(without, unit) is not None:
                    bridges.append((len(bridge), bridge, partner_idx + 1))
        print(name, "single_resolvent_bridges", sorted(bridges)[:12])
