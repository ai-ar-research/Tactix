"""Compare already-validated queue requests by exact conflict/decision clock.

Membership is a multiset, not a set. Raw stamps, links and values remain in
the input evidence; this view does not claim that equal requests mean equal
queue state or that unequal requests identify a solver defect.
"""
from collections import Counter


def group_bumps(rows):
    groups = {}
    previous = None
    for row in rows:
        clock = (row['conflicts'], row['decisions'])
        if previous is not None and any(a < b for a, b in zip(clock, previous)):
            raise ValueError('nonmonotone bump counters')
        groups.setdefault(clock, []).append(row['variable'])
        previous = clock
    return groups


def compare_groups(candidate, reference):
    a, b = group_bumps(candidate), group_bumps(reference)
    differences = []
    for clock in sorted(a.keys() | b.keys()):
        left, right = a.get(clock, []), b.get(clock, [])
        if left == right:
            continue
        ac, bc = Counter(left), Counter(right)
        differences.append(dict(clock=list(clock), counts=[len(left), len(right)],
            candidate_only=list((ac - bc).elements()),
            reference_only=list((bc - ac).elements()),
            same_members=ac == bc, candidate_order=left, reference_order=right))
    return dict(group_counts=[len(a), len(b)], differences=differences)
