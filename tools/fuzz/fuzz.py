#!/usr/bin/env python3
"""fuzz.py — soundness fuzzing runner (plan §8.2; see tools/fuzz/run.sh).

Oracles enforced on every case:
  1. model     — SAT verdicts: `checkmodel` verifies the v-line assignment
  2. proof     — UNSAT verdicts: witness-free DSR converted to DRAT and checked
                 by drat-trim/CakeLPR, every lemma audited by forward drat-trim;
                 witnessed DSR elaborated by no-option dsr-trim and checked by
                 Trestle, every lemma audited by forward dsr-trim
  3. differential — verdict must agree with the oracle solver
  4. crash/io  — clean exit (10/20), s-line consistent with exit code,
                 no sanitizer reports on stderr

Subcommands:
  run        fuzz N cases; write failure bundles; auto-shrink via ddmin
  check-one  re-run the oracles on a single CNF (ddmin predicate / repro)
  corpus     replay corpus/regressions/ — every entry must pass

A leg's strength is the number of cases an oracle actually ran on, which is not
--n. Two readouts on the summary line keep the difference visible:
  coverage:  cases that reached a verdict. A case that times out (or answers
             UNKNOWN) exercises no model/proof/differential oracle at all, so
             it contributes nothing; the non-terminating population is printed
             on its own line, saved under <outdir>/nonterminating/, and can be
             made fatal with --max-nonterminating-frac.
  reached:   per-registry-entry fire counts, read from the solver's own
             counters. A rule with 0 is a rule this leg says nothing about;
             --require-reached makes that fatal.
"""

import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import resource
import shlex
import shutil
import subprocess
import sys
import tempfile
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
# slurm.sh executes this file from a read-only build/pin-fuzz-harness-*/fuzz
# snapshot, where the two-dirname derivation would name <repo>/build instead
# of the repository; the wrappers export TACTIX_REPO_ROOT for that layout,
# and the live tools/fuzz layout still works without it.
ROOT = os.environ.get("TACTIX_REPO_ROOT") or os.path.dirname(
    os.path.dirname(HERE))
sys.path.insert(0, HERE)
from generators import generate, parse_dimacs, write_dimacs  # noqa: E402
from modelcheck import check_files  # noqa: E402
from shrink import minimize  # noqa: E402
# slurm.sh includes this shared parser in its immutable harness snapshot.
# The fallback is only for the maintained tools/fuzz checkout.
sys.path.append(os.path.join(ROOT, "tools", "bench"))
from proof_oracle import (bridge_trivial_input_binary_lsr,  # noqa: E402
                          bridge_trivial_input_lsr, bridge_trivial_input_lrat,
                          certificate_route, dsr_proof_as_drat, drat_stages,
                          restate_duplicate_input_lrat, DSRFormatError,
                          dsr_compress_status,
                          every_lemma_drat_stages, every_lemma_dsr_stages,
                          proof_check_status,
                          reference_dsr_stages, pb_proof_has_header, pb_stages,
                          stage_environment, canonical_checker_cnf,
                          drat_command_with_timeout, run_captured)

# Core clausal checkers.  A frozen collection is a directory holding exactly
# these basenames plus a SHA256SUMS manifest: `slurm.sh --checkers DIR` records
# its content-addressed copy in the harness snapshot (core-checkers and
# core-checkers.sha256), and TACTIX_FUZZ_CHECKERS selects one for a live
# checkout.  Without either, the mutable tools/checkers/bin links are used.
CORE_CHECKERS = ("cake_lpr", "drat-trim", "dsr-compress", "dsr-trim",
                 "lrat-check", "srcheck")


def _checker_collection():
    pinned = os.path.join(HERE, "core-checkers")
    selected = os.environ.get("TACTIX_FUZZ_CHECKERS")
    if os.path.isfile(pinned):
        with open(pinned) as f:
            directory = f.read().strip()
        if selected and os.path.realpath(selected) != os.path.realpath(directory):
            sys.exit(f"error: TACTIX_FUZZ_CHECKERS={selected} conflicts with "
                     f"this snapshot's frozen checkers {directory}")
        return directory, pinned + ".sha256"
    if selected:
        directory = os.path.abspath(selected)
        return directory, os.path.join(directory, "SHA256SUMS")
    return os.path.join(ROOT, "tools", "checkers", "bin"), None


CHECKER_DIR, CHECKER_MANIFEST = _checker_collection()


def verify_checker_collection():
    """Fail unless a frozen collection's binaries match its manifest exactly.

    Returns {basename: sha256}, or None for the unfrozen default links."""
    if CHECKER_MANIFEST is None:
        return None
    want = {}
    with open(CHECKER_MANIFEST) as f:
        for line in f:
            digest, name = line.split()
            name = name.lstrip("*")
            if name in want:
                sys.exit(f"error: duplicate checker {name} in {CHECKER_MANIFEST}")
            want[name] = digest
    if sorted(want) != sorted(CORE_CHECKERS):
        sys.exit(f"error: {CHECKER_MANIFEST} lists {sorted(want)}, "
                 f"expected {sorted(CORE_CHECKERS)}")
    for name, digest in want.items():
        path = os.path.join(CHECKER_DIR, name)
        if os.path.islink(path) or not os.path.isfile(path):
            sys.exit(f"error: frozen checker {path} is not a regular file")
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for block in iter(lambda: f.read(1 << 20), b""):
                h.update(block)
        if h.hexdigest() != digest:
            sys.exit(f"error: frozen checker {path} has sha256 "
                     f"{h.hexdigest()}, manifest says {digest}")
    return want


DRAT_TRIM = os.path.join(CHECKER_DIR, "drat-trim")
DSR_TRIM = os.path.join(CHECKER_DIR, "dsr-trim")
DSR_COMPRESS = os.path.join(CHECKER_DIR, "dsr-compress")
SR_CHECK = os.path.join(CHECKER_DIR, "srcheck")
LRAT_CHECK = os.path.join(CHECKER_DIR, "lrat-check")
CAKE_LPR = os.path.join(CHECKER_DIR, "cake_lpr")
PB_PINS = (os.path.join(HERE, "checkers") if
           os.path.isdir(os.path.join(HERE, "checkers")) else
           os.path.join(ROOT, "benchmarks", "baselines", "sc24-26-32g-5000s", "pins"))
VERIPB = os.path.join(PB_PINS, "veripb")
CAKE_PB = os.path.join(PB_PINS, "cake-pb-cnf")


class CaseOutcome:
    def __init__(self):
        self.verdict = "UNKNOWN"
        self.oracle_verdict = None
        self.failures = []  # list of (kind, detail)
        # Oracles deliberately not applied to this case, with the reason. An
        # uncounted skip is indistinguishable from a pass, so no oracle may
        # drop out silently.
        self.skips = []  # list of (oracle, reason)
        self.solver_out = ""
        self.solver_err = ""
        self.checker_runs = []
        self.proof_artifacts = []

    def fail(self, kind, detail):
        self.failures.append((kind, detail))

    def skip(self, oracle, reason):
        self.skips.append((oracle, reason))


def _cpu_backstop(timeout):
    """preexec_fn: kernel-enforced CPU cap on the child. subprocess.run(timeout=)
    only kills the child on the NORMAL timeout path; if the PARENT dies abruptly
    (session/nightly killed mid-run) the child is reparented to init and spins
    forever — the 13.5 h / 6-core orphan we hit. RLIMIT_CPU makes the kernel
    SIGXCPU (then SIGKILL) the child regardless of parent state, so no solver can
    ever outlive its budget. Slack keeps the wall-timeout the primary mechanism."""
    cap = int(timeout) + 5

    def _pre():
        resource.setrlimit(resource.RLIMIT_CPU, (cap, cap + 2))
    return _pre


def run_cmd(cmd, timeout, env=None):
    """Returns (exitcode, stdout, stderr); exitcode None on timeout."""
    try:
        r = run_captured(cmd, timeout=timeout,
                         preexec_fn=_cpu_backstop(timeout), env=env)
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired as e:
        def _txt(b):
            return b.decode(errors="replace") if isinstance(b, bytes) else (b or "")
        return None, _txt(e.stdout), _txt(e.stderr)


def parse_verdict(exitcode, stdout):
    sline = None
    for line in stdout.splitlines():
        if line.startswith("s "):
            sline = line[2:].strip()
            break
    if exitcode is None:
        return "TIMEOUT", sline
    if exitcode == 10:
        return "SAT" if sline == "SATISFIABLE" else "IO-MISMATCH", sline
    if exitcode == 20:
        return "UNSAT" if sline == "UNSATISFIABLE" else "IO-MISMATCH", sline
    if sline == "UNKNOWN":
        return "UNKNOWN", sline
    return "CRASH", sline


def lrat_verified(code, stdout):
    """Did lrat-check ACCEPT the proof?

    It prints `c VERIFIED` on success and `c NOT VERIFIED` on rejection, so a
    substring test for "VERIF"/"VERIFIED" matches BOTH — which is what the two
    native-LRAT oracles here used to do. Every LRAT rejection in the project's
    fuzz history was therefore recorded as a pass; only crashes and timeouts
    (`code is None`) ever failed a native-LRAT leg. Found 2026-08-05 while
    closing the V5 Sweep N2 coverage gap: a 200-case sample rejected 3 proofs
    that the harness called green. Test for the rejection FIRST.
    (drat-trim's own `s NOT VERIFIED` does not contain `s VERIFIED`, so the
    DRAT branches were never affected.)"""
    if code is None:
        return False
    up = stdout.upper()
    return "NOT VERIFIED" not in up and "VERIFIED" in up


def conversion_crosscheck_unsound(path):
    """Is `drat-trim -L` -> lrat-check a VALID oracle for this CNF?

    Returns a reason string when it is not, else None. Both classes below are
    disagreements BETWEEN THE TWO CHECKERS, not anything a solver does: with
    each of them, KISSAT's and CADICAL's own DRAT proofs are rejected exactly
    as ours are (measured 2026-08-05; three-solver table and a five-clause
    reproducer in
    benchmarks/reports/2026-08-05-drat-trim-conversion-crosscheck.md).
    Before `lrat_verified()` landed the same day, this oracle accepted
    `c NOT VERIFIED`, so the whole class was invisible — including in this
    harness's own selftest, where `expect_clean kissat` passed over two of them.

    1. A REPEATED LITERAL in some clause. drat-trim's parser sorts and DELETES
       duplicate literals as it reads the CNF (drat-trim.c parse(), the
       `buffer[i] == buffer[i+1]` loop), so it holds `80 80` as the unit `80`
       and its own RUP check passes; `-L` then cites input clauses BY ID; and
       lrat-check re-reads the ORIGINAL file without deduping, so that id names
       a clause it cannot propagate — `FAILED: multiple literals unassigned in
       hint 161: 80 80`.
    2. THE EMPTY CLAUSE in the input. drat-trim short-circuits (the formula is
       refuted at parse) and `-L` writes a ZERO-BYTE proof, so lrat-check never
       sees an empty-clause derivation: `Verification incomplete` ->
       `NOT VERIFIED`.

    Tautologies, duplicate clauses and plain UNSAT are all fine — measured, not
    assumed; do not widen this predicate without the same evidence, because
    every widening is an oracle you stop running.

    This gates ONLY the conversion cross-check. The NATIVE-LRAT oracle (solver
    run with --lrat) is deliberately untouched: it checks ids our own emitter
    wrote, it is the oracle that covers the frontend's input restatement, and
    it passes on both classes."""
    dup = False
    empty = False
    clause_literals = set()
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line[0] in "cp%":
                    continue
                # DIMACS clauses end at numeric zero, not at a line boundary.
                # Compare integer values: 81 and 0081 are the same literal.
                for token in line.split():
                    literal = int(token)
                    if literal == 0:
                        empty |= not clause_literals
                        clause_literals.clear()
                    else:
                        dup |= literal in clause_literals
                        clause_literals.add(literal)
    except OSError:
        return None
    if dup and empty:
        return "repeated input literal + empty input clause"
    if dup:
        return "drat-trim -L dedupes the CNF, lrat-check does not"
    if empty:
        return "empty input clause: drat-trim -L emits a zero-byte proof"
    return None


_RESTATED_RE = re.compile(r"^c input\s+(\d+) clauses restated", re.M)


def input_restated(stdout):
    """How many input clauses the solver had to restate after normalization.

    Reads the solver's own `c input` counter, so this measures the path at its
    SOURCE rather than re-deriving it from the CNF. 0 when the leg runs the
    solver with -q (no stats printed) — the standard legs do not."""
    m = _RESTATED_RE.search(stdout or "")
    return int(m.group(1)) if m else 0


_STRESS_FIRES_RE = re.compile(r"^c stress-target-fires\s+(\d+)", re.M)


def stress_target_fires(stdout):
    """Return the driver's direct count of preferred-rule applications."""
    return sum(int(m.group(1)) for m in
               _STRESS_FIRES_RE.finditer(stdout or ""))


_DENSE_RE = re.compile(r"^c stress-dense\s+(\d+)", re.M)


def stress_dense_connected(stdout):
    """Did this case walk with the dense occurrence index attached?

    random_solve derives that from the POLICY seed's low bit, so a leg that
    passes one fixed seed reaches only one half of the schedule space and its
    dense-gated tails (BveBackward, VariableCompaction) read as never-fired.
    Counted at the source, like every other reach number here."""
    m = _DENSE_RE.search(stdout or "")
    return m is not None and m.group(1) != "0"


# --- per-rule reachability, counted at the source -------------------------
# One row per registry entry (rule or policy op): the label, and a regex whose
# group(1) is a counter that is non-zero iff that entry's apply actually ran on
# the case. A leg that never fires a rule proves nothing about it, and only the
# solver's own counters can say.
#
# The regexes match the solver's stats block, so a reworded print line would
# silently zero a row. `spec_miss` counts rows whose line never appeared in any
# case that printed stats, and a leg with a spec miss fails.
#
# Never key a `dosed` entry on its `rounds` counter. `<rule>_rounds` moves on
# the canonical apply only, and DefaultPolicy drives every `EFFORT_SPECS` entry
# with `dosed=true` one instance at a time, so the round counter stays 0 on a
# run where the rule did all its work (`c bve 0 rounds, 11008 eliminated`).
# Those entries key on a work counter (resolutions / probed / checked / probes)
# instead — non-zero iff the apply body ran, which is what this table is for.
REACH_SPECS = [
    ("Propagate", r"^c propagations\s+(\d+)"),
    ("Decide", r"^c decisions\s+(\d+)"),
    # `c conflicts` is incremented by the PROPAGATE rule (propagate.cpp), not
    # by ConflictAnalysis, so keying on it made --require-reached
    # ConflictAnalysis satisfiable by Propagate alone. `c learned` is the
    # clause this rule actually produces.
    ("ConflictAnalysis", r"^c learned\s+(\d+)"),
    ("Restart", r"^c restarts\s+(\d+)"),
    ("Reduce", r"^c reductions\s+(\d+)"),
    # `c lucky` prints early/late/SAT before the probe count; keying on the
    # first field would miss a late-only Lucky, and the old pattern matched no
    # line at all, so this row read 0 in every leg ever run.
    ("Lucky", r"^c lucky\s+\d+ early, \d+ late \(\d+ SAT\), (\d+) probes"),
    # S5 instance 6 is the structural affine candidate path. Its own call
    # counter distinguishes it from the six reference Lucky probes.
    ("ParityModel", r"^c parity\s+(\d+) calls,"),
    ("ParityConsequence", r"^c theory-act\s+(\d+) parity,"),
    ("AffineConsequence", r"^c theory-act\s+\d+ parity, (\d+) affine,"),
    ("ExactCoverExtension",
     r"^c theory-act\s+\d+ parity, \d+ affine, (\d+) exact-cover,"),
    ("IndependentSetCover",
     r"^c theory-act\s+\d+ parity, \d+ affine, \d+ exact-cover, (\d+) independent-set,"),
    ("SemanticVariableAdvice",
     r"^c theory-act\s+\d+ parity, \d+ affine, \d+ exact-cover, \d+ independent-set, (\d+) advice attempts"),
    ("Vivify", r"^c vivify\s+(\d+) rounds,"),
    # These key on attempts, not rounds: they are `dosed` entries reached
    # through apply_instance, whose round counter never moves. See the note
    # above the table.
    ("Probe", r"^c probe\s+\d+ rounds, (\d+) probed,"),
    ("Backbone", r"^c backbone\s+\d+ rounds, (\d+) probed,"),
    ("TransitiveReduce", r"^c transitive\s+\d+ rounds, (\d+) checked,"),
    ("HyperBinaryRes", r"^c hbr\s+\d+ rounds, (\d+) probes,"),
    ("Substitute", r"^c substitute\s+(\d+) rounds,"),
    ("SubstitutionRedundancy",
     r"^c sr\s+\d+ applications, (\d+) applied,"),
    ("BCE", r"^c bce\s+(\d+) rounds,"),
    ("CCE", r"^c cce\s+(\d+) rounds,"),
    ("Instantiate", r"^c instantiate\s+(\d+) rounds,"),
    ("Sweep", r"^c sweep\s+(\d+) rounds,"),
    ("SweepResume",
     r"^c sweepsched\s+\d+ scheduled, \d+ rescheduled, (\d+) resumed,"),
    ("SweepPartition", r"^c sweepflip\s+\d+ attempts, (\d+) successful"),
    ("HyperTernaryRes", r"^c ternary\s+(\d+) rounds,"),
    ("Congruence", r"^c congruence\s+(\d+) rounds,"),
    ("Factor", r"^c factor\s+(\d+) factorings,"),
    ("BVE", r"^c bve\s+\d+ rounds, \d+ eliminated, (\d+) resolutions"),
    ("SubsumeForward", r"^c subsume\s+(\d+) rounds \("),
    ("Strengthen", r"^c strengthen\s+(\d+) rounds \("),
    ("Walk", r"^c walk\s+(\d+) walks"),
    ("LocalSearchGuidance",
     r"^c localsearch\s+\d+ hints offered, (\d+) applied"),
    # Known confound: `c rephased` is also incremented by Walk, so this row
    # alone cannot prove Rephase fired. It is kept because separating them
    # needs a new counter, and the separation was measured by hand: rephased
    # runs 3-5x walks on 23 of 24 Tier B instances, so Rephase does fire
    # independently. Treat a lone `--require-reached Rephase` as weak.
    ("Rephase", r"^c rephased\s+(\d+)"),
    ("Reorder", r"^c reordered\s+\d+ eligible, (\d+) decision"),
    ("SwitchMode", r"^c mode\s+\S+ after (\d+) switches"),
    # PATH rows, not registry rules (the conformance check only requires the
    # registry->row direction). Added after the 2026-08-08 wrong-SAT: the
    # backward pass's strengthen-to-unit branch dropped its unit while every
    # leg was green, because nothing counted the branch at the source — a
    # green leg proves nothing about a path no counter shows was reached.
    ("BveBackward", r"^c bve-bwd\s+(\d+) subsumers"),
    ("BveBackwardUnit",
     r"^c bve-bwd\s+\d+ subsumers, \d+ subsumed, \d+ strengthened, (\d+) units"),
    ("VariableCompaction", r"^c compact\s+(\d+) passes"),
    ("VivifyFlush",
     r"^c vivify\s+\d+ rounds, \d+ vivified, \d+ deleted, (\d+) flushed"),
    # PATH row: a strengthened (vivified) clause is exactly what arms
    # VivifyArenaOrder (retire/inherit -> changed) and therefore forces the
    # round-end ordered arena collection through the kernel's logical
    # clause-order sort. `rounds` alone cannot witness that path: a round that
    # strengthens nothing skips the collection.
    ("VivifyChanged", r"^c vivify\s+\d+ rounds, (\d+) vivified"),
    # PATH row: a committed elimination is exactly what consumes BVE's
    # materialized resolvent literal/LRAT-chain images into kernel clauses and
    # proof additions. `resolutions` alone cannot witness that path: a
    # candidate can generate resolvents and then abort at its bound without
    # ever committing one.
    ("BveEliminated", r"^c bve\s+\d+ rounds, (\d+) eliminated"),
]
_REACH_RES = [(label, re.compile(pat, re.M)) for label, pat in REACH_SPECS]
_STATS_BANNER_RE = re.compile(r"^c tactix ", re.M)


# Restart-controller coverage (docs/controller-interface.md): the policy prints
# one line per root family, one per named search configuration and one loop
# summary at every terminal, so requests, applications, effective changes,
# stop outcomes and search intervals are read at the source.
_CONTROLLER_LINE_RE = re.compile(
    r"^c controller-(?:(rule|search) (\S+)|(loop)) ((?:\S+=\d+ ?)+)$", re.M)


def controller_coverage(stdout):
    """{'rule': {name: fields}, 'search': {name: fields}, 'loop': fields}, or
    None when the case printed no controller coverage."""
    found = None
    for m in _CONTROLLER_LINE_RE.finditer(stdout or ""):
        found = found or {"rule": {}, "search": {}, "loop": {}}
        fields = {key: int(value) for key, value in
                  (item.split("=") for item in m.group(4).split())}
        if m.group(3):
            found["loop"] = fields
        else:
            found[m.group(1)][m.group(2)] = fields
    return found


def merge_controller_coverage(total, case, verdict):
    """Accumulate field sums and the number of cases with a non-zero field."""
    total["cases"] += 1
    total["verdicts"][verdict] = total["verdicts"].get(verdict, 0) + 1
    for kind in ("rule", "search"):
        for name, fields in case[kind].items():
            entry = total[kind].setdefault(name, {"sum": {}, "cases": {}})
            for key, value in fields.items():
                entry["sum"][key] = entry["sum"].get(key, 0) + value
                entry["cases"][key] = entry["cases"].get(key, 0) + (value > 0)
    loop = total["loop"]
    for key, value in case["loop"].items():
        loop["sum"][key] = loop["sum"].get(key, 0) + value
        loop["cases"][key] = loop["cases"].get(key, 0) + (value > 0)
        loop["max"][key] = max(loop["max"].get(key, 0), value)
    loop["cases_root_sequence_ge2"] += (
        case["loop"].get("maximum-root-actions", 0) >= 2)


def missing_controller_requirements(coverage, applications=(), effective=()):
    """Return requested controller rows whose aggregate source counter is zero.

    The controller prints all root families at every terminal, including
    structured families that were unavailable because no typed witness was
    offered.  Merely seeing a row therefore proves nothing.  Keep application
    and effective-transition requirements distinct: an attempted semantic
    action can consume one application and still publish no constraint.

    ``coverage`` may be one case from :func:`controller_coverage` or the
    aggregate written to controller-coverage.json; both use the same nested
    ``rule`` field and differ only in whether counters live directly in the
    row or under ``sum``.
    """
    missing = []
    rules = coverage.get("rule", {}) if coverage else {}
    for field, names in (("applications", applications),
                         ("effective", effective)):
        for name in names:
            row = rules.get(name, {})
            values = row.get("sum", row)
            if values.get(field, 0) == 0:
                missing.append(f"{name}:{field}")
    return missing


def rule_reachability(stdout):
    """(stats_present, {label: count}, {label: line_present}) for one case.

    `count` is the entry's fired-counter value; `line_present` says the print
    line existed at all, which separates "the rule did not fire" from "the
    harness is looking for a line the solver no longer prints"."""
    text = stdout or ""
    present = bool(_STATS_BANNER_RE.search(text))
    counts, seen = {}, {}
    if not present:
        return False, counts, seen
    for label, rx in _REACH_RES:
        m = rx.search(text)
        seen[label] = m is not None
        counts[label] = int(m.group(1)) if m else 0
    return True, counts, seen


# A leg's solver command may carry `{seed}` where a policy seed belongs
# (`--random-policy={seed}`, `--stress-policy={seed}`). `run` fills in each
# case's own seed; the single-CNF entry points below pin CORPUS_SEED so a
# replay is reproducible. Odd, because random_solve connects the dense
# occurrence index on odd seeds and the dense-gated rules are the ones a
# corpus entry is most likely to be protecting.
CORPUS_SEED = 1


def substitute_seed(cmd, seed):
    return [a.replace("{seed}", str(seed)) for a in cmd]


def substitute_workdir(cmd, workdir):
    """Bind per-run outputs without baking a vanished temp dir into repros."""
    return [a.replace("{workdir}", workdir) for a in cmd]


def run_case(cnf_path, solver, oracle, timeout, lrat, workdir):
    """Run the solver under test on cnf_path and apply all oracles.

    The default mode is startup PBP or native DSR. Explicit --drat or --lrat selects
    the corresponding compatibility oracle. Native LRAT is checked directly
    with cake_lpr; legacy lrat-check is not an acceptance oracle because it
    accepts a non-conflicting empty-lemma chain on a SAT input."""
    solver = substitute_workdir(solver, workdir)
    out = CaseOutcome()
    def check(command):
        command = list(map(str, command))
        checker = os.path.basename(command[0])
        if checker == 'drat-trim':
            command = drat_command_with_timeout(command, 120)
        try:
            c, o, e = (run_cmd(command, 120, env=stage_environment(checker))
                       if checker in ("cake-pb-cnf", "cake_lpr")
                       else run_cmd(command, 120))
        except OSError as error:
            c, o, e = -1, "", str(error)
        status = (dsr_compress_status(c, o + "\n" + e)
                  if checker == "dsr-compress"
                  else proof_check_status(checker, c, o, e))
        # The pinned drat-trim exits 1 on an input empty clause even while
        # reporting success. Confirm that semantic condition from the actual
        # original CNF; never accept an abnormal exit from success text alone.
        if (command[0] == DRAT_TRIM and c == 1 and
                "c trivial UNSAT" in o.splitlines() and
                "s VERIFIED" in o.splitlines()):
            _, clauses = parse_dimacs(cnf_path)
            status = "verified" if [] in clauses else "rejected"
        out.checker_runs.append(dict(command=command, exit_code=c, stdout=o,
                                     stderr=e, status=status))
        return status == "verified"
    native_lrat = "--lrat" in solver
    explicit_dsr = "--dsr" in solver
    is_tactix = os.path.basename(solver[0]) == "tactix" or "--auto-proof" in solver
    # Competition reference solvers still default to DRAT. Tactix's default is
    # native DSR; --drat keeps a same-source compatibility/control lane.
    explicit_drat = ("--drat" in solver or
                     (not native_lrat and not explicit_dsr and not is_tactix))
    native_dsr = not native_lrat and not explicit_drat
    proof = os.path.join(
        workdir, "proof.lrat" if native_lrat else
        ("proof.drat" if explicit_drat else "proof.dsr"))
    out.proof_artifacts.append(proof)
    if os.path.exists(proof):
        os.unlink(proof)

    code, stdout, stderr = run_cmd(solver + [cnf_path, proof], timeout)
    out.solver_out, out.solver_err = stdout, stderr
    verdict, sline = parse_verdict(code, stdout)
    out.verdict = verdict

    if "Sanitizer" in stderr or "runtime error" in stderr:
        out.fail("crash", "sanitizer report on stderr")
    # A policy that stops on an internal error prints `c ERROR:` and exits
    # with UNKNOWN; that is a failed run, not a non-terminating case.
    error_line = re.search(r"^c ERROR:.*$", stderr or "", re.M)
    if error_line:
        out.fail("crash", "solver error: " + error_line.group(0)[:200])
    if verdict == "CRASH":
        out.fail("crash", f"exit code {code}, s-line {sline!r}")
    elif verdict == "IO-MISMATCH":
        out.fail("io", f"exit code {code} inconsistent with s-line {sline!r}")

    if verdict == "SAT":
        out_file = os.path.join(workdir, "solver.out")
        with open(out_file, "w") as f:
            f.write(stdout)
        ok, msg = check_files(cnf_path, out_file)
        if not ok:
            out.fail("model", msg)

    if verdict == "UNSAT":
        checker_cnf = os.path.join(workdir, "checker.cnf")
        out.proof_artifacts.append(checker_cnf)
        try:
            checker_spelling = canonical_checker_cnf(cnf_path, checker_cnf)
        except (OSError, ValueError) as error:
            out.fail("proof", "invalid original DIMACS: " + str(error))
            return out
        if not os.path.exists(proof) or os.path.getsize(proof) == 0:
            # empty proof file is legal iff the input itself contains the
            # empty clause; drat-trim handles that, so only flag absence
            if not os.path.exists(proof):
                out.fail("proof", "no proof file produced")
        if os.path.exists(proof) and pb_proof_has_header(proof):
            if native_lrat or explicit_drat or explicit_dsr or "--no-binary" in solver:
                out.fail("proof", "PBP emitted in an explicit clausal format mode")
            else:
                kernel = os.path.join(workdir, "proof.kernel.pbp")
                out.proof_artifacts.append(kernel)
                if os.path.exists(kernel):
                    os.unlink(kernel)
                for name, command in pb_stages(
                        VERIPB, CAKE_PB, cnf_path, proof, kernel,
                        checker_cnf=checker_cnf):
                    if not check(command):
                        out.fail("proof", name + " did not verify startup PBP")
                        break
                    if name == "veripb" and (not os.path.isfile(kernel) or
                                             os.path.getsize(kernel) == 0):
                        out.fail("proof", "VeriPB produced no kernel proof")
                        break
        elif os.path.exists(proof) and native_lrat:
            if not check([CAKE_LPR, checker_cnf, proof]):
                out.fail("proof", "cake_lpr did not verify the native LRAT")
        elif os.path.exists(proof) and explicit_drat:
            if not check([DRAT_TRIM, checker_cnf, proof]):
                out.fail("proof", "drat-trim did not verify")
            elif lrat:
                # On some degenerate-but-legal CNFs drat-trim and lrat-check
                # disagree about the file itself, so the conversion cross-check
                # is not an oracle for any solver. See
                # conversion_crosscheck_unsound.
                why = conversion_crosscheck_unsound(checker_cnf)
                if why:
                    out.skip("lrat-convert", why)
                else:
                    lrat_file = os.path.join(workdir, "proof.lrat")
                    out.proof_artifacts.append(lrat_file)
                    if not check([DRAT_TRIM, checker_cnf, proof, "-L", lrat_file]):
                        out.fail("proof", "drat-trim -L (LRAT emission) failed")
                    else:
                        if not check([CAKE_LPR, checker_cnf, lrat_file]):
                            out.fail("proof", "cake_lpr did not verify the converted LRAT")
        elif os.path.exists(proof) and native_dsr:
            try:
                route = certificate_route(proof)
            except DSRFormatError as error:
                out.fail('proof', 'invalid DSR stream: ' + str(error))
                route = None
            if route == 'pbp':
                out.fail('proof', 'unexpected PBP in native DSR branch')
            if route == 'drat':
                converted = os.path.join(workdir, 'proof.as-drat')
                lrat_file = os.path.join(workdir, 'proof.lrat')
                lifted = os.path.join(workdir, 'proof.original-input.lrat')
                out.proof_artifacts += [converted, lrat_file, lifted]
                try:
                    converted_ok = dsr_proof_as_drat(proof, converted)
                except DSRFormatError as error:
                    converted_ok = False
                    out.fail('proof', 'invalid DSR conversion: ' + str(error))
                if not converted_ok:
                    out.fail('proof', 'witnessed DSR cannot use DRAT route')
                else:
                    if not check([DRAT_TRIM, checker_cnf, converted, '-L', lrat_file]):
                        out.fail('proof', 'drat-trim did not elaborate witness-free DSR')
                    else:
                        record = out.checker_runs[-1]
                        bridge_trivial_input_lrat(
                            checker_spelling, lrat_file, record['exit_code'],
                            record['stdout'], record['stderr'])
                        if not os.path.isfile(lrat_file) or not os.path.getsize(lrat_file):
                            out.fail('proof', 'drat-trim produced no LRAT')
                        else:
                            try:
                                checked_lrat, _ = restate_duplicate_input_lrat(
                                    checker_cnf, checker_spelling, lrat_file, lifted)
                            except ValueError as error:
                                out.fail('proof', 'input restatement: ' + str(error))
                            else:
                                if not check([CAKE_LPR, checker_cnf, checked_lrat]):
                                    out.fail('proof', 'CakeLPR rejected witness-free DSR')
                    # Backward elaboration above examines only the lemmas the
                    # refutation uses; forward drat-trim then checks every line
                    # of the lossless conversion, unused additions included.
                    for name, command in every_lemma_drat_stages(
                            DRAT_TRIM, cnf_path, converted,
                            checker_cnf=checker_cnf):
                        if not check(command):
                            out.fail('proof', 'every-lemma audit: forward '
                                     'drat-trim rejected witness-free DSR')
                            break
            # proof_oracle.reference_dsr_stages owns the pipeline definition.
            binary_lsr = os.path.join(workdir, "proof.binary.lsr")
            text_lsr = os.path.join(workdir, "proof.lsr")
            out.proof_artifacts += [binary_lsr, text_lsr]
            why = {"dsr-trim": "dsr-trim did not verify and elaborate DSR",
                   "dsr-compress": "dsr-compress did not expand the LSR",
                   "srcheck": "Trestle did not verify elaborated LSR"}
            # The reference pipeline is the authority for witnessed DSR; it
            # elaborates backward and so examines only the lemmas the
            # refutation uses.  Fuzz proofs are small, so every line is then
            # checked by forward elaboration.  Witness-free DSR takes the
            # DRAT owner's route instead and was audited forward above by
            # drat-trim on its lossless conversion (selftest controls cover
            # both every-lemma routes; see controls/every_lemma_controls.py).
            audited_stages = ((("", reference_dsr_stages),
                               ("every-lemma audit: ", every_lemma_dsr_stages))
                              if route == 'dsr' else ())
            for label, stages in audited_stages:
                failed = False
                for name, command in stages(
                        DSR_TRIM, DSR_COMPRESS, SR_CHECK, cnf_path, proof,
                        binary_lsr, text_lsr, checker_cnf=checker_cnf):
                    if not check(command):
                        out.fail("proof", label + why[name])
                        failed = True
                        break
                    made = command[-1] if name != "srcheck" else None
                    if made and (not os.path.isfile(made) or
                                 os.path.getsize(made) == 0):
                        out.fail("proof", label + name + " produced no LSR")
                        failed = True
                        break
                    if name == "dsr-trim":
                        (bridge_trivial_input_binary_lsr if made == binary_lsr
                         else bridge_trivial_input_lsr)(cnf_path, made)
                if failed:
                    break

    if oracle and verdict in ("SAT", "UNSAT"):
        code_o, stdout_o, _ = run_cmd(oracle + [cnf_path], timeout)
        overdict, _ = parse_verdict(code_o, stdout_o)
        out.oracle_verdict = overdict
        if overdict in ("SAT", "UNSAT") and overdict != verdict:
            out.fail("differential",
                     f"solver says {verdict}, oracle says {overdict}")
    return out


def save_bundle(outdir, idx, seed, name, cnf_path, outcome, solver, oracle,
                timeout, lrat, shrink, repro_args=()):
    kinds = "+".join(sorted({k for k, _ in outcome.failures}))
    bundle = os.path.join(outdir, f"{idx:06d}-{kinds}-seed{seed}")
    os.makedirs(bundle, exist_ok=True)
    shutil.copy(cnf_path, os.path.join(bundle, "case.cnf"))
    with open(os.path.join(bundle, "solver.out"), "w") as f:
        f.write(outcome.solver_out)
    with open(os.path.join(bundle, "solver.err"), "w") as f:
        f.write(outcome.solver_err)
    meta = {
        "seed": seed,
        "index": idx,
        "generator": name,
        "solver": solver,
        "oracle": oracle,
        "verdict": outcome.verdict,
        "oracle_verdict": outcome.oracle_verdict,
        "failures": outcome.failures,
        "timeout": timeout,
        "checker_runs": outcome.checker_runs,
    }
    for artifact in outcome.proof_artifacts:
        if os.path.isfile(artifact):
            shutil.copy2(artifact, os.path.join(bundle, os.path.basename(artifact)))
    with open(os.path.join(bundle, "meta.json"), "w") as f:
        json.dump(meta, f, indent=2)
    fail_kind = outcome.failures[0][0]
    check_cmd = [os.path.join(HERE, "fuzz.py"), "check-one",
                 "--solver", shlex.join(solver),
                 "--fail-kind", fail_kind,
                 "--timeout", str(timeout)]
    # the oracle run doubles predicate cost; only shrink with it when the
    # failure being preserved is the differential one
    if oracle and fail_kind == "differential":
        check_cmd += ["--oracle", shlex.join(oracle)]
    if lrat:
        check_cmd += ["--lrat"]
    check_cmd += list(repro_args)
    with open(os.path.join(bundle, "repro.sh"), "w") as f:
        f.write("#!/usr/bin/env bash\n# exit 0 iff the recorded failure "
                "kind reproduces on $1 (default: case.cnf)\n"
                f"cd \"$(dirname \"$0\")\"\n"
                f"{shlex.join(check_cmd)} \"${{1:-case.cnf}}\"\n")
    os.chmod(os.path.join(bundle, "repro.sh"), 0o755)
    if shrink:
        # in-process shrinking: one solver run per candidate, no
        # interpreter startups (see shrink.py)
        shrink_oracle = oracle if fail_kind == "differential" else None
        with tempfile.TemporaryDirectory(prefix="fuzz-shrink-") as sdir:
            def pred(nv, cls):
                cand = os.path.join(sdir, "cand.cnf")
                write_dimacs(cand, nv, cls)
                o = run_case(cand, solver, shrink_oracle, timeout, False,
                             sdir)
                return fail_kind in {k for k, _ in o.failures}

            nvars, clauses = parse_dimacs(cnf_path)
            before = len(clauses)
            nv, cls, evals = minimize(pred, nvars, clauses, max_seconds=120)
            write_dimacs(os.path.join(bundle, "min.cnf"), nv, cls)
        with open(os.path.join(bundle, "ddmin.log"), "w") as f:
            f.write(f"shrunk {before} -> {len(cls)} clauses "
                    f"({evals} evals, kind={fail_kind})\n")
    return bundle


def save_nonterminating(outdir, idx, seed, name, cnf_path, outcome, solver,
                        timeout):
    """Keep a non-terminating case so the loss is DIAGNOSABLE, not just counted.

    Not a failure bundle: nothing is wrong with the verdict, the case simply
    never produced one, so no oracle ran on it. The CNF plus `repro.sh` is what
    turns "a double-digit percentage of the leg was lost" into a livelock to
    shrink (task #39 / RV-S31), and it is also how a longer-timeout re-run
    separates merely slow from non-terminating."""
    d = os.path.join(outdir, "nonterminating", f"{idx:06d}-seed{seed}")
    os.makedirs(d, exist_ok=True)
    shutil.copy(cnf_path, os.path.join(d, "case.cnf"))
    meta = {"seed": seed, "index": idx, "generator": name,
            "solver": solver, "verdict": outcome.verdict,
            "timeout": timeout}
    harness_sha = os.environ.get("TACTIX_FUZZ_HARNESS_SHA256")
    if harness_sha:
        meta["harness_sha256"] = harness_sha
    with open(os.path.join(d, "meta.json"), "w") as f:
        json.dump(meta, f, indent=2)
    # Successful SLURM legs remove their transient submission snapshot.  The
    # submitter installs the same content-addressed harness under failures/ so
    # retained timeout controls never point at a deleted build/pin directory.
    replay_harness = os.environ.get("TACTIX_FUZZ_REPLAY_HARNESS", HERE)
    cmd = [os.path.join(replay_harness, "fuzz.py"), "check-one",
           "--solver", shlex.join(solver)]
    with open(os.path.join(d, "repro.sh"), "w") as f:
        # cd to the repo root, not to the bundle: a repo-relative --solver path
        # (`build/…/tactix`, which every leg uses) does not resolve anywhere
        # else, and the CNF is passed absolute so the cd cannot break it either.
        f.write("#!/usr/bin/env bash\n# re-run this case; pass a timeout in "
                "seconds as $1 to test whether it EVER terminates\n"
                f'cd {shlex.quote(ROOT)}\n'
                f"{shlex.join(cmd)}"
                ' --timeout "${1:-' f"{timeout}" '}" '
                f"{shlex.quote(os.path.abspath(os.path.join(d, 'case.cnf')))}\n")
    os.chmod(os.path.join(d, "repro.sh"), 0o755)
    return d


def cmd_run(args):
    solver = shlex.split(args.solver)
    oracle = shlex.split(args.oracle) if args.oracle else None
    # A misspelled --require-reached label would be a requirement that can
    # never fire and can never fail: reject it here rather than report green.
    known = {label for label, _ in REACH_SPECS}
    bad = [x for x in (args.require_reached or []) if x not in known]
    if bad:
        print(f"fuzz: unknown --require-reached entries {bad}; "
              f"known: {sorted(known)}")
        return 2
    outdir = args.outdir
    os.makedirs(outdir, exist_ok=True)
    nfail = 0
    kinds_count = {}
    verdicts = {}
    # Reachability, counted at the source: a green leg proves nothing about a
    # path no case reached. The solver prints `c input <n> clauses restated`
    # for the frontend's input-normalization path, so tally how many cases had
    # a normalized input clause and report it next to the pass/fail counts. Add
    # a line here for any other path whose coverage a leg must demonstrate.
    reach = {"restated_cases": 0, "restated_unsat": 0, "dense_cases": 0}
    # per-rule reachability (the same principle, generalised): cases in which
    # each registry entry actually fired, plus the spec-miss guard.
    reach_rules = {label: 0 for label, _ in REACH_SPECS}
    reach_spec_seen = {label: 0 for label, _ in REACH_SPECS}
    stats_cases = 0
    stress_fires = 0
    controller = {"cases": 0, "verdicts": {}, "rule": {}, "search": {},
                  "loop": {"sum": {}, "cases": {}, "max": {},
                           "cases_root_sequence_ge2": 0}}
    # Non-termination is lost coverage and must be as visible as a failure: a
    # case that reaches no verdict exercises no semantic oracle (model, proof
    # and differential all need SAT or UNSAT), so "N cases, 0 failures" really
    # means "N minus the non-terminating population". Weak-solver legs
    # (--random-policy above all) lose a double-digit percentage this way.
    nonterm = []  # [{index, seed, generator, nvars, nclauses, verdict}]
    saved_nonterm = [0]  # CNFs written to outdir/nonterminating/ (capped)
    skips_count = {}  # oracle -> cases where it was deliberately not applied
    done = 0
    lock = threading.Lock()  # guards the counters + stdout
    stop = threading.Event()  # set once max_failures is reached

    def one_case(i):
        # case i is fully determined by (seed, i) regardless of which
        # worker runs it or in what order — results are the same at any
        # --jobs level, only wall-clock changes (the work is subprocess-
        # bound, so threads parallelize it fine)
        nonlocal nfail, done, stats_cases, stress_fires
        if stop.is_set():
            return
        seed = args.seed + i
        name, nvars, clauses = generate(seed, i)
        # size cap (weak-solver legs, e.g. --random-policy: a random
        # schedule cannot finish big instances within the timeout, and
        # the semantic oracles only fire on a verdict): deterministically
        # re-roll oversized cases. The fixed edge degeneracies (name prefix
        # "edge-", e.g. edge-max-var-index = 500000 vars) are exempt so the
        # "all 10 edge cases run every batch" guarantee holds — they resolve
        # instantly via their unit clauses, so they're harmless even oversized.
        attempt = 0
        while (args.max_vars and nvars > args.max_vars
               and not name.startswith("edge-") and attempt < 1000):
            attempt += 1
            name, nvars, clauses = generate(seed + 7919 * attempt, i)
        # `{seed}` in the solver command becomes this case's seed, so a policy
        # whose schedule is seed-derived (--random-policy / --stress-policy)
        # varies with the case instead of replaying one walk per leg. The
        # substituted command is what the bundle records, so a failure still
        # reproduces exactly.
        case_solver = substitute_seed(solver, seed)
        with tempfile.TemporaryDirectory(prefix="fuzz-case-") as workdir:
            cnf = os.path.join(workdir, "case.cnf")
            write_dimacs(cnf, nvars, clauses)
            outcome = run_case(cnf, case_solver, oracle, args.timeout,
                               args.lrat, workdir)
            bundle = None
            if outcome.failures:
                # bundles (incl. shrinking) happen in the worker while the
                # case dir is alive; bundle names carry the case index, so
                # completion order does not matter
                bundle = save_bundle(outdir, i, seed, name, cnf, outcome,
                                     case_solver, oracle, args.timeout,
                                     args.lrat, not args.no_shrink)
            if outcome.verdict in ("TIMEOUT", "UNKNOWN"):
                with lock:
                    take = saved_nonterm[0] < args.max_nonterminating_saved
                    if take:
                        saved_nonterm[0] += 1
                if take:
                    save_nonterminating(outdir, i, seed, name, cnf, outcome,
                                        case_solver, args.timeout)
        restated = input_restated(outcome.solver_out)
        dense = stress_dense_connected(outcome.solver_out)
        target_fires = stress_target_fires(outcome.solver_out)
        stats_present, rcounts, rseen = rule_reachability(outcome.solver_out)
        controller_case = controller_coverage(outcome.solver_out)
        with lock:
            if controller_case:
                merge_controller_coverage(controller, controller_case,
                                          outcome.verdict)
            verdicts[outcome.verdict] = verdicts.get(outcome.verdict, 0) + 1
            if outcome.verdict in ("TIMEOUT", "UNKNOWN"):
                nonterm.append({"index": i, "seed": seed, "generator": name,
                                "nvars": nvars, "nclauses": len(clauses),
                                "verdict": outcome.verdict,
                                "timeout": args.timeout})
            if restated:
                reach["restated_cases"] += 1
                if outcome.verdict == "UNSAT":
                    reach["restated_unsat"] += 1
            if dense:
                reach["dense_cases"] += 1
            stress_fires += target_fires
            if stats_present:
                stats_cases += 1
                for label in reach_rules:
                    if rcounts.get(label, 0) > 0:
                        reach_rules[label] += 1
                    if rseen.get(label):
                        reach_spec_seen[label] += 1
            for o, _ in outcome.skips:
                skips_count[o] = skips_count.get(o, 0) + 1
            if outcome.failures:
                nfail += 1
                for k, _ in outcome.failures:
                    kinds_count[k] = kinds_count.get(k, 0) + 1
                print(f"FAIL case {i} seed {seed} ({name}): "
                      f"{outcome.failures} -> {bundle}", flush=True)
                if nfail >= args.max_failures and not stop.is_set():
                    print(f"stopping after {nfail} failures", flush=True)
                    stop.set()
            done += 1
            if done % 100 == 0:
                print(f"... {done}/{args.n} cases, {nfail} failures",
                      flush=True)

    jobs = max(1, args.jobs)
    if jobs == 1:
        for i in range(args.n):
            if stop.is_set():
                break
            one_case(i)
    else:
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
            list(ex.map(one_case, range(args.n)))
    # verdict distribution = oracle coverage: only SAT/UNSAT cases exercise
    # the differential/model/proof oracles — a TIMEOUT is sanitizer-only
    vsummary = " ".join(f"{k}={verdicts[k]}" for k in sorted(verdicts))
    ran = done  # cases actually executed (a --max-failures stop cuts the leg)
    nnon = len(nonterm)
    effective = ran - nnon
    print(f"fuzz: {args.n} cases, {nfail} failures"
          + (f" {kinds_count}" if kinds_count else "")
          + f" | verdicts: {vsummary}"
          + f" | coverage: {effective}/{ran} cases reached a verdict"
          + f" | reached: input-restated {reach['restated_cases']} cases"
            f" ({reach['restated_unsat']} UNSAT)"
          + f", dense-connected {reach['dense_cases']} cases"
          + (f" | oracles skipped: {skips_count}" if skips_count else ""))
    if nnon:
        pct = 100.0 * nnon / ran if ran else 0.0
        # Spelled out because the headline gets quoted without it: these cases
        # ran no semantic oracle, so the leg's strength is `effective`, not
        # `args.n`.
        print(f"fuzz: NON-TERMINATING {nnon} of {ran} cases ({pct:.1f}%) at "
              f"--timeout {args.timeout}s: no model/proof/differential oracle "
              f"ran on them, so this leg is {effective} cases, not {ran}. "
              f"{saved_nonterm[0]} kept under "
              f"{os.path.join(outdir, 'nonterminating')} "
              f"(seeds in nonterminating.json).")
        with open(os.path.join(outdir, "nonterminating.json"), "w") as f:
            json.dump({"solver": solver, "timeout": args.timeout,
                       "cases_run": ran, "nonterminating": nnon,
                       "detail": sorted(nonterm, key=lambda c: c["index"])},
                      f, indent=2)
    # Per-rule reachability: which registry entries this leg exercised. Zero
    # means every claim the leg makes about that rule is vacuous.
    if stats_cases:
        cols = " ".join(
            f"{label}={reach_rules[label]}"
            + ("!" if reach_rules[label] == 0 else "")
            + ("?" if reach_spec_seen[label] != stats_cases else "")
            for label, _ in REACH_SPECS)
        print(f"fuzz: reached (cases firing each rule, of {stats_cases} with "
              f"stats; ! = never fired, ? = counter line missing): {cols}")
    if controller["cases"]:
        with open(os.path.join(outdir, "controller-coverage.json"), "w") as f:
            json.dump(controller, f, indent=2, sort_keys=True)
        def zero(kind, key):
            return sorted(name for name, entry in controller[kind].items()
                          if not entry["cases"].get(key, 0))
        print(f"fuzz: controller coverage over {controller['cases']} cases "
              f"{controller['verdicts']}: families never applied "
              f"{zero('rule', 'applications')}, never effective "
              f"{zero('rule', 'effective')}; configurations without an "
              f"interval {zero('search', 'intervals')}, without a decision "
              f"{zero('search', 'decisions')}; loop "
              f"{controller['loop']['cases']} root-sequence>=2 in "
              f"{controller['loop']['cases_root_sequence_ge2']} cases "
              f"(detail: controller-coverage.json)")
    missing_controller = missing_controller_requirements(
        controller,
        args.require_controller_applied or (),
        args.require_controller_effective or ())
    if missing_controller:
        print("fuzz: REQUIRED CONTROLLER COVERAGE MISSING: "
              f"{missing_controller}")
    missing_spec = [label for label, _ in REACH_SPECS
                    if stats_cases and reach_spec_seen[label] == 0]
    if missing_spec:
        print("fuzz: REACH-SPEC MISS (the harness looked for a counter line "
              "the solver never printed; the readout for these rules is "
              f"meaningless until the spec is fixed): {missing_spec}")
    unreached = [label for label in (args.require_reached or [])
                 if reach_rules.get(label, 0) == 0]
    if unreached:
        print(f"fuzz: REQUIRED RULES NEVER FIRED: {unreached}")
    below_target_minimum = stress_fires < args.minimum_target_fires
    print(f"fuzz: stress target applications={stress_fires}")
    if below_target_minimum:
        print(f"fuzz: stress target applications {stress_fires} is below "
              f"required minimum {args.minimum_target_fires}")
    over_budget = (args.max_nonterminating_frac is not None and ran
                   and nnon / ran > args.max_nonterminating_frac)
    if over_budget:
        print(f"fuzz: non-terminating fraction {nnon / ran:.3f} exceeds "
              f"--max-nonterminating-frac {args.max_nonterminating_frac}")
    below_verdict_minimum = effective < args.minimum_verdicts
    if below_verdict_minimum:
        print(f"fuzz: verdict coverage {effective} is below required "
              f"minimum {args.minimum_verdicts}")
    return 1 if (nfail or missing_spec or unreached or missing_controller
                 or below_target_minimum
                 or over_budget
                 or below_verdict_minimum) else 0


def cmd_check_one(args):
    solver = substitute_seed(shlex.split(args.solver), CORPUS_SEED)
    oracle = shlex.split(args.oracle) if args.oracle else None
    with tempfile.TemporaryDirectory(prefix="fuzz-check-") as workdir:
        outcome = run_case(args.cnf, solver, oracle, args.timeout,
                           args.lrat, workdir)
    missing_controller = missing_controller_requirements(
        controller_coverage(outcome.solver_out),
        args.require_controller_applied or (),
        args.require_controller_effective or ())
    if missing_controller:
        outcome.fail("coverage", "required controller coverage missing: " +
                     ", ".join(missing_controller))
    kinds = {k for k, _ in outcome.failures}
    print(f"verdict={outcome.verdict} oracle={outcome.oracle_verdict} "
          f"failures={outcome.failures}"
          + (f" skipped={outcome.skips}" if outcome.skips else ""))
    if args.fail_kind:
        return 0 if args.fail_kind in kinds else 1
    return 0 if kinds else 1


LEGACY_POLICY_ARGS = ("--random-policy", "--stress-policy", "--replay=",
                      "--trace=", "--no-heuristics")


def cmd_corpus(args):
    corpus = os.path.join(HERE, "corpus", "regressions")
    solver = substitute_seed(shlex.split(args.solver), CORPUS_SEED)
    oracle = shlex.split(args.oracle) if args.oracle else None
    entries = sorted(
        d for d in (os.listdir(corpus) if os.path.isdir(corpus) else [])
        if os.path.isdir(os.path.join(corpus, d)))
    nfail = 0
    nonterm = 0
    with tempfile.TemporaryDirectory(prefix="fuzz-corpus-") as workdir:
        for entry in entries:
            for cnf_name in ("min.cnf", "case.cnf"):
                cnf = os.path.join(corpus, entry, cnf_name)
                if os.path.exists(cnf):
                    break
            else:
                continue
            # A case only reproduces under the SCHEDULE that produced it, and
            # some of them need a non-default one. meta.json has carried
            # `solver_args` / `solver_env` since entry 011 and this replay
            # ignored them, so such an entry passed for the wrong reason -- a
            # regression test that cannot fail. Both are now applied on top of
            # the caller's solver command (env restored afterwards; the loop is
            # sequential, so this is safe).
            meta = {}
            meta_path = os.path.join(corpus, entry, "meta.json")
            if os.path.exists(meta_path):
                with open(meta_path) as f:
                    meta = json.load(f)
            entry_args = list(meta.get("solver_args", []))
            entry_env = meta.get("solver_env") or {}
            if args.replace_entry_policy:
                # The leg's own policy replaces an entry's legacy schedule
                # (the CLI rejects combining two policies); proof-format
                # arguments stay, and the legacy replay belongs to the
                # DefaultPolicy leg, which applies solver_args unchanged.
                entry_args = [a for a in entry_args
                              if not a.startswith(LEGACY_POLICY_ARGS)]
                entry_env = {k: v for k, v in entry_env.items()
                             if not k.startswith("TACTIX_STRESS_")}
            entry_solver = solver + entry_args
            saved = {k: os.environ.get(k) for k in entry_env}
            os.environ.update({k: str(v) for k, v in entry_env.items()})
            try:
                outcome = run_case(cnf, entry_solver, oracle, args.timeout,
                                   args.lrat, workdir)
            finally:
                for k, v in saved.items():
                    if v is None:
                        os.environ.pop(k, None)
                    else:
                        os.environ[k] = v
            if outcome.failures:
                nfail += 1
                print(f"CORPUS FAIL {entry}: {outcome.failures}")
            # A replay that never reaches a verdict runs no SEMANTIC oracle
            # (model / proof / differential all need SAT or UNSAT), so for an
            # entry whose recorded failure was one of those, a timeout means the
            # entry is present and testing nothing — the same class as the
            # ignored `solver_args` above, and fatal for the same reason.
            #
            # It is NOT vacuous for a `crash`/`io` entry: those reproduce as an
            # assert/sanitizer abort, and the fixed solver's expected behaviour
            # is exactly "runs past the budget without aborting" (entry 006's own
            # meta says so). The crash oracle ran; only the semantic ones did
            # not. So report it and move on, and say which oracle stood.
            if outcome.verdict in ("TIMEOUT", "UNKNOWN"):
                # entries record their kind as `failure` (a string) or
                # `failure_kind` (a list); crash-only means EVERY recorded kind
                # is one the crash oracle covers
                kinds = meta.get("failure_kind") or [meta.get("failure", "")]
                kind = ",".join(k for k in kinds if k)
                crash_only = bool(kind) and all(k in ("crash", "io")
                                                for k in kinds)
                if crash_only:
                    print(f"CORPUS crash-oracle only {entry}: no verdict at "
                          f"--timeout {args.timeout}s, and no abort — which is "
                          f"the recorded pass condition for a `{kind}` entry")
                else:
                    nonterm += 1
                    print(f"CORPUS NON-TERMINATING {entry}: verdict "
                          f"{outcome.verdict} at --timeout {args.timeout}s — "
                          f"the `{kind or 'semantic'}` oracle this entry exists "
                          f"for did not run")
    print(f"corpus: {len(entries)} entries, {nfail} failures"
          + (f", {nonterm} non-terminating (no oracle ran)" if nonterm else ""))
    # Non-termination is fatal by default: an entry that cannot finish is not
    # replaying its regression. A weak policy (a stress policy driving whole
    # rounds to a fixpoint, with per-step invariant checks) genuinely cannot
    # finish the larger entries, so a leg may pre-register a CAP — a number in
    # the leg's own script, never a silent default.
    if nonterm > args.allow_nonterminating:
        print(f"corpus: {nonterm} non-terminating exceeds "
              f"--allow-nonterminating {args.allow_nonterminating}")
        return 1
    return 1 if nfail else 0


def cmd_checkers(args):
    hashes = verify_checker_collection()
    if hashes is None:
        print(f"core checkers: {CHECKER_DIR} (unfrozen default links)")
    else:
        print(f"core checkers: {CHECKER_DIR} (manifest {CHECKER_MANIFEST})")
        for name in CORE_CHECKERS:
            print(f"  {name} {hashes[name]}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("--solver", required=True,
                       help="solver under test (competition CLI: "
                            "solver CNF [PROOF]; exit 10/20)")
        p.add_argument("--oracle", default=None,
                       help="oracle solver for differential testing")
        p.add_argument("--timeout", type=float, default=5.0)
        p.add_argument("--lrat", action="store_true",
                       help="also convert explicit DRAT to LRAT and check it "
                        "with cake_lpr (native DSR always runs its required "
                        "proof pipeline)")

    def controller_requirements(p):
        p.add_argument("--require-controller-applied", default=None,
                       type=lambda s: [x for x in s.split(",") if x],
                       help="comma-separated controller root families that "
                            "must execute at least one application")
        p.add_argument("--require-controller-effective", default=None,
                       type=lambda s: [x for x in s.split(",") if x],
                       help="comma-separated controller root families that "
                            "must make at least one effective solver-state "
                            "transition")

    p = sub.add_parser("run")
    common(p)
    controller_requirements(p)
    p.add_argument("--n", type=int, default=1000)
    p.add_argument("--seed", type=int, default=20260709)
    p.add_argument("--outdir", default=os.path.join(HERE, "failures"))
    p.add_argument("--max-failures", type=int, default=20)
    p.add_argument("--no-shrink", action="store_true")
    p.add_argument("--max-vars", type=int, default=None,
                   help="re-roll cases above this size (weak-solver legs "
                        "need verdicts for the semantic oracles to fire)")
    p.add_argument("--require-reached", default=None,
                   type=lambda s: [x for x in s.split(",") if x],
                   help="comma-separated registry entries this leg must fire "
                        "(else it exits nonzero): "
                        + ",".join(label for label, _ in REACH_SPECS))
    p.add_argument("--max-nonterminating-frac", type=float, default=None,
                   help="fail the leg when more than this FRACTION of cases "
                        "never reached a verdict (default: report only). Such "
                        "cases run no semantic oracle, so they are lost "
                        "coverage, not passes")
    p.add_argument("--max-nonterminating-saved", type=int, default=25,
                   help="keep at most this many non-terminating cases (CNF + "
                        "repro) under <outdir>/nonterminating/")
    p.add_argument("--minimum-verdicts", type=int, default=0,
                   help="fail unless this many cases reach SAT or UNSAT")
    p.add_argument("--minimum-target-fires", type=int, default=0,
                   help="fail unless the stress driver reports this many "
                        "preferred-rule applications")
    p.add_argument("--jobs", type=int,
                   default=int(os.environ.get("SLURM_CPUS_PER_TASK", 8)),
                   help="parallel fuzz workers (correctness testing, not "
                        "timing — contention is harmless; results are "
                        "independent of the job count); defaults to the "
                        "SLURM allocation when run inside a job")
    p.set_defaults(func=cmd_run)

    p = sub.add_parser("check-one")
    common(p)
    controller_requirements(p)
    p.add_argument("cnf")
    p.add_argument("--fail-kind", default=None,
                   choices=["crash", "io", "model", "proof", "differential",
                            "coverage"])
    p.set_defaults(func=cmd_check_one)

    p = sub.add_parser("corpus")
    common(p)
    p.add_argument("--allow-nonterminating", type=int, default=0,
                   help="entries allowed to not reach a verdict (they replay "
                        "no oracle); default 0 = any is fatal")
    p.add_argument("--replace-entry-policy", action="store_true",
                   help="drop an entry's legacy policy arguments/stress "
                        "environment so the leg's own policy (e.g. "
                        "--controller-policy) replays it")
    p.set_defaults(func=cmd_corpus)

    p = sub.add_parser("checkers",
                       help="print and verify the core checker collection")
    p.set_defaults(func=cmd_checkers)

    args = ap.parse_args()
    verify_checker_collection()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
