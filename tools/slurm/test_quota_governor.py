"""Hysteresis and ownership for quota_governor.py; never touches a real job."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "quota_governor", Path(__file__).with_name("quota_governor.py"))
qg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qg)

TABLE = """NAME                   USED  QUOTA  OBJUSED  OBJQUOTA
hwu2                   900G     1T    4.27M      none
hwu                    526G     1T    4.27M      none
"""


class FakeSlurm:
    """squeue/scontrol stand-in with array tasks of owned and foreign jobs."""

    def __init__(self, pending, live=("100", "200")):
        self.pending = dict(pending)
        self.live = set(live)
        self.calls = []

    def __call__(self, cmd, **kw):
        self.calls.append(cmd)
        out, rc, err = "", 0, ""
        if cmd[0] == "squeue" and "PD" in cmd:
            ids = cmd[cmd.index("-j") + 1].split(",")
            out = "".join(f"{t}|{r}\n" for t, r in self.pending.items()
                          if t.split("_")[0] in ids)
        elif cmd[0] == "squeue":
            job = cmd[cmd.index("-j") + 1]
            if job in self.live:
                out = job + "_1\n"
            else:
                rc, err = 1, "slurm_load_jobs error: Invalid job id specified"
        elif cmd[0] == "scontrol":
            for task in cmd[2].split(","):
                if cmd[1] == "hold":
                    self.pending[task] = "JobHeldUser"
                else:
                    self.pending[task] = "None"
        return subprocess.CompletedProcess(cmd, rc, out, err)


def args(**kw):
    base = dict(user="hwu", hold_above=750.0, release_below=700.0)
    base.update(kw)
    return argparse.Namespace(**base)


class Parsing(unittest.TestCase):
    def test_sizes(self):
        self.assertEqual(qg.parse_size("526G"), 526.0)
        self.assertEqual(qg.parse_size("1T"), 1024.0)
        self.assertEqual(qg.parse_size("512M"), 0.5)
        self.assertIsNone(qg.parse_size("none"))

    def test_exact_user_row(self):
        self.assertEqual(qg.usage_from_table(TABLE, "hwu"), 526.0)
        self.assertIsNone(qg.usage_from_table(TABLE, "nobody"))

    def test_quota_fallback(self):
        run = lambda cmd, **kw: subprocess.CompletedProcess(cmd, 0, TABLE, "")
        self.assertEqual(qg.read_usage("hwu", zfs_file="/nonexistent", run=run),
                         (526.0, "quota -s"))


class Hysteresis(unittest.TestCase):
    def test_sequence(self):
        state, actions = "released", []
        for usage in (600, 749, 751, 740, 705, 700, 699, 720, 751, None, 650):
            state, action = qg.decide(state, usage, 750, 700)
            actions.append((usage, state, action))
        self.assertEqual(actions, [
            (600, "released", None), (749, "released", None),
            (751, "held", "hold"), (740, "held", "hold-new"),
            (705, "held", "hold-new"), (700, "held", "hold-new"),
            (699, "released", "release"), (720, "released", None),
            (751, "held", "hold"), (None, "held", "hold-new"),
            (650, "released", "release")])

    def test_unknown_usage_never_releases(self):
        self.assertEqual(qg.decide("held", None, 750, 700), ("held", "hold-new"))
        self.assertEqual(qg.decide("released", None, 750, 700), ("released", None))


class Cycle(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        (self.dir / "jobs.json").write_text(json.dumps(
            {"jobs": [{"id": "100"}, {"id": "200"}], "expect_more": True}))

    def tearDown(self):
        self.tmp.cleanup()

    def run_cycle(self, slurm, usage):
        return qg.cycle(self.dir, args(), run=slurm,
                        usage_reader=lambda: (usage, "test"), now="t")

    def test_holds_only_owned_pending_and_releases_only_own_holds(self):
        slurm = FakeSlurm({"100_5": "JobArrayTaskLimit", "100_6": "Priority",
                           "200_1": "JobHeldUser", "300_1": "Priority"})
        self.assertFalse(self.run_cycle(slurm, 800))
        holds = [c for c in slurm.calls if c[:2] == ["scontrol", "hold"]]
        self.assertEqual(holds, [["scontrol", "hold", "100_5,100_6"]])
        self.assertFalse(any(c[0] == "scancel" or "suspend" in c for c in slurm.calls))
        # newly pending owned task while held is held too
        slurm.pending["200_2"] = "Priority"
        self.run_cycle(slurm, 720)
        self.assertIn(["scontrol", "hold", "200_2"], slurm.calls)
        slurm.calls.clear()
        self.run_cycle(slurm, 699)
        releases = [c for c in slurm.calls if c[:2] == ["scontrol", "release"]]
        self.assertEqual(len(releases), 1)
        self.assertEqual(sorted(releases[0][2].split(",")), ["100_5", "100_6", "200_2"])
        # 200_1 was held by someone else and stays held
        self.assertEqual(slurm.pending["200_1"], "JobHeldUser")
        log = [json.loads(l) for l in (self.dir / "governor.log.jsonl").read_text().splitlines()]
        self.assertEqual([r["action"] for r in log], ["hold", "hold-new", "release"])

    def test_exit_only_when_all_gone_and_no_more_expected(self):
        slurm = FakeSlurm({}, live=())
        self.assertFalse(self.run_cycle(slurm, 500))  # expect_more still true
        (self.dir / "jobs.json").write_text(json.dumps(
            {"jobs": [{"id": "100"}, {"id": "200"}], "expect_more": False}))
        self.assertTrue(self.run_cycle(slurm, 500))
        slurm.live = {"200"}
        self.assertFalse(self.run_cycle(slurm, 500))


if __name__ == "__main__":
    unittest.main()
