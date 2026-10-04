#!/usr/bin/env python3
"""Self-controls for concurrent fuzz artifact namespace isolation."""

import concurrent.futures
import hashlib
import json
import pathlib
import sys
import tempfile


HERE = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HERE))

from artifact_layout import output_dir  # noqa: E402
import fuzz  # noqa: E402


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_leg(base, leg, payload):
    outdir = pathlib.Path(output_dir(base, "slurm-42", leg))
    work = outdir / "work"
    work.mkdir(parents=True)
    case = work / "case.cnf"
    case.write_text(payload)

    failed = fuzz.CaseOutcome()
    failed.verdict = "UNKNOWN"
    failed.solver_out = f"stdout-{leg}\n"
    failed.solver_err = f"stderr-{leg}\n"
    failed.fail("crash", f"failure-{leg}")
    bundle = pathlib.Path(fuzz.save_bundle(
        str(outdir), 7, 101, "control", str(case), failed,
        [f"solver-{leg}"], None, 1.0, False, False))

    timed_out = fuzz.CaseOutcome()
    timed_out.verdict = "TIMEOUT"
    retained = pathlib.Path(fuzz.save_nonterminating(
        str(outdir), 7, 101, "control", str(case), timed_out,
        [f"solver-{leg}"], 1.0))
    summary = outdir / "nonterminating.json"
    summary.write_text(json.dumps({"leg": leg, "index": 7}))
    return outdir, bundle, retained, summary


def main():
    with tempfile.TemporaryDirectory(prefix="fuzz-artifact-isolation-") as tmp:
        base = pathlib.Path(tmp)
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            futures = [
                pool.submit(write_leg, base, "default", "p cnf 1 1\n1 0\n"),
                pool.submit(write_leg, base, "random", "p cnf 1 1\n-1 0\n"),
            ]
            default, random = [future.result() for future in futures]

        assert default[0] != random[0]
        assert default[1].name == random[1].name == "000007-crash-seed101"
        assert default[2].name == random[2].name == "000007-seed101"

        for result, leg in ((default, "default"), (random, "random")):
            outdir, bundle, retained, summary = result
            assert bundle.parent == outdir
            assert retained.parent == outdir / "nonterminating"
            assert json.loads((bundle / "meta.json").read_text())["solver"] \
                == [f"solver-{leg}"]
            assert json.loads((retained / "meta.json").read_text())["solver"] \
                == [f"solver-{leg}"]
            assert f"solver-{leg}" in (bundle / "repro.sh").read_text()
            assert str(retained / "case.cnf") in \
                (retained / "repro.sh").read_text()
            assert json.loads(summary.read_text()) == {"leg": leg, "index": 7}

        assert digest(default[1] / "case.cnf") != \
            digest(random[1] / "case.cnf")
        assert digest(default[2] / "case.cnf") != \
            digest(random[2] / "case.cnf")

        try:
            output_dir(base, "../escape", "default")
        except ValueError:
            pass
        else:
            raise AssertionError("unsafe run id was accepted")

    print("artifact-isolation controls: PASS")


if __name__ == "__main__":
    main()
