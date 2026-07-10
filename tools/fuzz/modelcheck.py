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
    assignment = parse_model(out_path)
    if assignment is None:
        return False, "no v lines in solver output"
    clauses = []
    cur = []
    with open(cnf_path) as f:
        for line in f:
            line = line.strip()
            if not line or line[0] in "cp":
                continue
            for tok in line.split():
                lit = int(tok)
                if lit == 0:
                    clauses.append(cur)
                    cur = []
                else:
                    cur.append(lit)
    if cur:
        clauses.append(cur)
    ok, msg = check_clauses(clauses, assignment)
    if ok:
        msg += f", {len(assignment)} vars assigned"
    return ok, msg
