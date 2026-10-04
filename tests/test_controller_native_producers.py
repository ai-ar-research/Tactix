#!/usr/bin/env python3
"""Focused frontend controls for native controller-producer selection."""

from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


def main():
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="tactix-controller-producers-") as tmp:
        work = Path(tmp)
        cnf = work / "sat.cnf"
        cnf.write_text("p cnf 1 1\n1 0\n")

        def run(*options):
            return subprocess.run(
                [str(binary), *options, str(cnf)], capture_output=True,
                text=True, timeout=30)

        help_result = subprocess.run(
            [str(binary), "--help"], capture_output=True, text=True, timeout=30)
        assert help_result.returncode == 0
        assert "--controller-native-producers=all|none|LIST" in help_result.stdout
        assert "--controller-families=LIST" in help_result.stdout

        for value in ("", "unknown", "gf3-affine,", "all,gf3-affine"):
            result = run("--controller-policy=7",
                         f"--controller-native-producers={value}")
            assert result.returncode == 1
            assert "invalid --controller-native-producers" in result.stderr

        without_policy = run("--controller-native-producers=tseitin-cycle")
        assert without_policy.returncode == 1
        assert "controller settings require it" in without_policy.stderr

        for value in ("", "unknown", "Decide", "Lucky,"):
            result = run("--controller-policy=7",
                         f"--controller-families={value}")
            assert result.returncode == 1
            assert "invalid --controller-families" in result.stderr

        family_without_policy = run(
            "--controller-families=TseitinCycleBreaking")
        assert family_without_policy.returncode == 1
        assert "controller settings require it" in family_without_policy.stderr

        trace = work / "selected.trace"
        selected = run(
            "--controller-policy=7",
            "--controller-native-producers=gf3-affine,direct-csp",
            "--controller-native-producers=gf3-gaussian,tseitin-cycle",
            f"--controller-trace={trace}")
        assert selected.returncode == 10, selected.stderr
        header = trace.read_text().splitlines()[:2]
        assert len(header) == 2
        configuration = shlex.split(header[1])[1]
        for family in ("Gf3AffineConsequence", "DirectCspSupport",
                       "Gf3GaussianConsequence", "TseitinCycleBreaking"):
            assert f'producer "{family}"' in configuration

        restricted_trace = work / "restricted.trace"
        restricted = run(
            "--controller-policy=7",
            "--controller-native-producers=all",
            "--controller-families=Gf3GaussianConsequence",
            "--controller-families=TseitinCycleBreaking",
            f"--controller-trace={restricted_trace}")
        assert restricted.returncode == 10, restricted.stderr
        restricted_configuration = shlex.split(
            restricted_trace.read_text().splitlines()[1])[1]
        assert 'family "Gf3GaussianConsequence"' in restricted_configuration
        assert 'family "TseitinCycleBreaking"' in restricted_configuration
        assert 'family "Gf3AffineConsequence"' not in restricted_configuration
        restricted_replay = run(
            f"--controller-replay={restricted_trace}",
            "--controller-native-producers=all",
            "--controller-families=Gf3GaussianConsequence,TseitinCycleBreaking")
        assert restricted_replay.returncode == 10, restricted_replay.stderr
        restricted_mismatch = run(
            f"--controller-replay={restricted_trace}",
            "--controller-native-producers=all")
        assert restricted_mismatch.returncode == 1
        assert "configuration differs" in restricted_mismatch.stderr

        with tempfile.NamedTemporaryFile(mode="w", dir=work,
                                         suffix=".actions") as actions:
            actions.write("online-strategy v1\n")
            actions.flush()
            conflicting = run(
                "--controller-policy=7",
                "--controller-families=Lucky",
                f"--controller-actions={actions.name}")
            assert conflicting.returncode == 1
            assert "select the same action collection" in conflicting.stderr

        replayed = run(
            f"--controller-replay={trace}",
            "--controller-native-producers=all")
        assert replayed.returncode == 10, replayed.stderr
        mismatch = run(f"--controller-replay={trace}")
        assert mismatch.returncode == 1
        assert "configuration differs" in mismatch.stderr

        none_trace = work / "none.trace"
        none = run("--controller-policy=7",
                   "--controller-native-producers=none",
                   f"--controller-trace={none_trace}")
        assert none.returncode == 10, none.stderr
        none_configuration = shlex.split(
            none_trace.read_text().splitlines()[1])[1]
        for family in ("Gf3AffineConsequence", "DirectCspSupport",
                       "Gf3GaussianConsequence", "TseitinCycleBreaking"):
            assert f'producer "{family}"' not in none_configuration

        print("controller native-producer CLI controls PASS")


if __name__ == "__main__":
    main()
