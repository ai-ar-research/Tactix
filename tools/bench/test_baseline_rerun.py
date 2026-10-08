#!/usr/bin/env python3
"""Selection, limits and an end-to-end fake-pin task for baseline_rerun.py.

Fake solvers/checkers are tiny shell scripts; nothing here runs a real solver.
"""
import importlib.util
import json
import lzma
import os
import signal
import time
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
spec = importlib.util.spec_from_file_location("baseline_rerun", HERE / "baseline_rerun.py")
br = importlib.util.module_from_spec(spec)
spec.loader.exec_module(br)
FROZEN = REPO / "benchmarks/baselines/sc24-26-32g-5000s"


def stage(name, rc, out="", err="", timed_out=False, **extra):
    return dict(name=name, exit_code=rc, stdout_tail=out, stderr_tail=err,
                timed_out=timed_out, **extra)


def check(*stages):
    return {"result": "not-verified", "stages": list(stages)}


class Classification(unittest.TestCase):
    def test_classes(self):
        cases = [
            (check(stage("drat-trim", 0, "c MEMOUT: failed to reallocate lrat table")), "memout"),
            (check(stage("drat-trim", 0, "start verification\ns TIMEOUT")), "timeout"),
            (check(stage("veripb", None, timed_out=True)), "timeout"),
            (check(stage("veripb", 134, err="memory allocation of 592 bytes failed")), "memout"),
            (check(stage("veripb", 134, err="Os { code: 12, kind: OutOfMemory }")), "memout"),
            (check(stage("cake-pb-cnf", 1, err="CakeML heap space exhausted.")), "memout"),
            (check(stage("dsr-trim", 255, err="Ran out of memory on xrealloc()")), "memout"),
            (check(stage("srcheck", 1, err="INTERNAL PANIC: out of memory")), "memout"),
            (check(stage("dsr-trim", 137, killed_by_runner=None)), "memout"),
            (check(stage("dsr-trim", 1, oom_kill_delta=1)), "memout"),
            (check(stage("dsr-trim", 139)), "crash"),
            (check(stage("srcheck", 0, "c Error or invalid proof s INVALID")), "rejected"),
            (check(stage("veripb", 1, err="Error: Elaboration error")), "rejected"),
            (check(stage("drat-trim", 1, "c trivial UNSAT s VERIFIED")), "other"),
            ({"result": "verified", "stages": [stage("cake-lpr", 0)]}, "verified"),
            (None, None),
        ]
        for record, expected in cases:
            self.assertEqual(br.classify_check(record), expected, record)

    def test_timeout_precedes_memory_text(self):
        record = check(stage("veripb", None, err="out of memory", timed_out=True))
        self.assertEqual(br.classify_check(record), "timeout")

    def test_selection_excludes_non_unsat(self):
        rows = [
            {"task": 1, "solver": "s", "verdict": "UNKNOWN",
             "proof_check": check(stage("x", None, timed_out=True))},
            {"task": 2, "solver": "s", "verdict": "UNSAT",
             "proof_check": check(stage("x", None, timed_out=True))},
            {"task": 3, "solver": "t", "verdict": "UNSAT",
             "proof_check": check(stage("x", None, timed_out=True))},
            {"task": 4, "solver": "s", "verdict": "UNSAT", "proof_check": None},
            {"task": 5, "solver": "s", "verdict": "SAT", "proof_check": None},
        ]
        chosen = br.selection_rows(rows, {"s"}, ("timeout", "memout"))
        self.assertEqual([(r["task"], c) for r, c in chosen], [(2, "timeout")])
        self.assertEqual(len(br.selection_rows(rows, {"s"}, ("all",))), 4)

    @unittest.skipUnless((FROZEN / "results.json").is_file(), "frozen results absent")
    def test_frozen_counts(self):
        chosen = br.selection_rows(
            br.load_results(FROZEN),
            {"kissat-4.0.4", "kissat-sc2024", "ae-kissat-mab", "lymphosat"},
            ("timeout", "memout"))
        self.assertEqual(br.selection_counts(chosen), {
            "kissat-4.0.4": {"memout": 7, "timeout": 5},
            "kissat-sc2024": {"memout": 7, "timeout": 4},
            "ae-kissat-mab": {"memout": 3, "timeout": 8},
            "lymphosat": {"memout": 24, "timeout": 7},
        })


class Limits(unittest.TestCase):
    def test_cake_heap(self):
        self.assertEqual(br.cake_heap_mib(32), 24576)  # frozen amendment 2
        self.assertEqual(br.cake_heap_mib(64), 57344)
        with self.assertRaises(ValueError):
            br.cake_heap_mib(8)

    def test_scheduler_time(self):
        self.assertEqual(br.scheduler_time(5000, 28800), "10:15:00")

    def test_group_rss_reads_only_the_group(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc = Path(tmp)
            page_kb = os.sysconf("SC_PAGE_SIZE") // 1024
            def write(pid, comm, pgrp, rss):
                (proc / str(pid)).mkdir()
                fields = ["S", "1", str(pgrp)] + ["0"] * 18 + [str(rss)] + ["0"] * 20
                (proc / str(pid) / "stat").write_text(f"{pid} ({comm}) " + " ".join(fields))
            write(10, "a b) c", 10, 100)
            write(11, "child", 10, 50)
            write(12, "other", 12, 999)
            (proc / "self").mkdir()
            self.assertEqual(br.group_rss_kb(10, proc), 150 * page_kb)

    def test_memory_cap_kills_group(self):
        with tempfile.TemporaryDirectory() as tmp:
            t = Path(tmp)
            rec = br.measured(["sleep", "30"], t / "o", t / "e", 60, t / "m",
                              rss_cap_kb=10, poll_seconds=0.05,
                              sampler=lambda pgid: 11)
            self.assertEqual(rec["killed_by_runner"], "memory-cap")
            self.assertFalse(rec["timed_out"])
            self.assertLess(rec["wall_seconds"], 20)
            rec = br.measured(["sleep", "30"], t / "o", t / "e", 0.3, t / "m",
                              poll_seconds=0.05, sampler=lambda pgid: 0)
            self.assertTrue(rec["timed_out"])
            self.assertIsNone(rec["exit_code"])
            rec = br.measured(["true"], t / "o", t / "e", 10, t / "m")
            self.assertEqual(rec["exit_code"], 0)
            self.assertIsNone(rec["killed_by_runner"])


FAKE_SOLVER = """#!/bin/bash
if [ -n "$FAKE_SLEEP" ]; then printf 'partial' > "$2"; sleep "$FAKE_SLEEP"; fi
if [ -n "$FAKE_SAT" ]; then echo 's SATISFIABLE'; echo 'v 1 0'; exit 10; fi
printf '0\\n' > "$2"; echo 's UNSATISFIABLE'; exit 20
"""
FAKE_DRAT_TRIM = """#!/bin/bash
[ -f "$2" ] || exit 9
echo "TMP=$TMPDIR"; printf '1 0 0\\n' > "$4"; echo 's VERIFIED'
"""
FAKE_CAKE = """#!/bin/bash
[ -f "$2" ] || exit 9
echo "heap=$CML_HEAP_SIZE"; echo 's VERIFIED UNSAT'
"""


class EndToEnd(unittest.TestCase):
    def test_prepare_run_aggregate(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            cache = root / "benchmarks/cache/tierD24-26"
            cache.mkdir(parents=True)
            with lzma.open(cache / "a.cnf.xz", "wb") as f:
                f.write(b"p cnf 1 2\n1 0\n-1 0\n")
            src = root / "benchmarks/baselines/src"
            pins = src / "pins"
            pins.mkdir(parents=True)
            for name, body in (("solver", FAKE_SOLVER), ("drat-trim", FAKE_DRAT_TRIM),
                               ("cake-lpr", FAKE_CAKE), ("veripb", FAKE_CAKE),
                               ("cake-pb-cnf", FAKE_CAKE), ("checkmodel", "#!/bin/bash\nexit 0\n"),
                               ("modelcheck.py", "")):
                (pins / name).write_text(body)
                (pins / name).chmod(0o755)
            solver = {"name": "fake", "kind": "positional", "certificate": "drat",
                      "executable": "pins/solver", "checker_pipeline": "drat-cake-lpr",
                      "vbs_eligible": "SAT+UNSAT"}
            checkers = {n: f"pins/{n}" for n in ("drat-trim", "cake-lpr", "veripb",
                                                  "cake-pb-cnf", "checkmodel", "modelcheck.py")}
            (src / "manifest.json").write_text(json.dumps({
                "solvers": [solver], "checkers": checkers,
                "instances": [{"rel": "x/a.cnf.xz", "expected": "UNSAT"}],
                "tasks": [{"instance": 0, "solver": 0}]}))
            mem_row = {"task": 0, "solver": "fake", "instance": "x/a.cnf.xz",
                       "expected": "UNSAT", "verdict": "UNSAT", "certificate_bytes": 2,
                       "proof_check": check(stage("drat-trim", 0,
                                                  "c MEMOUT: failed to reallocate lrat table"))}
            (src / "results.json").write_text(json.dumps({"results": [mem_row]}))
            env = dict(os.environ, TACTIX_REPO_ROOT=str(root))
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            run = lambda *args, **kw: subprocess.run(
                [sys.executable, str(HERE / "baseline_rerun.py"), *args],
                env=kw.get("env", env), capture_output=True, text=True)
            p = run("--campaign", "benchmarks/baselines/new", "--prepare",
                    "--source", "benchmarks/baselines/src", "--solvers", "fake",
                    "--classes", "memout", "--mem-gib", "64",
                    "--solver-memory-gib", "32", "--reason", "test")
            self.assertEqual(p.returncode, 0, p.stderr)
            camp = root / "benchmarks/baselines/new"
            man = json.loads((camp / "manifest.json").read_text())
            self.assertEqual(man["selection"]["counts"], {"fake": {"memout": 1}})
            self.assertEqual(man["limits"]["cake_heap_mib"], 57344)
            self.assertIn("watchdog", man["limits"]["solver_memory_enforcement"])
            self.assertEqual(man["tasks"][0]["source_task"], 0)
            # a second prepare into the same directory is refused
            self.assertNotEqual(run("--campaign", "benchmarks/baselines/new", "--prepare",
                                    "--source", "benchmarks/baselines/src",
                                    "--solvers", "fake").returncode, 0)
            p = run("--campaign", str(camp), "--run-one", "0")
            self.assertEqual(p.returncode, 0, p.stderr)
            res = json.loads((camp / "results/00000.json").read_text())
            self.assertTrue(res["valid"])
            self.assertEqual([s["name"] for s in res["proof_check"]["stages"]],
                             ["drat-trim", "cake-lpr"])
            self.assertIn("heap=57344", res["proof_check"]["stages"][1]["stdout_tail"])
            self.assertIn(str(camp / "scratch/00000/tmp"),
                          res["proof_check"]["stages"][0]["stdout_tail"])
            self.assertTrue(res["scratch_deleted"])
            self.assertFalse((camp / "scratch/00000").exists())
            # once-only: a second run of the same task is refused
            self.assertNotEqual(run("--campaign", str(camp), "--run-one", "0").returncode, 0)
            p = run("--campaign", str(camp), "--aggregate")
            self.assertEqual(p.returncode, 0, p.stderr)
            agg = json.loads((camp / "results.json").read_text())
            self.assertEqual(agg["summary"], {"fake": {"UNSAT-verified": 1}})
            self.assertEqual(agg["source_class_transitions"],
                             {"fake": {"memout -> UNSAT-verified": 1}})
            self.assertEqual(agg["scratch_leftovers"], [])
            # a pin edit after preparation fails closed
            (pins / "cake-lpr").write_text(FAKE_CAKE + "\n")
            (camp / "claims/00000.json").chmod(0o644)
            self.assertIn("pin mismatch", run("--campaign", str(camp), "--run-one", "0").stderr)
            (pins / "cake-lpr").write_text(FAKE_CAKE)

            # SIGTERM (scancel / time limit) kills the stage group, deletes
            # the task scratch and leaves an incomplete record.
            p = run("--campaign", "benchmarks/baselines/term", "--prepare",
                    "--source", "benchmarks/baselines/src", "--solvers", "fake",
                    "--classes", "all", "--reason", "test")
            self.assertEqual(p.returncode, 0, p.stderr)
            term = root / "benchmarks/baselines/term"
            proc = subprocess.Popen(
                [sys.executable, str(HERE / "baseline_rerun.py"), "--campaign",
                 str(term), "--run-one", "0"],
                env=dict(env, FAKE_SLEEP="30"), stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL)
            partial = term / "scratch/00000/proof-dir/proof.out"
            for _ in range(200):
                if partial.exists():
                    break
                time.sleep(0.05)
            self.assertTrue(partial.exists())
            start = time.monotonic()
            proc.send_signal(signal.SIGTERM)
            self.assertEqual(proc.wait(timeout=20), 3)
            self.assertLess(time.monotonic() - start, 15)
            res = json.loads((term / "results/00000.json").read_text())
            self.assertEqual(res["outcome"], "INTERRUPTED_SIGTERM")
            self.assertTrue(res["scratch_deleted"])
            self.assertFalse((term / "scratch/00000").exists())


if __name__ == "__main__":
    unittest.main()
