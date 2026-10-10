#!/usr/bin/env python3
"""Seed-7-only policy search for the cruxmiter size-32 family.

The development target is deliberately fixed to cruxmiter32seed7.cnf.  Other
size-32 seeds remain untouched until a policy has been selected.  The screen
writes certificates (so solve timings include proof production) but defers the
expensive checker; the final stage verifies every selected arm and Kissat.
"""

import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
from types import SimpleNamespace

import run


ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PIN_COMMIT = "cd10a2e1e"
PIN = os.path.join(ROOT, "build", f"pin-cruxmiter-{PIN_COMMIT}")
OLD_PIN = os.path.join(ROOT, "build", "pin-cruxmiter-255f5ade0")
DEFAULT = os.path.join(PIN, "tactix")
POLICY = os.path.join(PIN, "examples", "tactix-cruxmiter-policy")
OLD_POLICY = os.path.join(OLD_PIN, "examples", "tactix-cruxmiter-policy")
NEXT_POLICY = os.path.join(
    ROOT, "build", "pin-cruxmiter-d2f139f0b", "examples",
    "tactix-cruxmiter-policy")
INPUT_POLICY = os.path.join(
    ROOT, "build", "pin-cruxmiter-13500d0b0", "examples",
    "tactix-cruxmiter-policy")
PHASE_POLICY = os.path.join(
    ROOT, "build", "pin-cruxmiter-a5dddbaca", "examples",
    "tactix-cruxmiter-policy")
FIXPOINT_POLICY = os.path.join(
    ROOT, "build", "pin-cruxmiter-97b3567f1", "examples",
    "tactix-cruxmiter-policy")
ORDER_POLICY = os.path.join(
    ROOT, "build", "pin-cruxmiter-2611ff982", "examples",
    "tactix-cruxmiter-policy")
DEEP_SWEEP_POLICY = os.path.join(
    ROOT, "build", "pin-cruxmiter-612e5ddac", "examples",
    "tactix-cruxmiter-policy")
TACO_WARMUP_POLICY = os.path.join(
    ROOT, "build", "pin-cruxmiter-75cfe6d23", "examples",
    "tactix-cruxmiter-policy")
TAIL_BVE_POLICY = os.path.join(
    ROOT, "build", "pin-cruxmiter-0bf67f97e", "examples",
    "tactix-cruxmiter-policy")
BDD_ORDER_POLICY = os.path.join(
    ROOT, "build", "pin-cruxmiter-43db14f2b", "examples",
    "tactix-cruxmiter-policy")
KISSAT = os.path.join(ROOT, "tools", "solvers", "bin", "kissat")
ENV = "/usr/bin/env"
REL = "cruxmiter/cruxmiter32seed7.cnf"


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def cli(*settings):
    return [ENV, *settings, DEFAULT, "--dsr"]


def policy(profile="backbone", *settings, binary=POLICY):
    return [ENV, *settings, binary, f"--profile={profile}", "--drat"]


# All arms are formula-independent settings or public-rule sequences.  No arm
# branches on the benchmark name or reads private Kernel state.
ARMS = {
    "default": cli(),
    "default-stable": cli("TACTIX_FORCE_MODE=stable"),
    "default-focused": cli("TACTIX_FORCE_MODE=focused"),
    "known-132": policy("backbone", "TACTIX_FORCE_MODE=stable",
                        binary=OLD_POLICY),
    "backbone-stable": policy("backbone", "TACTIX_FORCE_MODE=stable"),
    "backbone-auto": policy("backbone"),
    "backbone-focused": policy("backbone", "TACTIX_FORCE_MODE=focused"),
    "gate-stable": policy("gate", "TACTIX_FORCE_MODE=stable"),
    "backbone-close-stable": policy("backbone-close",
                                    "TACTIX_FORCE_MODE=stable"),
    "backbone-heavy-stable": policy("backbone-heavy",
                                    "TACTIX_FORCE_MODE=stable"),
    "symmetry-backbone-stable": policy("symmetry-backbone",
                                       "TACTIX_FORCE_MODE=stable"),
    "backbone-symmetry-stable": policy("backbone-symmetry",
                                       "TACTIX_FORCE_MODE=stable"),
    "symmetry-sandwich-stable": policy("symmetry-sandwich",
                                       "TACTIX_FORCE_MODE=stable"),
    "full-stable": policy("full", "TACTIX_FORCE_MODE=stable"),
    # Direct public-policy interpretations of stable=2,target=0 and the three
    # phase choices.  These deliberately bypass DefaultPolicy search.
    "target0-negative": policy("search"),
    "target0-positive": policy("positive"),
    "target0-saved": policy("saved"),
    # Dedicated public clients combining the structural prelude with the exact
    # stable=2,target=0,phase=0 search interpretation.
    "gate-target0": policy("gate-search", binary=NEXT_POLICY),
    "backbone-target0": policy("backbone-search", binary=NEXT_POLICY),
    "backbone-close-target0": policy("backbone-close-search",
                                     binary=NEXT_POLICY),
    "target0-no-repeat": policy(
        "backbone-close-search", "TACTIX_CRUX_INPROCESS_INTERVAL=0",
        binary=NEXT_POLICY),
    "target0-repeat1k": policy(
        "backbone-close-search", "TACTIX_CRUX_INPROCESS_INTERVAL=1000",
        binary=NEXT_POLICY),
    "target0-repeat5k": policy(
        "backbone-close-search", "TACTIX_CRUX_INPROCESS_INTERVAL=5000",
        binary=NEXT_POLICY),
    "target0-repeat50k": policy(
        "backbone-close-search", "TACTIX_CRUX_INPROCESS_INTERVAL=50000",
        binary=NEXT_POLICY),
    "target0-reluctant64": policy(
        "backbone-close-search", "TACTIX_CRUX_RELUCTANT_PERIOD=64",
        binary=NEXT_POLICY),
    "target0-reluctant256": policy(
        "backbone-close-search", "TACTIX_CRUX_RELUCTANT_PERIOD=256",
        binary=NEXT_POLICY),
    "target0-reluctant4096": policy(
        "backbone-close-search", "TACTIX_CRUX_RELUCTANT_PERIOD=4096",
        binary=NEXT_POLICY),
    "target0-reduce500": policy(
        "backbone-close-search", "TACTIX_CRUX_REDUCE_INTERVAL=500",
        binary=NEXT_POLICY),
    "target0-reduce1000": policy(
        "backbone-close-search", "TACTIX_CRUX_REDUCE_INTERVAL=1000",
        binary=NEXT_POLICY),
    "target0-reduce5000": policy(
        "backbone-close-search", "TACTIX_CRUX_REDUCE_INTERVAL=5000",
        binary=NEXT_POLICY),
    "target0-focus100": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=100",
        binary=NEXT_POLICY),
    "target0-focus500": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=500",
        binary=NEXT_POLICY),
    "target0-focus1000": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=1000",
        binary=NEXT_POLICY),
    "target0-focus2000": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=2000",
        binary=NEXT_POLICY),
    "target0-focus5000": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=5000",
        binary=NEXT_POLICY),
    "target0-focus10000": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=10000",
        binary=NEXT_POLICY),
    "target0-focus1000-no-repeat": policy(
        "backbone-close-search", "TACTIX_CRUX_FOCUS_CONFLICTS=1000",
        "TACTIX_CRUX_INPROCESS_INTERVAL=0", binary=NEXT_POLICY),
    # Search and in-processing ablations around the known stable winner.
    "stable-randec0": policy("backbone", "TACTIX_FORCE_MODE=stable",
                             "TACTIX_RANDEC=0"),
    "stable-chrono0": policy("backbone", "TACTIX_FORCE_MODE=stable",
                             "TACTIX_CHRONO_LEVELS=0"),
    "stable-chrono20": policy("backbone", "TACTIX_FORCE_MODE=stable",
                              "TACTIX_CHRONO_LEVELS=20"),
    "stable-chrono1000": policy("backbone", "TACTIX_FORCE_MODE=stable",
                                "TACTIX_CHRONO_LEVELS=1000"),
    "stable-no-conflict-reuse": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_CONFLICT_REUSE=0"),
    "stable-no-jump-reasons": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_JUMP_REASONS=0"),
    "stable-no-reason-bump": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_NO_BUMP_REASONS=1"),
    "stable-reduce-arith": policy(
        "backbone", "TACTIX_FORCE_MODE=stable",
        "TACTIX_REDUCE_CADENCE=arith", "TACTIX_REDUCE_RAMP=0"),
    "stable-reorder0": policy("backbone", "TACTIX_FORCE_MODE=stable",
                              "TACTIX_REORDER=0"),
    "stable-walk0": policy("backbone", "TACTIX_FORCE_MODE=stable",
                           "TACTIX_WALK_PERMILLE=0"),
    "stable-no-factor": policy("backbone", "TACTIX_FORCE_MODE=stable",
                               "TACTIX_NO_FACTOR=1"),
    "stable-no-sweep": policy("backbone", "TACTIX_FORCE_MODE=stable",
                              "TACTIX_NO_SWEEP=1"),
    "stable-no-bve": policy("backbone", "TACTIX_FORCE_MODE=stable",
                            "TACTIX_NO_BVE=1"),
    "stable-preprocess-only": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_PREPROCESS_ONLY=1"),
    "stable-eager0": policy("backbone", "TACTIX_FORCE_MODE=stable",
                            "TACTIX_EAGER_SUBSUME=0"),
    "stable-no-closure-delay": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_NO_CLOSURE_DELAY=1"),
    "stable-backbone100": policy(
        "backbone", "TACTIX_FORCE_MODE=stable",
        "TACTIX_SLOT_PERMILLE=Backbone:100"),
    "stable-vivify300": policy(
        "backbone", "TACTIX_FORCE_MODE=stable",
        "TACTIX_SLOT_PERMILLE=Vivify:300"),
    "stable-substitute100": policy(
        "backbone", "TACTIX_FORCE_MODE=stable",
        "TACTIX_SLOT_PERMILLE=Substitute:100"),
    "stable-aggressive-slots": policy(
        "backbone", "TACTIX_FORCE_MODE=stable",
        "TACTIX_SLOT_PERMILLE=Backbone:100,Vivify:300,Substitute:100"),
    "stable-bve64": policy("backbone", "TACTIX_FORCE_MODE=stable",
                           "TACTIX_BVE_RAMP_MAX=64"),
    "stable-cce1": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_SLOT_ENABLE=CCE",
        "TACTIX_SLOT_MAX_APPLIES=CCE:1"),
    # Coordinate search around the first seed-7 winner (Reorder disabled).
    "default-stable-reorder0": cli("TACTIX_FORCE_MODE=stable",
                                   "TACTIX_REORDER=0"),
    "gate-stable-reorder0": policy("gate", "TACTIX_FORCE_MODE=stable",
                                   "TACTIX_REORDER=0"),
    "reorder-randec0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_RANDEC=0"),
    "reorder-reduce-arith": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_REDUCE_CADENCE=arith", "TACTIX_REDUCE_RAMP=0"),
    "reorder-no-factor": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_FACTOR=1"),
    "reorder-no-bve": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1"),
    "reorder-no-closure-delay": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_CLOSURE_DELAY=1"),
    "reorder-no-jump": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_JUMP_REASONS=0"),
    "reorder-no-conflict-reuse": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_CONFLICT_REUSE=0"),
    "reorder-walk0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_WALK_PERMILLE=0"),
    "reorder-reduce-no-factor": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_REDUCE_CADENCE=arith", "TACTIX_REDUCE_RAMP=0",
        "TACTIX_NO_FACTOR=1"),
    "reorder-reduce-no-bve": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_REDUCE_CADENCE=arith", "TACTIX_REDUCE_RAMP=0",
        "TACTIX_NO_BVE=1"),
    "reorder-no-factor-no-bve": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_FACTOR=1", "TACTIX_NO_BVE=1"),
    "reorder-randec-reduce": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_RANDEC=0", "TACTIX_REDUCE_CADENCE=arith",
        "TACTIX_REDUCE_RAMP=0"),
    "reorder-reduce-no-factor-no-bve": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_REDUCE_CADENCE=arith", "TACTIX_REDUCE_RAMP=0",
        "TACTIX_NO_FACTOR=1", "TACTIX_NO_BVE=1"),
    "reorder-core": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_RANDEC=0", "TACTIX_REDUCE_CADENCE=arith",
        "TACTIX_REDUCE_RAMP=0", "TACTIX_NO_FACTOR=1", "TACTIX_NO_BVE=1",
        "TACTIX_NO_CLOSURE_DELAY=1"),
    "reorder-core-no-jump": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_RANDEC=0", "TACTIX_REDUCE_CADENCE=arith",
        "TACTIX_REDUCE_RAMP=0", "TACTIX_NO_FACTOR=1", "TACTIX_NO_BVE=1",
        "TACTIX_NO_CLOSURE_DELAY=1", "TACTIX_JUMP_REASONS=0"),
    # Coordinate search around Reorder=off + BVE=off, the second-stage winner.
    "default-reorder-no-bve": cli(
        "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0", "TACTIX_NO_BVE=1"),
    "gate-reorder-no-bve": policy(
        "gate", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1"),
    "close-reorder-no-bve": policy(
        "backbone-close", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1"),
    "reorder-no-bve-randec0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_RANDEC=0"),
    "reorder-no-bve-walk0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_WALK_PERMILLE=0"),
    "reorder-no-bve-randec0-walk0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_RANDEC=0", "TACTIX_WALK_PERMILLE=0"),
    "reorder-no-bve-no-jump": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_JUMP_REASONS=0"),
    "reorder-no-bve-no-conflict-reuse": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0"),
    "reorder-no-bve-no-sweep": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_NO_SWEEP=1"),
    "reorder-no-bve-chrono0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CHRONO_LEVELS=0"),
    "reorder-no-bve-chrono20": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CHRONO_LEVELS=20"),
    "reorder-no-bve-chrono1000": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CHRONO_LEVELS=1000"),
    # Third-stage center: Reorder=off, BVE=off, conflict-clause reuse=off.
    "rbc-default": cli(
        "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0", "TACTIX_NO_BVE=1",
        "TACTIX_CONFLICT_REUSE=0"),
    "rbc-gate": policy(
        "gate", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0"),
    "rbc-randec0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0", "TACTIX_RANDEC=0"),
    "rbc-walk0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_WALK_PERMILLE=0"),
    "rbc-no-factor": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0", "TACTIX_NO_FACTOR=1"),
    "rbc-no-sweep": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0", "TACTIX_NO_SWEEP=1"),
    "rbc-no-closure-delay": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_NO_CLOSURE_DELAY=1"),
    "rbc-no-jump": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_JUMP_REASONS=0"),
    "rbc-backbone100": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Backbone:100"),
    "rbc-hbr40": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=HyperBinaryRes:40"),
    "rbc-hbr100": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=HyperBinaryRes:100"),
    "rbc-substitute100": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Substitute:100"),
    "rbc-sweep200": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Sweep:200"),
    "rbc-sweep-deep8": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Sweep:1000", "TACTIX_SWEEP_DEPTH=8",
        "TACTIX_SWEEP_VARS=1024", "TACTIX_SWEEP_CLAUSES=4096",
        "TACTIX_SWEEP_FLIP_ROUNDS=4", "TACTIX_SWEEP_TICKS=100000000",
        "TACTIX_SWEEP_ROUND_TICKS=500000000"),
    "rbc-sweep-deep16": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Sweep:1000", "TACTIX_SWEEP_DEPTH=16",
        "TACTIX_SWEEP_VARS=1024", "TACTIX_SWEEP_CLAUSES=4096",
        "TACTIX_SWEEP_FLIP_ROUNDS=4", "TACTIX_SWEEP_TICKS=100000000",
        "TACTIX_SWEEP_ROUND_TICKS=500000000"),
    "rbc-sweep-deep46": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Sweep:1000", "TACTIX_SWEEP_DEPTH=46",
        "TACTIX_SWEEP_VARS=2048", "TACTIX_SWEEP_CLAUSES=8192",
        "TACTIX_SWEEP_FLIP_ROUNDS=8", "TACTIX_SWEEP_TICKS=200000000",
        "TACTIX_SWEEP_ROUND_TICKS=1000000000"),
    "rbc-sweep-deep46-general": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Sweep:1000", "TACTIX_SWEEP_DEPTH=46",
        "TACTIX_SWEEP_VARS=2048", "TACTIX_SWEEP_CLAUSES=8192",
        "TACTIX_SWEEP_FLIP_ROUNDS=8", "TACTIX_SWEEP_TICKS=200000000",
        "TACTIX_SWEEP_ROUND_TICKS=1000000000",
        "TACTIX_SWEEP_FORCE_GENERAL=1"),
    # Unlike the environment knobs above, these invoke one typed public Sweep
    # step at the requested depth before DefaultPolicy starts.  This bypasses
    # the default scheduler's depth-three growth ceiling and directly tests
    # the full 46-level circuit cone observed in the parsed CNF.
    **{
        f"rbc-online-deep-sweep{depth}": policy(
            "deep-sweep", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
            "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
            f"TACTIX_CRUX_SWEEP_DEPTH={depth}", binary=DEEP_SWEEP_POLICY)
        for depth in (8, 16, 32, 46)
    },
    **{
        f"rbc-online-deep-sweep{depth}-vivify": policy(
            "deep-sweep", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
            "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
            "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
            "TACTIX_NO_SWEEP=1", f"TACTIX_CRUX_SWEEP_DEPTH={depth}",
            binary=DEEP_SWEEP_POLICY)
        for depth in (16, 46)
    },
    "rbc-transitive100": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=TransitiveReduce:100"),
    "rbc-vivify200": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200"),
    "rbc-vivify300": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:300"),
    "rbc-vivify400": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:400"),
    "rbc-vivify600": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:600"),
    "rbc-vivify1000": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:1000"),
    "rbc-vivify200-reuse0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_VIVIFY_REUSE=0"),
    "rbc-vivify200-flush0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_VIVIFY_FLUSH=0"),
    "rbc-vivify200-reuse0-flush0": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_VIVIFY_REUSE=0",
        "TACTIX_VIVIFY_FLUSH=0"),
    "rbc-vivify200-no-factor-sweep": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
        "TACTIX_NO_SWEEP=1"),
    # Conflict-analysis refinements around the current 42.6-second center.
    # The trace is search-dominated after the productive Vivify passes, so
    # these vary which implication reasons feed the stable heap and learned
    # clause tiers without changing any proof rule.
    **{
        f"rbcv-bump-limit{limit}": policy(
            "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
            "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
            "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
            "TACTIX_NO_SWEEP=1", f"TACTIX_BUMP_REASONS_LIMIT={limit}")
        for limit in (0, 1, 2, 4, 20, 50)
    },
    **{
        f"rbcv-bump-rate{rate}": policy(
            "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
            "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
            "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
            "TACTIX_NO_SWEEP=1", f"TACTIX_BUMP_REASONS_RATE={rate}")
        for rate in (1, 2, 5, 20, 50)
    },
    **{
        f"rbcv-eager{amount}": policy(
            "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
            "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
            "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
            "TACTIX_NO_SWEEP=1", f"TACTIX_EAGER_SUBSUME={amount}")
        for amount in (0, 1, 2)
    },
    "rbcv-no-ext-reason-bump": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
        "TACTIX_NO_SWEEP=1", "TACTIX_NO_EXT_REASON_BUMP=1"),
    "rbcv-no-dynamic-tiers": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
        "TACTIX_NO_SWEEP=1", "TACTIX_NO_DYNAMIC_TIERS=1"),
    "rbcv-no-compact": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
        "TACTIX_NO_SWEEP=1", "TACTIX_NO_COMPACT=1"),
    "rbcv-no-shrink-glue": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
        "TACTIX_NO_SWEEP=1", "TACTIX_SHRINK_GLUE=0"),
    "rbcv-chrono-reuse": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
        "TACTIX_NO_SWEEP=1", "TACTIX_CHRONO_REUSE=1"),
    **{
        f"rbcv-vivify{dose}": policy(
            "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
            "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
            f"TACTIX_SLOT_PERMILLE=Vivify:{dose}", "TACTIX_NO_FACTOR=1",
            "TACTIX_NO_SWEEP=1")
        for dose in (125, 150, 175, 225, 250, 275)
    },
    "rbcv-disable-backbone": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
        "TACTIX_NO_SWEEP=1", "TACTIX_SLOT_DISABLE=Backbone"),
    "rbcv-disable-backbone-congruence": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
        "TACTIX_NO_SWEEP=1", "TACTIX_SLOT_DISABLE=Backbone,Congruence"),
    "rbcv-disable-structural": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
        "TACTIX_NO_SWEEP=1",
        "TACTIX_SLOT_DISABLE=Backbone,Congruence,Strengthen"),
    "rbcv-vivify-only": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
        "TACTIX_NO_SWEEP=1",
        "TACTIX_SLOT_DISABLE=Backbone,Congruence,Strengthen,Substitute,TransitiveReduce"),
    **{
        f"taco-warmup-{conflicts}": policy(
            "taco-warmup", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
            "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
            "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
            "TACTIX_NO_SWEEP=1", "TACTIX_SLOT_DISABLE=Backbone",
            f"TACTIX_CRUX_WARMUP_CONFLICTS={conflicts}",
            binary=TACO_WARMUP_POLICY)
        for conflicts in (100, 1000, 5000, 10000, 25000, 50000, 100000,
                          250000)
    },
    **{
        f"tail-bve-{cutoff}": policy(
            "bdd-bve", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
            "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
            "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
            "TACTIX_NO_SWEEP=1", "TACTIX_SLOT_DISABLE=Backbone",
            f"TACTIX_CRUX_BVE_CUTOFF={cutoff}", binary=TAIL_BVE_POLICY)
        for cutoff in (565, 560, 550, 540, 530, 528, 520, 500)
    },
    **{
        f"rbc-order-{order}": policy(
            "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
            "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
            f"TACTIX_CRUX_INPUT_ORDER={order}", binary=BDD_ORDER_POLICY)
        for order in ("primary", "primary-reverse", "primary-interleave",
                      "primary-interleave-reverse")
    },
    **{
        f"rbcv-order-{order}": policy(
            "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
            "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
            "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
            "TACTIX_NO_SWEEP=1", "TACTIX_SLOT_DISABLE=Backbone",
            f"TACTIX_CRUX_INPUT_ORDER={order}", binary=BDD_ORDER_POLICY)
        for order in ("primary", "primary-reverse", "primary-interleave",
                      "primary-interleave-reverse")
    },
    "rbc-cce1": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_ENABLE=CCE", "TACTIX_SLOT_MAX_APPLIES=CCE:1"),
    # The seed-7 boundary trace shows all structural progress in the startup
    # and first paired search-time rounds.  Later Congruence/Sweep/Factor and
    # Backbone passes are no-ops, while Vivify repeatedly spends most of the
    # inprocessing ticks deleting learned clauses.  Preserve the productive
    # prefix and cap the tail through the public slot scheduler.
    "rbc-cap2": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_MAX_APPLIES=Congruence:2,Substitute:2,Backbone:2,Vivify:2,Sweep:2,TransitiveReduce:2,Factor:2"),
    "rbc-cap3": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_MAX_APPLIES=Congruence:3,Substitute:3,Backbone:3,Vivify:3,Sweep:3,TransitiveReduce:3,Factor:3"),
    "rbc-cap5": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_MAX_APPLIES=Congruence:5,Substitute:5,Backbone:5,Vivify:5,Sweep:5,TransitiveReduce:5,Factor:5"),
    "rbc-cap10": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_MAX_APPLIES=Congruence:10,Substitute:10,Backbone:10,Vivify:10,Sweep:10,TransitiveReduce:10,Factor:10"),
    "rbc-vivify2": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_MAX_APPLIES=Vivify:2"),
    "rbc-vivify3": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_MAX_APPLIES=Vivify:3"),
    "rbc-vivify5": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_MAX_APPLIES=Vivify:5"),
    "rbc-cap3-prune-noops": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_DISABLE=Sweep,Factor,Strengthen",
        "TACTIX_SLOT_MAX_APPLIES=Congruence:3,Substitute:3,Backbone:3,Vivify:3,TransitiveReduce:2"),
    "fixpoint-stable": policy(
        "backbone-fixpoint", "TACTIX_FORCE_MODE=stable",
        binary=FIXPOINT_POLICY),
    "fixpoint-reorder-no-bve": policy(
        "backbone-fixpoint", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", binary=FIXPOINT_POLICY),
    "fixpoint-rbc": policy(
        "backbone-fixpoint", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        binary=FIXPOINT_POLICY),
    "fixpoint-rbc-capped": policy(
        "backbone-fixpoint", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_SLOT_DISABLE=Sweep,Factor,Strengthen",
        "TACTIX_SLOT_MAX_APPLIES=Congruence:3,Substitute:3,Backbone:3,Vivify:3,TransitiveReduce:2",
        binary=FIXPOINT_POLICY),
    "rbc-order-reverse": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_CRUX_INPUT_ORDER=reverse", binary=ORDER_POLICY),
    "rbc-order-max-desc": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_CRUX_INPUT_ORDER=max-desc", binary=ORDER_POLICY),
    "rbc-order-hot": policy(
        "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_CRUX_INPUT_ORDER=hot", binary=ORDER_POLICY),
    **{
        f"rbc-order-random{seed}": policy(
            "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
            "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
            f"TACTIX_CRUX_INPUT_ORDER=random{seed}", binary=ORDER_POLICY)
        for seed in range(32)
    },
    **{
        f"rbc-vivify-order-random{seed}": policy(
            "backbone", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
            "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
            "TACTIX_SLOT_PERMILLE=Vivify:200", "TACTIX_NO_FACTOR=1",
            "TACTIX_NO_SWEEP=1", f"TACTIX_CRUX_INPUT_ORDER=random{seed}",
            binary=ORDER_POLICY)
        for seed in range(32)
    },
    # Preserve the winning scheduler/search settings while using the public
    # warm-up surface to install TACO's phase=0 observation as saved phases.
    "rbc-backbone-negative": policy(
        "backbone-negative", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0", binary=PHASE_POLICY),
    "rbc-backbone-positive": policy(
        "backbone-positive", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0", binary=PHASE_POLICY),
    "rb-backbone-negative": policy(
        "backbone-negative", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", binary=PHASE_POLICY),
    "rbc-backbone-negative-randec0": policy(
        "backbone-negative", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0", "TACTIX_RANDEC=0",
        binary=PHASE_POLICY),
    "rbc-backbone-negative-walk0": policy(
        "backbone-negative", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        "TACTIX_WALK_PERMILLE=0", binary=PHASE_POLICY),
    # Explicitly preserve the producer/consumer order from Kissat's probe
    # phase instead of the default name-sorted inprocessing slot order.
    "rbc-kissat-order": policy(
        "kissat-order", "TACTIX_FORCE_MODE=stable", "TACTIX_REORDER=0",
        "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0", binary=PHASE_POLICY),
    "rbc-kissat-order-negative": policy(
        "kissat-order-negative", "TACTIX_FORCE_MODE=stable",
        "TACTIX_REORDER=0", "TACTIX_NO_BVE=1", "TACTIX_CONFLICT_REUSE=0",
        binary=PHASE_POLICY),
    "kissat-order-negative": policy(
        "kissat-order-negative", "TACTIX_FORCE_MODE=stable",
        binary=PHASE_POLICY),
    # CNF-derived policy: variables 1..32 are primary inputs of a complete
    # acyclic gate encoding; branch only on them and let propagation evaluate
    # variables 33..570.
    "input-ranked": policy("input-ranked", binary=INPUT_POLICY),
    "input-saved": policy("input-saved", binary=INPUT_POLICY),
    "input-ascending": policy("input-ascending", binary=INPUT_POLICY),
    "input-descending": policy("input-descending", binary=INPUT_POLICY),
    "input-ranked-focus1000": policy(
        "input-ranked", "TACTIX_CRUX_FOCUS_CONFLICTS=1000",
        binary=INPUT_POLICY),
    "input-ranked-focus10000": policy(
        "input-ranked", "TACTIX_CRUX_FOCUS_CONFLICTS=10000",
        binary=INPUT_POLICY),
    "input-ranked-reluctant256": policy(
        "input-ranked", "TACTIX_CRUX_RELUCTANT_PERIOD=256",
        binary=INPUT_POLICY),
    "input-ranked-reduce500": policy(
        "input-ranked", "TACTIX_CRUX_REDUCE_INTERVAL=500",
        binary=INPUT_POLICY),
    "kissat": [KISSAT],
    # Directly validate the CNC+TACO observation on the development seed.
    # These are diagnostic controls for transferring the winning components
    # back through Tactix's public policy surface.
    "kissat-taco": [KISSAT, "--stable=2", "--target=0", "--phase=0"],
    "kissat-taco-positive": [
        KISSAT, "--stable=2", "--target=0", "--phase=1"],
    "kissat-taco-target": [
        KISSAT, "--stable=2", "--target=1", "--phase=0"],
    "kissat-taco-reorder0": [
        KISSAT, "--stable=2", "--target=0", "--phase=0", "--reorder=0"],
    "kissat-taco-eliminate0": [
        KISSAT, "--stable=2", "--target=0", "--phase=0", "--eliminate=0"],
    "kissat-taco-reorder0-eliminate0": [
        KISSAT, "--stable=2", "--target=0", "--phase=0", "--reorder=0",
        "--eliminate=0"],
    "kissat-taco-vivify200": [
        KISSAT, "--stable=2", "--target=0", "--phase=0",
        "--vivifyeffort=200"],
    "kissat-taco-sweep0": [
        KISSAT, "--stable=2", "--target=0", "--phase=0", "--sweep=0"],
}


DEFAULT_SCREEN = tuple(name for name in ARMS if name != "kissat")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stage", choices=("screen", "verify"))
    parser.add_argument("--arms", default="")
    parser.add_argument("--instances", default=REL,
                        help="comma-separated cruxmiter size variants, seed 7 only")
    parser.add_argument("--timeout", type=float, default=None)
    parser.add_argument("--partition", default="cpu-dense-hwulab-q")
    parser.add_argument("--wait", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    requested = tuple(filter(None, args.arms.split(",")))
    selected = requested or (DEFAULT_SCREEN if args.stage == "screen" else (
        "default", "known-132", "backbone-stable", "kissat"))
    unknown = sorted(set(selected) - set(ARMS))
    if unknown:
        raise SystemExit("unknown arm(s): " + ", ".join(unknown))
    timeout = args.timeout if args.timeout is not None else (
        240.0 if args.stage == "screen" else 600.0)
    rels = tuple(filter(None, args.instances.split(",")))
    if not rels or any(not re.fullmatch(
            r"cruxmiter/cruxmiter(?:2[0-9]|3[0-2])seed7\.cnf", rel)
                       for rel in rels):
        raise SystemExit("--instances accepts only cruxmiter sizes 20..32 seed 7")
    paths = {rel: os.path.join(ROOT, rel) for rel in rels}
    for binary in (DEFAULT, POLICY, OLD_POLICY, NEXT_POLICY, INPUT_POLICY,
                   PHASE_POLICY, FIXPOINT_POLICY, ORDER_POLICY,
                   DEEP_SWEEP_POLICY, TACO_WARMUP_POLICY, TAIL_BVE_POLICY,
                   BDD_ORDER_POLICY, KISSAT):
        if not os.path.isfile(binary) or not os.access(binary, os.X_OK):
            raise SystemExit(f"missing executable pin: {binary}")
    missing = [path for path in paths.values() if not os.path.isfile(path)]
    if missing:
        raise SystemExit("missing input(s): " + ", ".join(missing))

    # Screen timings include proof writing but defer verification.  Verify is
    # submission-grade: every SAT model or UNSAT proof is independently checked.
    verify = args.stage == "verify"
    solvers = [(name, ARMS[name], True, verify, verify) for name in selected]
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    label = f"crux-seed7-{args.stage}"
    outdir = os.path.join(run.RUNS, f"{stamp}-{label}")
    os.makedirs(outdir, exist_ok=False)
    metadata = {
        "stage": args.stage,
        "development_instances": rels,
        "held_out_size32_seeds": [0, 2, 3, 4, 5, 6, 8, 9],
        "target_speedup": "5x-10x versus faster of DefaultPolicy and Kissat",
        "timeout_seconds": timeout,
        "partition": args.partition,
        "cpus_per_task": 4,
        "memory_gib": 16,
        "pin_source_commit": PIN_COMMIT,
        "campaign_wrapper_commit": subprocess.run(
            ["git", "-C", ROOT, "rev-parse", "--short", "HEAD"], check=True,
            capture_output=True, text=True).stdout.strip(),
        "campaign_wrapper_sha256": digest(__file__),
        "input_sha256": {rel: digest(path) for rel, path in paths.items()},
        "executables": {path: digest(path) for path in
                        (DEFAULT, POLICY, OLD_POLICY, NEXT_POLICY,
                         INPUT_POLICY, PHASE_POLICY, FIXPOINT_POLICY,
                         ORDER_POLICY, DEEP_SWEEP_POLICY, TACO_WARMUP_POLICY,
                         TAIL_BVE_POLICY, BDD_ORDER_POLICY, KISSAT)},
        "arms": {name: ARMS[name] for name in selected},
        "proof_policy": ("write and verify" if verify else
                         "write for timed parity; delete without checking"),
        "proof_lifecycle": (
            "task-local mkdtemp; removed on normal return, exception, "
            "SIGTERM, and SIGINT"),
    }
    with open(os.path.join(outdir, "campaign.json"), "w") as target:
        json.dump(metadata, target, indent=2, sort_keys=True)
        target.write("\n")
    submit = SimpleNamespace(partition=args.partition, throttle=None,
                             dry_run=args.dry_run, wait=args.wait, tier=label)
    print(f"development targets: {', '.join(rels)}")
    print(f"arms: {', '.join(selected)}")
    # run.slurm_submit maps 16 GiB to two physical / four logical CPUs.
    return run.slurm_submit(
        submit, [(rel, "UNKNOWN", 0.0) for rel in rels], solvers, paths,
        timeout, 16,
        outdir, stamp, PIN_COMMIT, label)


if __name__ == "__main__":
    raise SystemExit(main())
