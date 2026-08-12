"""Model-checking core shared by the `checkmodel` CLI (plan §8.2 oracle 1)
and fuzz.py. Never trusts solver bookkeeping: a clause counts as satisfied
only by a literal the model actually assigns."""


def parse_model(out_path):
    """Parse competition v lines. Returns dict {var: bool} or None if no
    v lines were present."""
    assignment = {}
    saw_v = False
    with open(out_path, errors="replace") as f:
        for line in f:
            if line.startswith("v ") or line.rstrip() == "v":
                saw_v = True
                for tok in line.split()[1:]:
                    lit = int(tok)
                    if lit != 0:
                        assignment[abs(lit)] = lit > 0
    return assignment if saw_v else None


def check_clauses(clauses, assignment):
    """Returns (ok, message)."""
    for i, clause in enumerate(clauses):
        if not any(assignment.get(abs(l)) is (l > 0) for l in clause):
            return False, (f"clause {i + 1} "
                           f"({' '.join(map(str, clause))} 0) not satisfied")
    return True, f"{len(clauses)} clauses satisfied"


def check_files(cnf_path, out_path):
    """Returns (ok, message) for a DIMACS file + solver output file."""
    nvars = None
    with open(cnf_path) as f:
        for line in f:
            fields = line.split()
            if len(fields) >= 4 and fields[0] == "p" and fields[1] == "cnf":
                nvars = int(fields[2])
                break
    if nvars is None:
        return False, "missing DIMACS header"

    # File-scale checking can see tens of millions of variables. A Python
    # dict costs several dozen bytes per assignment, and materializing every
    # clause a second time can exhaust the same 32 GB allocation used by the
    # solver. Values are 0=missing, 1=false, 2=true; this remains independent
    # of all solver data structures and preserves parse_model's last-value-wins
    # behavior for repeated model literals.
    assignment = bytearray(nvars + 1)
    assigned = 0
    saw_v = False
    with open(out_path, errors="replace") as f:
        for line in f:
            if line.startswith("v ") or line.rstrip() == "v":
                saw_v = True
                for tok in line.split()[1:]:
                    lit = int(tok)
                    var = abs(lit)
                    if lit == 0 or var > nvars:
                        continue
                    if assignment[var] == 0:
                        assigned += 1
                    assignment[var] = 2 if lit > 0 else 1
    if not saw_v:
        return False, "no v lines in solver output"

    clauses = 0
    cur = []
    satisfied = False
    with open(cnf_path) as f:
        for line in f:
            line = line.strip()
            if not line or line[0] in "cp":
                continue
            for tok in line.split():
                lit = int(tok)
                if lit == 0:
                    clauses += 1
                    if not satisfied:
                        return False, (f"clause {clauses} "
                                       f"({' '.join(map(str, cur))} 0) "
                                       "not satisfied")
                    cur = []
                    satisfied = False
                else:
                    cur.append(lit)
                    var = abs(lit)
                    value = assignment[var] if var <= nvars else 0
                    satisfied |= value == (2 if lit > 0 else 1)
    if cur:
        clauses += 1
        if not satisfied:
            return False, (f"clause {clauses} "
                           f"({' '.join(map(str, cur))} 0) not satisfied")
    return True, f"{clauses} clauses satisfied, {assigned} vars assigned"
