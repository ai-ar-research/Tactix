#!/usr/bin/env python3
"""Focused forced-TTF generator, action, and reach-accounting controls."""

import importlib.util
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/fuzz"))
SPEC = importlib.util.spec_from_file_location(
    "forced_ttf", ROOT / "tools/fuzz/forced_ttf.py")
FORCED = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FORCED)


def main():
    action = (ROOT / "tools/fuzz/controls/forced-ttf/balanced.actions").read_text()
    assert action == FORCED.action_text()
    assert "balanced-signature-model(256,8,240,70000,24," in action
    assert ":instances[10]:" in action
    run_sh = (ROOT / "tools/fuzz/run.sh").read_text()
    slurm_sh = (ROOT / "tools/fuzz/slurm.sh").read_text()
    assert "--forced-ttf) FORCED_TTF=1" in run_sh
    assert 'exec python3 "$HERE/forced_ttf.py"' in run_sh
    assert '"$HERE/forced_ttf.py"' in slurm_sh
    assert '"$HERE/controls/forced-ttf"' in slurm_sh

    signatures, pairs, boundary = FORCED.shape(3, 2)
    assert (signatures, pairs, boundary) == (4, 18, 42)
    assert [FORCED.fixed_pair(3, 2, ball) for ball in range(3)] == [
        (25, 26), (33, 34), (41, 42)]
    nvars, clauses, _, _ = FORCED.base_formula(
        3, 2, ["guarded", "binary", "guarded"])
    assert nvars == 43
    assert [-1] in clauses and [-5] in clauses and [-18] in clauses
    assert [43] in clauses
    assert [-43, 25, 26] in clauses and [33, 34] in clauses
    assert set(FORCED.balanced_signs(3, 2)) == {
        (-1, 1, 1), (1, -1, -1)}

    classes = {}
    for index in range(10):
        name, nvars, clauses, verdict, reached, tight = FORCED.mutate(7, index)
        classes[index] = (name, verdict, reached, tight)
        assert clauses and max(abs(lit) for clause in clauses for lit in clause) <= nvars
    assert classes[0][2] is True
    assert all(classes[index][2] is False for index in (1, 2, 3))
    assert classes[5][1] == "UNSAT"
    assert classes[6][1] == "SAT" and "validation-poison" in classes[6][0]
    assert classes[7][1] == "UNSAT" and "validation-poison" in classes[7][0]
    assert classes[8][3] is True
    _, _, shuffled, _, reached, _ = FORCED.mutate(7, 9)
    assert reached
    assert shuffled[:7] == [[-1], [-5], [-9], [-10], [-14], [-18], [43]]

    exact = """\
c balance    9 calls, 8 recognized, 7 forced TTF, 6 candidates, 5 restarts, 4 steps, 3 completion rounds, 2 SAT, 1 interrupted
"""
    assert FORCED.balance_counters(exact) == {
        "calls": 9, "recognized": 8, "forced": 7, "candidates": 6,
        "restarts": 5, "steps": 4, "completion": 3, "sat": 2,
        "interrupted": 1}
    ambiguous = exact.replace("7 forced TTF", "0 forced TTF")
    counters = FORCED.balance_counters(ambiguous)
    assert counters["recognized"] == 8 and counters["forced"] == 0
    assert FORCED.balance_counters("c balance 8 recognized\n") is None
    print("forced-TTF fuzz controls: PASS")


if __name__ == "__main__":
    main()
