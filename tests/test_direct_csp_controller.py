#!/usr/bin/env python3
"""Focused native DirectCSP controller profile and replay boundary."""

from pathlib import Path
import subprocess
import sys
import tempfile


def profile(*, direct_post=0, propagation_post=1):
    parameters = (
        "direct-csp-support1(100000,1000000,10000000,64,64,65,"
        "10000000,100000,1000000,10000000,1,2147483647,"
        "100000,100000000,1)")
    return (
        "sequence-repeat=count:2 "
        "step=1:DirectCspSupport:3:once:1:0:3:"
        f"post({direct_post}):rulework(2,10000000):material(0,0):"
        f"{parameters}:instances[]:grids[]:sr[] "
        "step=2:Propagate:3:once:1:2:1:"
        f"post({propagation_post}):rulework(2,1):material(0,0):"
        "inherit:instances[]:grids[]:sr[] "
        "step=3:SemanticVariableAdvice:3:once:1:0:1:post(0):"
        "rulework(2,1):material(0,0):"
        "semantic-variable-advice1(64,4096,2,7):"
        "instances[]:grids[]:sr[]")


def main():
    binary = Path(sys.argv[1]).resolve()
    checkmodel = Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="tactix-direct-csp-controller-") as tmp:
        work = Path(tmp)
        cnf = work / "direct-csp.cnf"
        cnf.write_text(
            "p cnf 7 16\n"
            "1 2 3 0\n-1 -2 0\n-1 -3 0\n-2 -3 0\n"
            "4 5 6 7 0\n-4 -5 0\n-4 -6 0\n-4 -7 0\n"
            "-5 -6 0\n-5 -7 0\n-6 -7 0\n"
            "-1 -7 0\n-2 -4 0\n-2 -6 0\n-2 -7 0\n-3 -5 0\n")
        selected_profile = work / "direct-csp.profile"
        selected_profile.write_text(profile())

        def run(*options):
            return subprocess.run(
                [str(binary), *options, str(cnf)], capture_output=True,
                text=True, timeout=30)

        help_result = subprocess.run(
            [str(binary), "--help"], capture_output=True, text=True, timeout=30)
        assert help_result.returncode == 0
        assert "--controller-direct-csp-profile=FILE" in help_result.stdout

        without_policy = run(
            f"--controller-direct-csp-profile={selected_profile}")
        assert without_policy.returncode == 1
        assert "controller settings require it" in without_policy.stderr

        disabled = run("--controller-policy=5",
                       f"--controller-direct-csp-profile={selected_profile}")
        assert disabled.returncode == 1
        assert "requires the native producer" in disabled.stderr

        invalid_profile = work / "invalid.profile"
        invalid_profile.write_text(profile(direct_post=1))
        invalid = run(
            "--controller-policy=5",
            "--controller-native-producers=direct-csp",
            f"--controller-direct-csp-profile={invalid_profile}")
        assert invalid.returncode == 1
        assert "no bundled follow-up" in invalid.stderr

        trace = work / "direct-csp.trace"
        records = work / "direct-csp.records.jsonl"
        selected = run(
            "--controller-policy=5",
            "--controller-native-producers=direct-csp",
            "--controller-families=DirectCspSupport,Propagate,"
            "SemanticVariableAdvice",
            f"--controller-direct-csp-profile={selected_profile}",
            f"--controller-trace={trace}",
            f"--structural-provenance={records}")
        assert selected.returncode == 10, selected.stderr
        output = work / "selected.out"
        output.write_text(selected.stdout)
        checked = subprocess.run(
            [str(checkmodel), str(cnf), str(output)], capture_output=True,
            text=True, timeout=30)
        assert checked.returncode == 0, checked.stderr
        # Startup solving may decide this tiny model before controller entry.
        # The C++ controller test drives D -> P -> A -> distinct D stepwise;
        # this process-level fixture owns option parsing and whole-run replay.
        assert trace.stat().st_size > 0
        assert records.stat().st_size > 0

        replay_records = work / "direct-csp-replay.records.jsonl"
        replayed = run(
            f"--controller-replay={trace}",
            "--controller-native-producers=direct-csp",
            "--controller-families=DirectCspSupport,Propagate,"
            "SemanticVariableAdvice",
            f"--controller-direct-csp-profile={selected_profile}",
            f"--structural-provenance={replay_records}")
        assert replayed.returncode == 10, replayed.stderr
        replay_output = work / "replayed.out"
        replay_output.write_text(replayed.stdout)
        replay_checked = subprocess.run(
            [str(checkmodel), str(cnf), str(replay_output)],
            capture_output=True, text=True, timeout=30)
        assert replay_checked.returncode == 0, replay_checked.stderr
        assert replay_records.read_bytes() == records.read_bytes()

        print("direct CSP controller profile/replay/model PASS")


if __name__ == "__main__":
    main()
