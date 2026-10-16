"""The paper's experiments: instances, limits, and the command of every configuration.

Each configuration was copied from the archived campaign in paper/logs/, with
binaries replaced by their paper/run/tools/ builds. In a command, {cnf} is the
decompressed formula, {proof} the certificate path and {proof_dir} a directory
for solvers that choose their own certificate name (they write proof.out).

Proof kinds decide how an UNSAT answer is checked:
  tactix   Tactix's own certificate (DSR, DRAT-compatible DSR, or PB), routed by
           its content: dsr-trim + Trestle, drat-trim + CakeLPR, or VeriPB + CakePB
  drat     drat-trim (to LRAT) + CakeLPR
  satsuma  Satsuma's DSR: dsr-trim, dsr-compress -d, Trestle
  pbp      VeriPB elaboration + CakePB
  none     no certificate (UNSAT is not checked)
"""

T = "tools/tactix"  # Tactix builds, one directory per paper-* tag
S = "tools/solvers"

KISSAT = {"command": [f"{S}/kissat-4.0.4", "{cnf}", "{proof}"], "proof": "drat"}
REFERENCES = {
    "Satsuma+Kissat": {"command": [f"{S}/satsuma-kissat/run_satsuma_kissat.sh", "{cnf}", "{proof_dir}"],
                       "proof": "satsuma"},
    "Kissat 4.0.4": KISSAT,
    "Kissat-sc2024": {"command": [f"{S}/kissat-sc2024", "{cnf}", "{proof}"], "proof": "drat"},
    "AE-Kissat-MAB": {"command": [f"{S}/ae-kissat-mab", "{cnf}", "{proof}"], "proof": "drat"},
    "LymphoSAT": {"command": [f"{S}/lymphosat/run.sh", "{cnf}", "{proof_dir}"], "proof": "pbp"},
}

# The cruxmiter strategy as evaluated: the public-interface program plus the
# environment settings it was run with.
CRUX_ENVIRONMENT = {
    "TACTIX_FORCE_MODE": "stable", "TACTIX_REORDER": "0", "TACTIX_NO_BVE": "1",
    "TACTIX_CONFLICT_REUSE": "0", "TACTIX_SLOT_PERMILLE": "Vivify:200", "TACTIX_NO_FACTOR": "1",
    "TACTIX_NO_SWEEP": "1", "TACTIX_SLOT_DISABLE": "Backbone", "TACTIX_CRUX_CS_ROUNDS": "4",
}

EXPERIMENTS = {
    "smoke": {
        "description": "The example formulas: every solver path and proof route, in about a minute",
        "instances": {"kind": "files", "glob": "examples/cnf/*.cnf"},
        "limits": {"seconds": 300, "memory_gib": 4, "cpus": 1, "check_seconds": 600},
        "configurations": {
            "Tactix": {"command": [f"{T}/paper-tierd/tactix", "{cnf}", "{proof}"], "proof": "tactix"},
            "Kissat 4.0.4": KISSAT,
        },
    },
    "tierd": {
        "description": "SAT Competition 2024-2026 main tracks (1,166 formulas), six solvers",
        "instances": {"kind": "gbd", "list": "paper/benchmarks/tierd.sources",
                      "years": "paper/benchmarks/tierd-years.tsv"},
        "limits": {"seconds": 5000, "memory_gib": 32, "cpus": 4, "check_seconds": 28800},
        "configurations": {
            "Tactix": {"command": [f"{T}/paper-tierd/tactix", "--auto-proof", "{cnf}", "{proof}"],
                       "proof": "tactix"},
            **REFERENCES,
        },
    },
    "milestones": {
        "description": "Tactix at the first commit of each development milestone, on Tier D, without proofs",
        "instances": {"kind": "gbd", "list": "paper/benchmarks/tierd.sources",
                      "years": "paper/benchmarks/tierd-years.tsv"},
        "limits": {"seconds": 5000, "memory_gib": 32, "cpus": 4, "check_seconds": 28800},
        "configurations": {
            **{f"Milestone {m}": {"command": [f"{T}/paper-milestone-{m.lower()}/tactix", "{cnf}"],
                                  "proof": "none", "commit_tag": f"paper-milestone-{m.lower()}"}
               for m in "ABCDE"},
            "Current": {"command": [f"{T}/paper-tierd/tactix", "--auto-proof", "{cnf}", "{proof}"],
                        "proof": "tactix", "commit_tag": "paper-tierd"},
        },
    },
    "interface": {
        "description": "The default strategy through tactix.h against the C++ build, on four sets",
        "instances": {"kind": "sets", "sets": {
            "Tier A": {"kind": "archive", "archive": "paper/benchmarks/tier-a.tar.xz",
                       "list": "paper/benchmarks/tier-a.list",
                       "limits": {"seconds": 30, "cpus": 8}},
            "B28+C5": {"kind": "gbd", "list": "paper/benchmarks/interface-b28c5.sources"},
            "Rank 1-50": {"kind": "gbd", "list": "paper/benchmarks/interface-rank1-50.sources"},
            "WV100": {"kind": "gbd", "list": "paper/benchmarks/interface-wv100.sources"},
        }},
        "limits": {"seconds": 1200, "memory_gib": 32, "cpus": 12, "check_seconds": 1800},
        "configurations": {
            "C interface": {"command": [f"{T}/paper-interface/tactix-public", "--auto-proof", "{cnf}",
                                        "{proof}"], "proof": "tactix"},
            "C++": {"command": [f"{T}/paper-tierd/tactix", "--auto-proof", "{cnf}", "{proof}"],
                    "proof": "tactix"},
        },
    },
    "cruxmiter": {
        "description": "Case study: the cruxmiter strategy on the ten size-32 cruxmiter formulas",
        "instances": {"kind": "files", "glob": "paper/benchmarks/cruxmiter/cruxmiter32seed*.cnf"},
        "limits": {"seconds": 43200, "memory_gib": 48, "cpus": 12, "check_seconds": 259200},
        "configurations": {
            "Default Tactix": {"command": [f"{T}/paper-cruxmiter/tactix", "--dsr", "{cnf}", "{proof}"],
                               "proof": "tactix"},
            "Public policy": {"command": [f"{T}/paper-cruxmiter/tactix-cruxmiter-policy",
                                          "--profile=backbone", "--drat", "{cnf}", "{proof}"],
                              "environment": CRUX_ENVIRONMENT, "proof": "drat"},
            "Kissat 4.0.4": KISSAT,
        },
    },
    "chvatal": {
        "description": "Case study: the Chvatal strategy on 23 subproblems of case n = 7",
        "instances": {"kind": "files", "glob": "paper/benchmarks/chvatal/*.cnf.xz",
                      "selection": "paper/logs/chvatal/paper-hard.json"},
        "limits": {"seconds": 5000, "memory_gib": 16, "cpus": 4, "check_seconds": 7200},
        "configurations": {
            "Chvatal Tactix": {"command": [f"{T}/paper-chvatal/tactix-chvatal-policy", "--twins", "--dsr",
                                           "{cnf}", "{proof}"], "proof": "tactix"},
            "Chvatal Tactix (stable)": {"command": [f"{T}/paper-chvatal/tactix-chvatal-policy", "--twins",
                                                    "--dsr", "{cnf}", "{proof}"],
                                        "environment": {"TACTIX_FORCE_MODE": "stable"}, "proof": "tactix"},
            "Default Tactix": {"command": [f"{T}/paper-chvatal-default/tactix", "--dsr", "{cnf}", "{proof}"],
                               "proof": "tactix"},
            "Kissat 4.0.4": KISSAT,
            "Satsuma+Kissat": REFERENCES["Satsuma+Kissat"],
        },
    },
}

# Paper commits, created as git tags by the artifact; setup.sh builds each one.
TAGS = {
    "paper-tierd": "0f7c10174",
    "paper-interface": "984a72441",
    "paper-milestone-a": "dcc3ecbd6",
    "paper-milestone-b": "546298924",
    "paper-milestone-c": "6f44f23d6",
    "paper-milestone-d": "f50e46c0c",
    "paper-milestone-e": "0a9063387",
    "paper-cruxmiter": "5d2bab322",
    "paper-chvatal": "4dc8e673d",
    "paper-chvatal-default": "903ec8bb4",
}
