#!/usr/bin/env python3
"""Reach phase for 20260827-1120-rank5-deferred-state-guidance-fuzz.

The isolated Rank-5 deferred-state/deferred-guidance candidate (accepted
source base 3c3164d, working src/tests diff SHA c15d14e8...f777b over five
files) changes exactly one lifecycle surface: the shipped DefaultPolicy
frontend (main.cpp, the no-random/no-stress/no-replay path) calls
kernel_defer_cold_state before parsing, postponing 40 bytes/variable of
rule scratch/touch state (bve_retry, backbone_priority, touched, removed,
touched_kept, bve_min_bound, seen, eager_marks, marked_levels,
probing.stamp) and leaving search guidance H unattached through startup
propagation and early Lucky. Touch/removal/kept/BVE-mark EVENTS still
advance touch_epoch and the stats counters exactly; only the array writes
are skipped while deferred, and every read predicate asserts
!cold_state_deferred in this Debug pin. A failed early opportunity calls
kernel_materialize_cold_state and then attaches ScopedHeuristics (consuming
the parser's retained activation order) before sparse collection and every
later rule. Random, replay, stress, and API/bare-kernel surfaces keep the
eager lifecycle.

No printed solver counter separates the changed lifecycle from its
unchanged neighbors: `c lucky N early` counts calls, not early-terminal
verdicts reached with the state still unmaterialized, and nothing prints
whether materialization or the deferred H attach ran. Per the fuzz skill
each changed path gets a direct gdb witness on the exact campaign pin
(line numbers frozen by the campaign's source-identity checks):

  dfr src/kernel/kernel.cpp:34            kernel_defer_cold_state body
                                          (`k.cold_state_deferred = true`):
                                          the default-frontend-only defer
                                          entry (counting).
  tvd src/kernel/kernel.hpp:1206          kernel_touch_var's epoch line
      [cond k.cold_state_deferred,        WHILE DEFERRED: a touch event
       first hit only]                    whose epoch/stats advanced but
                                          whose array write was skipped
                                          (default universes only; the
                                          negative controls prove dfr==0,
                                          which makes deferred touches
                                          impossible by construction).
  mat src/kernel/kernel.cpp:48            kernel_materialize_cold_state
                                          PAST the early return
                                          (`k.bve_retry.assign`): actual
                                          materialization after a failed
                                          early opportunity (counting).
  hat src/policies/default_policy.cpp:2341 `owned_h.emplace(k)`: the
                                          deferred search-guidance attach,
                                          crossing the second boundary
                                          right after mat (counting).
  hea src/main.cpp:245                    `kernel.h = &heuristics`: the
                                          EAGER pre-parse session H attach
                                          that the default frontend no
                                          longer takes and random/replay/
                                          stress must still take (counting).
  egr src/kernel/kernel.cpp:86            kernel_reserve_vars' eager
                                          `k.bve_retry.resize` under
                                          `!cold_state_deferred`: the eager
                                          cold-state build the negative
                                          controls must take at parse and
                                          the default frontend may take
                                          only after materialization
                                          (post-materialize var-adding
                                          rules) (counting).

Derived whole-case witnesses over the default universes:
  ets = dfr==1 and mat==0 and hat==0 and exit==10: an early-Lucky SAT
        terminal with cold state AND guidance never materialized.
  etu = dfr==1 and mat==0 and hat==0 and exit==20: a startup-propagation
        root refutation before either boundary (the solve returns 20
        before kernel_materialize_cold_state).
  xbo = dfr==1 and mat==1 and hat==1: a failed early opportunity that
        crossed BOTH materialization boundaries before preprocessing and
        search.
  tvd>=1: at least one touch event was recorded while deferred.

kernel_defer_cold_state has exactly one production caller: main.cpp's
default frontend (`!random_policy && !stress_policy && !replay_path`). The
stress and random universes below are therefore NEGATIVE CONTROLS: every
one of dfr/tvd/mat/hat must be exactly zero there AND the eager lifecycle
must be positively witnessed (hea==1, egr>=1), or the deferral leaked
outside the shipped frontend and the campaign fails.

Per-case consistency (checked for every terminating default gdb case):
  - exit code in {10, 20};
  - dfr == 1 (the default frontend always defers) and hea == 0 (it never
    eagerly attaches the session H);
  - hat == mat (guidance attaches exactly when the boundary is crossed)
    and mat <= 1;
  - mat == 0 implies egr == 0 (nothing may eagerly build cold state while
    it is deferred; with mat==1 a post-materialization var-adding rule may
    legally take the eager resize path).

Scan universes select cases at the exact 5-second quick budget from the
harness's own deterministic generator distribution; the three default
selection sets carve disjoint case classes from one scanned universe
(early-SAT: exit 10 with zero decisions; early-UNSAT: exit 20 with zero
decisions and zero learned; crossed: at least one decision, ordered by
descending learned). Selected cases run under gdb and every terminating
gdb case must be oracle-clean under fuzz.py check-one (independent
original-CNF model check on SAT, native-LRAT lrat-check acceptance on
UNSAT, pinned kissat differential).

Usage: reach.py <tactix-debug-pin>
Runs on a compute node inside reach.sbatch (never in the session shell),
executed from that job's read-only harness snapshot; fuzz.py,
generators.py and their siblings are loaded from this file's own directory
when present there (the snapshot layout) and from tools/fuzz otherwise.
The prep probe imports scan_one/gdb_case/run_gdb_case from this file so
the submitted recipe is validated before any leg exists; keep them
import-safe.
"""
import concurrent.futures
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.environ.get("TACTIX_REPO_ROOT", "/home/hwu/SAT-solving")
FUZZ_DIR = HERE if os.path.exists(os.path.join(HERE, "fuzz.py")) \
    else os.path.join(ROOT, "tools", "fuzz")
FUZZ = os.path.join(FUZZ_DIR, "fuzz.py")
ORACLE = os.path.join(ROOT, "tools", "solvers", "bin", "kissat")
sys.path.insert(0, FUZZ_DIR)
from generators import generate, write_dimacs  # noqa: E402

BASE_ENV = {"ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
            "UBSAN_OPTIONS": "halt_on_error=1"}
BPB_STRESS = {"TACTIX_STRESS_RULES": "BVE,Probe,Backbone",
              "TACTIX_STRESS_NORMAL_EVERY": "6"}

STATS = {
    "decisions": re.compile(r"^c decisions\s+(\d+)", re.M),
    "propagations": re.compile(r"^c propagations\s+(\d+)", re.M),
    "learned": re.compile(r"^c learned\s+(\d+)", re.M),
    "lucky_probes": re.compile(
        r"^c lucky\s+\d+ early, \d+ late \(\d+ SAT\), (\d+) probes", re.M),
    "bve_resolutions": re.compile(
        r"^c bve\s+\d+ rounds, \d+ eliminated, (\d+) resolutions", re.M),
    "probe_probed": re.compile(r"^c probe\s+\d+ rounds, (\d+) probed,", re.M),
    "backbone_probed": re.compile(
        r"^c backbone\s+\d+ rounds, (\d+) probed,", re.M),
    "target_fires": re.compile(r"^c stress-target-fires (\d+)", re.M),
}

# (name, location, condition, first-hit-only). Each breakpoint counts into
# its own gdb convenience variable ($dfr, $tvd, ...) via a `commands` block
# attached immediately after creation, so witness identity never depends on
# gdb's breakpoint numbering. tvd disables after its first hit: its
# condition sits on the touch-event hot line and one deferred touch per
# case is the whole claim. tvd is created only for default-universe cases
# (include_tvd): for the negative controls dfr==0 already proves
# cold_state_deferred is never set (kernel_defer_cold_state is its only
# writer), and evaluating a conditional breakpoint on every eager touch
# event would only slow the case without adding evidence.
BPS = [
    ("dfr", "src/kernel/kernel.cpp:34", None, False),
    ("tvd", "src/kernel/kernel.hpp:1206", "k.cold_state_deferred", True),
    ("mat", "src/kernel/kernel.cpp:48", None, False),
    ("hat", "src/policies/default_policy.cpp:2341", None, False),
    ("hea", "src/main.cpp:245", None, False),
    ("egr", "src/kernel/kernel.cpp:86", None, False),
]
COUNT_KEYS = ("dfr", "tvd", "mat", "hat", "hea", "egr", "exit")
COUNTS_RE = re.compile(
    r"REACH-COUNTS dfr=(\d+) tvd=(\d+) mat=(\d+) hat=(\d+) hea=(\d+) "
    r"egr=(\d+) exit=(-?\d+)")
GDB_SETUP_ERRORS = ("No source file named", "No line ", "No symbol",
                    "Error in testing", "Cannot insert breakpoint")

SCAN_INDEX = 20
SCAN_TIMEOUT = 5.0
GDB_TIMEOUT = 600
CHECK_TIMEOUT = 900

# Floors over all terminating default-universe gdb cases. Attempt-1
# provisional values from the expected shape of the changed lifecycle
# (every default case defers; early-Lucky SAT and crossed-boundary search
# both common in the generator distribution at the quick budget; the
# root-refutation class etu is intrinsically rarer, floor 1); the prep
# probe measures the real yield and a failed probe revises these floors
# with its measured basis before any leg is submitted.
FLOORS = {"ets": 3, "etu": 1, "xbo": 3, "tvd": 3}
MIN_SELECT = {"default-early-sat": 3, "default-early-unsat": 1,
              "default-crossed": 3, "bpb-stress": 5, "random": 5}

# One scanned default universe; three disjoint selection sets carve the
# early-SAT / early-UNSAT / crossed classes. Seed ranges are disjoint from
# every prior campaign's scan universes (highest previously used: 1019999).
DEFAULT_SEEDS = range(1100000, 1103000)
DEFAULT_SETS = [
    ("default-early-sat",
     lambda code, s: code == 10 and s["decisions"] == 0,
     lambda row: row[0], 10),
    ("default-early-unsat",
     lambda code, s: code == 20 and s["decisions"] == 0
     and s["learned"] == 0,
     lambda row: row[0], 8),
    ("default-crossed",
     lambda code, s: s["decisions"] >= 1,
     lambda row: (-row[1]["learned"], row[0]), 10),
]
# Negative-control universes: the eager lifecycle must be positively
# witnessed and every deferral witness must be zero. Selection prefers
# cases where the eager cold-state consumers (BVE/Probe/Backbone) actually
# fired, so the controls also show consumers reading the eagerly built
# arrays.
NEG_UNIVERSES = [
    ("bpb-stress", range(1103000, 1103400), BPB_STRESS,
     ("--stress-policy",),
     lambda code, s: s["bve_resolutions"] + s["probe_probed"]
     + s["backbone_probed"] >= 1,
     lambda row: (-(row[1]["bve_resolutions"] + row[1]["probe_probed"]
                    + row[1]["backbone_probed"]), row[0]), 6),
    ("random", range(1104000, 1104200), None, ("--random-policy={seed}",),
     lambda code, s: s["bve_resolutions"] + s["probe_probed"]
     + s["backbone_probed"] >= 1,
     lambda row: (-(row[1]["bve_resolutions"] + row[1]["probe_probed"]
                    + row[1]["backbone_probed"]), row[0]), 6),
]

# Deterministic witnesses measured by a prior prep attempt on the exact
# pin (seed, set-label, extra env, args); empty until an attempt measures
# them. Universe selection excludes KNOWN_SEEDS so no case counts twice.
KNOWN_CASES = []
KNOWN_SEEDS = {seed for seed, _, _, _ in KNOWN_CASES}


def solver_env(extra):
    return {**os.environ, **BASE_ENV, **(extra or {})}


def case_path(workdir, seed):
    name, n, clauses = generate(seed, SCAN_INDEX)
    path = os.path.join(workdir, f"case-{seed}.cnf")
    write_dimacs(path, n, clauses)
    return path


def sub_args(solver_args, seed):
    return tuple(a.replace("{seed}", str(seed)) for a in solver_args)


def scan_one(pin, workdir, seed, extra_env=None, solver_args=()):
    """(seed, exitcode-or-None, {stat: count}) at the exact quick budget."""
    path = case_path(workdir, seed)
    args = sub_args(solver_args, seed)
    try:
        r = subprocess.run([pin, *args, path], capture_output=True,
                           text=True, timeout=SCAN_TIMEOUT,
                           env=solver_env(extra_env))
    except subprocess.TimeoutExpired:
        os.unlink(path)
        return seed, None, {key: 0 for key in STATS}
    os.unlink(path)
    stats = {}
    for key, rx in STATS.items():
        m = rx.search(r.stdout)
        stats[key] = int(m.group(1)) if m else 0
    return seed, r.returncode, stats


def gdb_script(workdir, pin, path, extra_env=None, solver_args=(),
               include_tvd=True):
    lines = ["set confirm off", "set pagination off",
             "set startup-with-shell off", "set breakpoint pending off"]
    for key, val in {**BASE_ENV, **(extra_env or {})}.items():
        lines.append(f"set environment {key}={val}")
    lines.append(f"file {pin}")
    lines.append("set args " + " ".join([*solver_args, path]))
    for name, _, _, _ in BPS:
        lines.append(f"set ${name} = 0")
    bpnum = 0
    for name, loc, cond, once in BPS:
        if name == "tvd" and not include_tvd:
            continue
        bpnum += 1
        lines.append(f"break {loc}" + (f" if {cond}" if cond else ""))
        lines.append(f"commands {bpnum}")
        lines.append("silent")
        lines.append(f"set ${name} = ${name} + 1")
        if once:
            lines.append(f"disable {bpnum}")
        lines.append("continue")
        lines.append("end")
    lines.append("run")
    lines.append('printf "REACH-COUNTS dfr=%d tvd=%d mat=%d hat=%d hea=%d '
                 'egr=%d exit=%d\\n", $dfr, $tvd, $mat, $hat, $hea, $egr, '
                 "$_exitcode")
    script = os.path.join(workdir, f"reach-{os.path.basename(path)}.gdb")
    with open(script, "w") as f:
        f.write("\n".join(lines) + "\n")
    return script


def gdb_case(pin, workdir, seed, extra_env=None, solver_args=(),
             include_tvd=True):
    """{count-key: int} for one case, or None for a bounded nontermination.

    Exits the process on gdb setup failure (unresolved breakpoint line or
    condition symbol): that is a recipe defect, never case evidence."""
    path = case_path(workdir, seed)
    args = sub_args(solver_args, seed)
    script = gdb_script(workdir, pin, path, extra_env, args, include_tvd)
    try:
        r = subprocess.run(["gdb", "-batch", "-nx", "-x", script],
                           capture_output=True, text=True,
                           timeout=GDB_TIMEOUT, env=solver_env(extra_env),
                           cwd=ROOT)
    except subprocess.TimeoutExpired:
        os.unlink(path)
        os.unlink(script)
        return None
    os.unlink(path)
    os.unlink(script)
    blob = (r.stdout or "") + (r.stderr or "")
    for bad in GDB_SETUP_ERRORS:
        if bad in blob:
            sys.exit(f"REACH FAIL: gdb setup error on seed {seed} "
                     f"('{bad}'); tail: {blob[-600:]}")
    m = COUNTS_RE.search(r.stdout or "")
    if not m:
        sys.exit(f"REACH FAIL: no REACH-COUNTS line for seed {seed}; "
                 f"tail: {blob[-600:]}")
    return dict(zip(COUNT_KEYS, map(int, m.groups())))


def check_one(pin, workdir, seed, extra_env=None, solver_args=()):
    """(oracle_clean, summary_line) via the harness's own per-case oracles."""
    path = case_path(workdir, seed)
    solver = ("env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "
              "UBSAN_OPTIONS=halt_on_error=1 ")
    for key, val in (extra_env or {}).items():
        solver += f"{key}={val} "
    solver += pin
    for a in sub_args(solver_args, seed):
        solver += " " + a
    solver += " --lrat"
    try:
        c = subprocess.run(
            ["python3", FUZZ, "check-one", "--solver", solver,
             "--oracle", ORACLE, "--lrat", "--timeout", "300", path],
            capture_output=True, text=True, timeout=CHECK_TIMEOUT,
            env=solver_env(None), cwd=ROOT)
    except subprocess.TimeoutExpired:
        sys.exit(f"REACH FAIL: seed={seed} check-one harness hung at "
                 f"{CHECK_TIMEOUT} s")
    os.unlink(path)
    lines = (c.stdout or "").strip().splitlines()
    line = lines[-1] if lines else ""
    return "failures=[]" in line, line


def case_flags_from_counts(counts):
    return {
        "ets": counts["dfr"] == 1 and counts["mat"] == 0
        and counts["hat"] == 0 and counts["exit"] == 10,
        "etu": counts["dfr"] == 1 and counts["mat"] == 0
        and counts["hat"] == 0 and counts["exit"] == 20,
        "xbo": counts["dfr"] == 1 and counts["mat"] == 1
        and counts["hat"] == 1,
        "tvd": counts["tvd"] >= 1,
    }


def run_gdb_case(pin, workdir, uname, seed, extra_env, solver_args,
                 all_flags, negative_control=False):
    """gdb + oracle for one case; returns False for a bounded
    nontermination (no claim), True after recording the case's flags.
    Exits the process on any reach-failure condition."""
    counts = gdb_case(pin, workdir, seed, extra_env, solver_args,
                      include_tvd=not negative_control)
    if counts is None:
        print(f"[{uname}] gdb seed={seed}: NONTERMINATING at "
              f"{GDB_TIMEOUT}s under gdb (no oracle ran, no claim)")
        sys.stdout.flush()
        return False
    print(f"[{uname}] gdb seed={seed}: " + " ".join(
        f"{key}={counts[key]}" for key in COUNT_KEYS))
    sys.stdout.flush()
    if counts["exit"] not in (10, 20):
        sys.exit(f"REACH FAIL: seed={seed} unexpected exit "
                 f"{counts['exit']} under gdb")
    row = " ".join(f"{k}={counts[k]}" for k in COUNT_KEYS)
    if negative_control:
        if any(counts[key] for key in ("dfr", "tvd", "mat", "hat")):
            sys.exit(f"REACH FAIL: seed={seed} ({uname}) hit a deferral "
                     f"witness outside the shipped default frontend: {row}; "
                     "kernel_defer_cold_state leaked past main.cpp's "
                     "default_frontend gate")
        if counts["hea"] != 1:
            sys.exit(f"REACH FAIL: seed={seed} ({uname}) eager session H "
                     f"attach missing (hea={counts['hea']}): {row}; the "
                     "eager lifecycle changed for a non-default frontend")
        if counts["egr"] < 1:
            sys.exit(f"REACH FAIL: seed={seed} ({uname}) eager cold-state "
                     f"resize missing: {row}; the eager lifecycle changed "
                     "for a non-default frontend")
        ok, line = check_one(pin, workdir, seed, extra_env, solver_args)
        print(f"  oracle: {line}")
        sys.stdout.flush()
        if not ok:
            sys.exit(f"REACH FAIL: seed={seed} oracle failures: {line}")
        return True
    if counts["dfr"] != 1:
        sys.exit(f"REACH FAIL: seed={seed} default frontend did not defer "
                 f"exactly once: {row}")
    if counts["hea"] != 0:
        sys.exit(f"REACH FAIL: seed={seed} default frontend eagerly "
                 f"attached the session H: {row}")
    if counts["hat"] != counts["mat"] or counts["mat"] > 1:
        sys.exit(f"REACH FAIL: seed={seed} materialization/guidance "
                 f"boundary mismatch (hat must equal mat <= 1): {row}")
    if counts["mat"] == 0 and counts["egr"] > 0:
        sys.exit(f"REACH FAIL: seed={seed} eager cold-state resize while "
                 f"deferred and unmaterialized: {row}")
    all_flags.append(case_flags_from_counts(counts))
    ok, line = check_one(pin, workdir, seed, extra_env, solver_args)
    print(f"  oracle: {line}")
    sys.stdout.flush()
    if not ok:
        sys.exit(f"REACH FAIL: seed={seed} oracle failures: {line}")
    return True


def main():
    pin = os.path.abspath(sys.argv[1])
    workdir = tempfile.mkdtemp(prefix="r5-reach-",
                               dir=os.environ.get("TMPDIR"))
    workers = int(os.environ.get("SLURM_CPUS_PER_TASK", "2"))
    all_flags = []  # one {floor-key: bool} per terminating default gdb case
    nonterminating = 0

    for seed, uname, extra_env, solver_args in KNOWN_CASES:
        if not run_gdb_case(pin, workdir, uname, seed, extra_env,
                            solver_args, all_flags):
            nonterminating += 1

    with concurrent.futures.ThreadPoolExecutor(workers) as ex:
        rows = list(ex.map(lambda s: scan_one(pin, workdir, s),
                           DEFAULT_SEEDS))
    terminated = [(seed, code, stats) for seed, code, stats in rows
                  if code is not None and seed not in KNOWN_SEEDS]
    print(f"[default] scan: {len(rows)} cases at {SCAN_TIMEOUT}s, "
          f"{len(terminated)} terminated (known cases excluded)")
    sys.stdout.flush()
    for sname, selector, order, gdb_cases in DEFAULT_SETS:
        firing = [(seed, stats) for seed, code, stats in terminated
                  if selector(code, stats)]
        firing.sort(key=order)
        print(f"[{sname}] {len(firing)} selected")
        sys.stdout.flush()
        if len(firing) < MIN_SELECT[sname]:
            sys.exit(f"REACH FAIL: only {len(firing)} {sname} cases "
                     f"selected (need {MIN_SELECT[sname]})")
        for seed, _ in firing[:gdb_cases]:
            if not run_gdb_case(pin, workdir, sname, seed, None, (),
                                all_flags):
                nonterminating += 1

    for uname, seeds, extra_env, solver_args, selector, order, gdb_cases \
            in NEG_UNIVERSES:
        with concurrent.futures.ThreadPoolExecutor(workers) as ex:
            rows = list(ex.map(
                lambda s: scan_one(pin, workdir, s, extra_env, solver_args),
                seeds))
        firing = [(seed, stats) for seed, code, stats in rows
                  if code is not None and selector(code, stats)
                  and seed not in KNOWN_SEEDS]
        firing.sort(key=order)
        print(f"[{uname}] scan: {len(rows)} cases at {SCAN_TIMEOUT}s, "
              f"{len(firing)} selected")
        sys.stdout.flush()
        if len(firing) < MIN_SELECT[uname]:
            sys.exit(f"REACH FAIL: only {len(firing)} {uname} cases "
                     f"selected (need {MIN_SELECT[uname]})")
        for seed, _ in firing[:gdb_cases]:
            if not run_gdb_case(pin, workdir, uname, seed, extra_env,
                                solver_args, all_flags,
                                negative_control=True):
                nonterminating += 1

    totals = {key: sum(flags[key] for flags in all_flags) for key in FLOORS}
    print(f"reach summary: {len(all_flags)} terminating default-floor gdb "
          f"cases ({nonterminating} bounded nonterminations, no claim), "
          "cases per witness: "
          + " ".join(f"{key}={totals[key]}" for key in FLOORS))
    for key, floor in FLOORS.items():
        if totals[key] < floor:
            sys.exit(f"REACH FAIL: only {totals[key]} cases hit witness "
                     f"'{key}' (need {floor})")
    print("REACH OK: the default-frontend defer entry, deferred touch "
          "events, early-Lucky SAT and startup-refutation terminals with "
          "cold state and guidance unmaterialized, and failed early "
          "opportunities crossing both materialization boundaries are all "
          "directly witnessed with full per-case oracles, and every "
          "deferral witness is exactly zero under the stress/random "
          "negative controls while their eager lifecycle is positively "
          "witnessed")


if __name__ == "__main__":
    main()
