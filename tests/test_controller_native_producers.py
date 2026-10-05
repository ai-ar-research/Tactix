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
        nary_cnf = work / "nary.cnf"
        nary_cnf.write_text(
            "p cnf 9 15\n"
            "1 2 3 0\n-1 -2 0\n-1 -3 0\n-2 -3 0\n"
            "4 5 6 0\n-4 -5 0\n-4 -6 0\n-5 -6 0\n"
            "7 8 9 0\n-7 -8 0\n-7 -9 0\n-8 -9 0\n"
            "-1 -4 -7 0\n-2 -5 -8 0\n-3 -6 -9 0\n")

        def affine_profile(maximum_clauses=10000, *, rule="AffineConsequence",
                           version=2, granularity=3, instances=""):
            if version == 2:
                parameters = (
                    f"affine-consequence2({maximum_clauses},100000,1000000,"
                    "1000,10000,10000000,8,32,32,4,1000000,1000000,"
                    "10000000,10000000,32,32,10000,10000,3,100000,"
                    "10000,1000000)")
            else:
                parameters = (
                    f"affine-consequence1({maximum_clauses},100000,1000000,"
                    "1000,10000,10000000,8,16,16,4,1000000,1000000,"
                    "10000000,1)")
            return (
                "sequence-repeat=once:1 "
                f"step=1:{rule}:{granularity}:once:1:0:3:post(0):"
                f"rulework(2,1000000):material(0,0):{parameters}:"
                f"instances[{instances}]:grids[]:sr[]")

        profile = work / "gf2-affine.profile"
        profile.write_text(affine_profile())

        def run(*options):
            return subprocess.run(
                [str(binary), *options, str(cnf)], capture_output=True,
                text=True, timeout=30)

        help_result = subprocess.run(
            [str(binary), "--help"], capture_output=True, text=True, timeout=30)
        assert help_result.returncode == 0
        assert "--controller-native-producers=all|none|LIST" in help_result.stdout
        assert "--controller-gf2-affine-profile=FILE" in help_result.stdout
        assert "--controller-families=LIST" in help_result.stdout

        for value in ("", "unknown", "gf3-affine,", "all,gf3-affine"):
            result = run("--controller-policy=7",
                         f"--controller-native-producers={value}")
            assert result.returncode == 1
            assert "invalid --controller-native-producers" in result.stderr

        without_policy = run("--controller-native-producers=tseitin-cycle")
        assert without_policy.returncode == 1
        assert "controller settings require it" in without_policy.stderr

        profile_without_policy = run(
            f"--controller-gf2-affine-profile={profile}")
        assert profile_without_policy.returncode == 1
        assert "controller settings require it" in profile_without_policy.stderr

        invalid_profiles = {
            "wrong-rule": affine_profile(rule="Walk"),
            "v1": affine_profile(version=1),
            "payload": affine_profile(granularity=0, instances="1"),
        }
        for name, contents in invalid_profiles.items():
            path = work / f"{name}.profile"
            path.write_text(contents)
            result = run(
                "--controller-policy=7",
                "--controller-native-producers=gf2-affine",
                f"--controller-gf2-affine-profile={path}")
            assert result.returncode == 1
            assert "invalid GF2 affine producer profile" in result.stderr

        disabled_profile = run(
            "--controller-policy=7",
            f"--controller-gf2-affine-profile={profile}")
        assert disabled_profile.returncode == 1
        assert "requires the native producer" in disabled_profile.stderr

        excluded_profile = run(
            "--controller-policy=7",
            "--controller-native-producers=gf2-affine",
            "--controller-families=Lucky",
            f"--controller-gf2-affine-profile={profile}")
        assert excluded_profile.returncode == 1
        assert "outside the configured action collection" in excluded_profile.stderr

        static_actions = work / "static-affine.actions"
        static_actions.write_text(affine_profile())
        static_profile = run(
            "--controller-policy=7",
            "--controller-native-producers=gf2-affine",
            f"--controller-actions={static_actions}",
            f"--controller-gf2-affine-profile={profile}")
        assert static_profile.returncode == 1
        assert "conflicts with a static action offer" in static_profile.stderr

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
            "--controller-native-producers=gf2-affine,gf3-affine,direct-csp,nary-csp",
            "--controller-native-producers=gf3-gaussian,tseitin-cycle",
            f"--controller-trace={trace}")
        assert selected.returncode == 10, selected.stderr
        header = trace.read_text().splitlines()[:2]
        assert len(header) == 2
        configuration = shlex.split(header[1])[1]
        for family in ("AffineConsequence", "Gf3AffineConsequence", "DirectCspSupport",
                       "NaryCspPrefix",
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

        profile_trace = work / "profile.trace"
        profiled = run(
            "--controller-policy=7",
            "--controller-native-producers=gf2-affine",
            "--controller-families=AffineConsequence",
            f"--controller-gf2-affine-profile={profile}",
            f"--controller-trace={profile_trace}")
        assert profiled.returncode == 10, profiled.stderr
        profiled_configuration = shlex.split(
            profile_trace.read_text().splitlines()[1])[1]
        assert 'producer "AffineConsequence"' in profiled_configuration
        assert 'offered "AffineConsequence"' not in profiled_configuration
        assert "affine-consequence2(10000," in profiled_configuration

        same_profile_replay = run(
            f"--controller-replay={profile_trace}",
            "--controller-native-producers=gf2-affine",
            "--controller-families=AffineConsequence",
            f"--controller-gf2-affine-profile={profile}")
        assert same_profile_replay.returncode == 10, same_profile_replay.stderr
        missing_profile_replay = run(
            f"--controller-replay={profile_trace}",
            "--controller-native-producers=gf2-affine",
            "--controller-families=AffineConsequence")
        assert missing_profile_replay.returncode == 1
        assert "configuration differs" in missing_profile_replay.stderr
        different_profile = work / "gf2-affine-different.profile"
        different_profile.write_text(affine_profile(maximum_clauses=9999))
        different_profile_replay = run(
            f"--controller-replay={profile_trace}",
            "--controller-native-producers=gf2-affine",
            "--controller-families=AffineConsequence",
            f"--controller-gf2-affine-profile={different_profile}")
        assert different_profile_replay.returncode == 1
        assert "configuration differs" in different_profile_replay.stderr

        none_trace = work / "none.trace"
        none = run("--controller-policy=7",
                   "--controller-native-producers=none",
                   f"--controller-trace={none_trace}")
        assert none.returncode == 10, none.stderr
        none_configuration = shlex.split(
            none_trace.read_text().splitlines()[1])[1]
        for family in ("AffineConsequence", "Gf3AffineConsequence", "DirectCspSupport",
                       "NaryCspPrefix",
                       "Gf3GaussianConsequence", "TseitinCycleBreaking"):
            assert f'producer "{family}"' not in none_configuration

        reached = False
        last_effective = None
        for seed in range(1, 129):
            provenance = work / f"nary-{seed}.jsonl"
            effective = subprocess.run(
                [str(binary), f"--controller-policy={seed}",
                 "--controller-native-producers=nary-csp",
                 "--controller-families=NaryCspPrefix",
                 f"--structural-provenance={provenance}", str(nary_cnf)],
                capture_output=True, text=True, timeout=30)
            last_effective = effective
            assert effective.returncode == 10, effective.stderr
            lines = effective.stdout.splitlines()
            rule_line = next(
                (line for line in lines if line.startswith(
                    "c controller-rule NaryCspPrefix ")), "")
            producer_line = next(
                (line for line in lines if line.startswith(
                    "c controller-producer NaryCspPrefix ")), "")
            action_line = next(
                (line for line in lines if line.startswith("c nary-csp ")), "")
            rule_counts = dict(field.split("=", 1) for field in
                               rule_line.split()[3:] if "=" in field)
            producer_counts = dict(field.split("=", 1) for field in
                                   producer_line.split()[3:] if "=" in field)
            action_attempts = int(action_line.split()[2]) if action_line else 0
            if (rule_counts and producer_counts and
                    int(rule_counts["requests"]) > 0 and
                    int(rule_counts["effective"]) > 0 and
                    int(producer_counts["calls"]) > 0 and
                    int(producer_counts["offers"]) > 0 and
                    action_attempts > 0):
                assert "c ERROR: controller selection:" not in effective.stderr
                assert provenance.stat().st_size > 0
                reached = True
                break
        assert reached, (
            "nary-csp CLI producer never supplied an effective action\n"
            f"stdout:\n{last_effective.stdout}\nstderr:\n{last_effective.stderr}")

        print("controller native-producer CLI controls PASS")


if __name__ == "__main__":
    main()
