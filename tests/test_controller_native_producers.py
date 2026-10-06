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
        search_cnf = work / "search.cnf"
        search_cnf.write_text("p cnf 2 1\n1 2 0\n")
        nary_cnf = work / "nary.cnf"
        nary_cnf.write_text(
            "p cnf 9 15\n"
            "1 2 3 0\n-1 -2 0\n-1 -3 0\n-2 -3 0\n"
            "4 5 6 0\n-4 -5 0\n-4 -6 0\n-5 -6 0\n"
            "7 8 9 0\n-7 -8 0\n-7 -9 0\n-8 -9 0\n"
            "-1 -4 -7 0\n-2 -5 -8 0\n-3 -6 -9 0\n")

        def affine_profile(maximum_clauses=10000, *, rule="AffineConsequence",
                           version=2, granularity=3, instances="", advice=False):
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
            result = (
                "sequence-repeat=once:1 "
                f"step=1:{rule}:{granularity}:once:1:0:3:post(0):"
                f"rulework(2,1000000):material(0,0):{parameters}:"
                f"instances[{instances}]:grids[]:sr[]")
            if advice:
                result += (
                    " step=2:SemanticVariableAdvice:3:once:1:0:1:post(0):"
                    "rulework(2,1):material(0,0):"
                    "semantic-variable-advice1(64,4096,2,7):"
                    "instances[]:grids[]:sr[]")
            return result

        profile = work / "gf2-affine.profile"
        profile.write_text(affine_profile())

        def run(*options, source=None):
            return subprocess.run(
                [str(binary), *options, str(source or cnf)], capture_output=True,
                text=True, timeout=30)

        help_result = subprocess.run(
            [str(binary), "--help"], capture_output=True, text=True, timeout=30)
        assert help_result.returncode == 0
        assert "--controller-native-producers=all|none|LIST" in help_result.stdout
        assert "--controller-gf2-affine-profile=FILE" in help_result.stdout
        assert "--controller-startup-prefix=FILE" in help_result.stdout
        assert "--controller-families=LIST" in help_result.stdout
        assert "--controller-search-configurations=all|LIST" in help_result.stdout
        assert "gf3-general-row" in help_result.stdout

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

        startup_prefix = work / "startup-prefix.program"
        startup_prefix.write_text(
            "sequence-repeat=once:1 "
            "step=71:Walk:3:once:1:0:1:post(0):"
            "rulework(2,1000000):material(0,0):"
            "walk(5,7):instances[]:grids[]\n")
        startup_without_policy = run(
            f"--controller-startup-prefix={startup_prefix}")
        assert startup_without_policy.returncode == 1
        assert "controller settings require it" in startup_without_policy.stderr

        startup_trace = work / "startup.trace"
        startup_selected = run(
            "--controller-policy=7", "--controller-families=Walk",
            f"--controller-startup-prefix={startup_prefix}",
            f"--controller-trace={startup_trace}", source=search_cnf)
        assert startup_selected.returncode == 10, startup_selected.stderr
        startup_lines = startup_trace.read_text().splitlines()
        startup_configuration = shlex.split(startup_lines[1])[1]
        assert "startup-prefix" in startup_configuration
        assert any(line.startswith("request ") and
                   "step=71:Walk:" in line for line in startup_lines)
        startup_replayed = run(
            f"--controller-replay={startup_trace}",
            "--controller-families=Walk",
            f"--controller-startup-prefix={startup_prefix}", source=search_cnf)
        assert startup_replayed.returncode == 10, startup_replayed.stderr

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

        for value in ("", "unknown", "vmtf/adaptive-glue/saved,",
                      "vmtf/adaptive-glue/saved,vmtf/adaptive-glue/saved"):
            result = run("--controller-policy=7",
                         f"--controller-search-configurations={value}")
            assert result.returncode == 1
            assert "invalid --controller-search-configurations" in result.stderr
        search_without_policy = run(
            "--controller-search-configurations=vmtf/adaptive-glue/saved")
        assert search_without_policy.returncode == 1
        assert "controller settings require it" in search_without_policy.stderr

        search_trace = work / "search.trace"
        search_selected = run(
            "--controller-policy=17",
            "--controller-families=Walk",
            "--controller-search-configurations="
            "vmtf/adaptive-glue/focused-rotation",
            f"--controller-trace={search_trace}", source=search_cnf)
        assert search_selected.returncode == 10, search_selected.stderr
        search_configuration = shlex.split(
            search_trace.read_text().splitlines()[1])[1]
        assert ('search-configuration "vmtf/adaptive-glue/focused-rotation"'
                in search_configuration)
        requests = [line for line in search_trace.read_text().splitlines()
                    if line.startswith("request ")]
        assert any(" ResumeSearch vmtf/adaptive-glue/focused-rotation" in line
                   for line in requests)
        assert not any(" ResumeSearch " in line and
                       "vmtf/adaptive-glue/focused-rotation" not in line
                       for line in requests)
        search_replayed = run(
            f"--controller-replay={search_trace}",
            "--controller-families=Walk",
            "--controller-search-configurations="
            "vmtf/adaptive-glue/focused-rotation", source=search_cnf)
        assert search_replayed.returncode == 10, search_replayed.stderr
        search_mismatch = run(
            f"--controller-replay={search_trace}",
            "--controller-families=Walk", source=search_cnf)
        assert search_mismatch.returncode == 1
        assert "configuration differs" in search_mismatch.stderr

        accumulated_search_trace = work / "accumulated-search.trace"
        accumulated_search = run(
            "--controller-policy=17",
            "--controller-search-configurations=vmtf/adaptive-glue/saved",
            "--controller-search-configurations=evsids/reluctant/target",
            f"--controller-trace={accumulated_search_trace}")
        assert accumulated_search.returncode == 10, accumulated_search.stderr
        accumulated_configuration = shlex.split(
            accumulated_search_trace.read_text().splitlines()[1])[1]
        assert ('search-configuration "vmtf/adaptive-glue/saved"'
                in accumulated_configuration)
        assert ('search-configuration "evsids/reluctant/target"'
                in accumulated_configuration)

        trace = work / "selected.trace"
        selected = run(
            "--controller-policy=7",
            "--controller-native-producers=gf2-affine,gf3-affine,direct-csp,nary-csp,nary-csp-equality",
            "--controller-native-producers=gf3-gaussian,gf3-general-row,tseitin-cycle",
            f"--controller-trace={trace}")
        assert selected.returncode == 10, selected.stderr
        header = trace.read_text().splitlines()[:2]
        assert len(header) == 2
        configuration = shlex.split(header[1])[1]
        for family in ("AffineConsequence", "Gf3AffineConsequence", "DirectCspSupport",
                       "NaryCspPrefix", "NaryCspEquality",
                       "Gf3GaussianConsequence", "Gf3GeneralRowConsequence",
                       "TseitinCycleBreaking"):
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

        advised_profile = work / "gf2-affine-advised.profile"
        advised_profile.write_text(affine_profile(advice=True))
        advised_trace = work / "advised.trace"
        advised = run(
            "--controller-policy=7",
            "--controller-native-producers=gf2-affine",
            "--controller-families=AffineConsequence,Propagate,"
            "SemanticVariableAdvice",
            f"--controller-gf2-affine-profile={advised_profile}",
            f"--controller-trace={advised_trace}")
        assert advised.returncode == 10, advised.stderr
        advised_configuration = shlex.split(
            advised_trace.read_text().splitlines()[1])[1]
        assert "semantic-variable-advice1(64,4096,2,7)" in advised_configuration
        advised_replayed = run(
            f"--controller-replay={advised_trace}",
            "--controller-native-producers=gf2-affine",
            "--controller-families=AffineConsequence,Propagate,"
            "SemanticVariableAdvice",
            f"--controller-gf2-affine-profile={advised_profile}")
        assert advised_replayed.returncode == 10, advised_replayed.stderr
        advice_missing = run(
            f"--controller-replay={advised_trace}",
            "--controller-native-producers=gf2-affine",
            "--controller-families=AffineConsequence,Propagate,"
            "SemanticVariableAdvice",
            f"--controller-gf2-affine-profile={profile}")
        assert advice_missing.returncode == 1
        assert "configuration differs" in advice_missing.stderr

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
                       "NaryCspPrefix", "NaryCspEquality",
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
