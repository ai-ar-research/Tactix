"""The paper's experiments: instances, limits, and the command of every configuration.

Each configuration was copied from its campaign manifest in paper/logs/, with
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

Every experiment can be run under three profiles that differ only in the
instances and the resource limits (bench.py --profile):
  full     the paper's instance sets and limits (the default)
  reduced  the artifact-evaluation subsets (paper/benchmarks/subsets/*-reduced.txt)
           with smaller limits, sized for 4 cores and 16 GB
  smoke    a few quick instances of the same subsets with short limits, to
           check that every solver and checker runs on each experiment's
           benchmarks
A profile may also replace the limits of an instance set ("sets") and run
only some configurations; the reduced and smoke milestones take the current
version's runs from the tierd experiment, as the paper did.
"""

SUB = "paper/benchmarks/subsets"
T = "tools/tactix"  # Tactix builds, one directory per entry of BUILDS
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

# Limits of the reduced and smoke profiles. At most three runs execute at the
# same time (bench.py --jobs 3), so 3 x 4 GiB fit in 16 GB.
REDUCED_MEMORY_GIB = 4
SMOKE = {"seconds": 60, "memory_gib": 4, "cpus": 1, "check_seconds": 120}

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
        "limits": {"seconds": 5000, "memory_gib": 32, "cpus": 4, "check_seconds": 28800,
                   "check_memory_gib": 64},
        "profiles": {
            "reduced": {"subset": f"{SUB}/tierd-reduced.txt",
                        "limits": {"seconds": 600, "memory_gib": REDUCED_MEMORY_GIB, "cpus": 1,
                                   "check_seconds": 600}},
            "smoke": {"subset": f"{SUB}/tierd-smoke.txt", "limits": SMOKE},
        },
        "configurations": {
            "Tactix": {"command": [f"{T}/paper-tierd/tactix", "--auto-proof", "{cnf}", "{proof}"],
                       "proof": "tactix"},
            **REFERENCES,
        },
    },
    "milestones": {
        "description": "Tactix at the first commit of each development milestone, on the tierd formulas, without proofs",
        "instances": {"kind": "gbd", "list": "paper/benchmarks/tierd.sources",
                      "years": "paper/benchmarks/tierd-years.tsv"},
        "limits": {"seconds": 5000, "memory_gib": 32, "cpus": 4, "check_seconds": 28800},
        "profiles": {
            # The tierd formulas and limits; the current version's runs are
            # the tierd experiment's Tactix runs (compare.py takes them from
            # there), as in the paper.
            "reduced": {"subset": f"{SUB}/tierd-reduced.txt", "current_from_tierd": True,
                        "configurations": [f"Milestone {m}" for m in "ABCDE"],
                        "limits": {"seconds": 600, "memory_gib": REDUCED_MEMORY_GIB, "cpus": 1,
                                   "check_seconds": 600}},
            "smoke": {"subset": f"{SUB}/tierd-smoke.txt", "current_from_tierd": True,
                      "configurations": [f"Milestone {m}" for m in "ABCDE"], "limits": SMOKE},
        },
        "configurations": {
            **{f"Milestone {m}": {"command": [f"{T}/paper-milestone-{m.lower()}/tactix", "{cnf}"],
                                  "proof": "none", "build": f"paper-milestone-{m.lower()}"}
               for m in "ABCDE"},
            "Current": {"command": [f"{T}/paper-tierd/tactix", "--auto-proof", "{cnf}", "{proof}"],
                        "proof": "tactix", "build": "paper-tierd"},
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
        "profiles": {
            # Figure 5 compares solving times: the reduced run checks SAT models
            # but not certificates (checking was three quarters of its time).
            "reduced": {"subset": f"{SUB}/interface-reduced.txt", "check": False,
                        "limits": {"seconds": 300, "memory_gib": REDUCED_MEMORY_GIB, "cpus": 1,
                                   "check_seconds": 600},
                        "sets": {"Tier A": {"seconds": 30}}},
            "smoke": {"subset": f"{SUB}/interface-smoke.txt", "limits": SMOKE,
                      "sets": {"Tier A": {"seconds": 30}}},
        },
        "configurations": {
            "C interface": {"command": [f"{T}/paper-interface/tactix-public", "--auto-proof", "{cnf}",
                                        "{proof}"], "proof": "tactix"},
            "C++": {"command": [f"{T}/paper-tierd/tactix", "--auto-proof", "{cnf}", "{proof}"],
                    "proof": "tactix"},
        },
    },
    "cruxmiter": {
        "description": "Case study: the cruxmiter strategy on the paper's benchmark set",
        "instances": {"kind": "files", "glob": "paper/benchmarks/cruxmiter/cruxmiter32seed*.cnf"},
        "limits": {"seconds": 43200, "memory_gib": 48, "cpus": 12, "check_seconds": 259200},
        "profiles": {
            "reduced": {"subset": f"{SUB}/cruxmiter-reduced.txt",
                        "limits": {"seconds": 1200, "memory_gib": REDUCED_MEMORY_GIB, "cpus": 1,
                                   "check_seconds": 2400}},
            "smoke": {"subset": f"{SUB}/cruxmiter-smoke.txt", "limits": SMOKE},
        },
        "configurations": {
            "Default Tactix": {"command": [f"{T}/paper-tierd/tactix", "--dsr", "{cnf}", "{proof}"],
                               "proof": "tactix"},
            "Public policy": {"command": [f"{T}/paper-cruxmiter-public/tactix-cruxmiter-policy",
                                          "{cnf}", "{proof}"], "proof": "drat"},
            "Kissat 4.0.4": KISSAT,
        },
    },
    "chvatal": {
        "description": "Case study: the Chvatal strategy on 23 subproblems of case n = 7",
        "instances": {"kind": "files", "glob": "paper/benchmarks/chvatal/*.cnf.xz",
                      "selection": "paper/logs/chvatal/paper-hard.json"},
        "limits": {"seconds": 5000, "memory_gib": 16, "cpus": 4, "check_seconds": 7200,
                   "check_memory_gib": 64},
        "profiles": {
            "reduced": {"subset": f"{SUB}/chvatal-reduced.txt",
                        "limits": {"seconds": 900, "memory_gib": REDUCED_MEMORY_GIB, "cpus": 1,
                                   "check_seconds": 2400}},
            "smoke": {"subset": f"{SUB}/chvatal-smoke.txt", "limits": SMOKE},
        },
        "configurations": {
            "Chvatal Tactix": {"command": [f"{T}/paper-chvatal/tactix-chvatal-policy",
                                           "{cnf}", "{proof}"], "proof": "tactix"},
            "Default Tactix": {"command": [f"{T}/paper-tierd/tactix", "--dsr", "{cnf}", "{proof}"],
                               "proof": "tactix"},
            "Kissat 4.0.4": KISSAT,
            "Satsuma+Kissat": REFERENCES["Satsuma+Kissat"],
        },
    },
}

# The Tactix commits the paper evaluated, by build name; setup.sh builds each
# one into tools/tactix/<name>/. All of them are in the history of this branch.
# The paper's Default Tactix runs in the case studies used commits 5d2bab322
# and 903ec8bb4; their command-line solver is the same as paper-tierd's (they
# differ only in examples and in C-interface-only paths), so it is used here.
BUILDS = {
    "paper-tierd": "0f7c10174faa13e41e3ab60654bf8fc3322f4041",
    "paper-interface": "984a72441a5f46528ec63546869bbea56b82e753",
    "paper-milestone-a": "dcc3ecbd6591b80889f642c33ecda19ca660efcc",
    "paper-milestone-b": "546298924b6e8989b39436b7e44b7317d1bf60e6",
    "paper-milestone-c": "6f44f23d6f1b32126d7def74ad3918b3b9705a83",
    "paper-milestone-d": "f50e46c0c45b049325e365b9afb17b3d9debaca4",
    "paper-milestone-e": "0a90633874488baeffc5ab4d3630017f6f46e71e",
    "paper-cruxmiter-public": "74e2317d425ae68075c63e10b7254c1b569c8123",
    "paper-chvatal": "74e2317d425ae68075c63e10b7254c1b569c8123",
}
