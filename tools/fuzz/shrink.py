"""CNF delta-debugging core used by the `ddmin` CLI and by fuzz.py's
in-bundle shrinking (plan §8.2).

minimize() takes an in-process predicate `pred(nvars, clauses) -> bool`
(True iff the failure of interest is still present) so per-candidate cost is
one solver run, not several interpreter startups.
"""


def _clause_sweeps(pred, nvars, clauses, budget):
    changed = True
    while changed and budget[0] > 0:
        changed = False
        i = 0
        while i < len(clauses) and budget[0] > 0:
            candidate = clauses[:i] + clauses[i + 1:]
            budget[0] -= 1
            if candidate and pred(nvars, candidate):
                clauses = candidate
                changed = True
            else:
                i += 1
    return clauses


def _literal_sweep(pred, nvars, clauses, budget):
    ci = 0
    while ci < len(clauses) and budget[0] > 0:
        li = 0
        while li < len(clauses[ci]) and budget[0] > 0:
            candidate = [list(c) for c in clauses]
            candidate[ci].pop(li)
            budget[0] -= 1
            if pred(nvars, candidate):
                clauses = candidate
            else:
                li += 1
        ci += 1
    return clauses


def minimize(pred, nvars, clauses, max_evals=2000, max_seconds=None):
    """Returns (nvars, clauses, evals_used). Interleaves clause-level and
    literal-level reduction until fixpoint or budget exhaustion, then
    renumbers variables densely (kept only if the failure survives it).

    The coarse phase tries only a few chunked rounds: failing cores are
    usually spread out, so the classic ddmin granularity ladder mostly
    burns budget on contiguous chunks that never drop. `max_seconds`
    bounds wall clock: near-transition random instances have genuinely
    huge cores and expensive evals, and shrinking must not stall a run.
    """
    budget = [max_evals]
    if max_seconds is not None:
        import time
        deadline = time.monotonic() + max_seconds
        inner_pred = pred

        def pred(nv, cls):  # noqa: F811 — deliberate wrap
            if time.monotonic() > deadline:
                budget[0] = 0
                return False
            return inner_pred(nv, cls)

    for granularity in (2, 4, 8):
        chunk = max(1, len(clauses) // granularity)
        i = 0
        while i < len(clauses) and budget[0] > 0:
            candidate = clauses[:i] + clauses[i + chunk:]
            budget[0] -= 1
            if candidate and pred(nvars, candidate):
                clauses = candidate
            else:
                i += chunk

    while budget[0] > 0:
        before = (len(clauses), sum(len(c) for c in clauses))
        clauses = _clause_sweeps(pred, nvars, clauses, budget)
        clauses = _literal_sweep(pred, nvars, clauses, budget)
        if (len(clauses), sum(len(c) for c in clauses)) == before:
            break

    used = sorted({abs(l) for c in clauses for l in c})
    remap = {v: i + 1 for i, v in enumerate(used)}
    renumbered = [[(1 if l > 0 else -1) * remap[abs(l)] for l in c]
                  for c in clauses]
    if pred(len(used), renumbered):
        nvars, clauses = len(used), renumbered

    return nvars, clauses, max_evals - budget[0]
