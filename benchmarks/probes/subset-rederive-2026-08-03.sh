#!/usr/bin/env bash
# Re-derive the T3 rule-neutral subset with REPLICATES.
#
# WHY.  `benchmarks/tiers/C-ruleneutral.list` is derived from window
# `20260727-002213-C-kabl`, a SINGLE-SHOT sweep ablation: exclude an instance iff
# ablating Sweep moves either baseline by >25 %.  One measurement per arm cannot
# distinguish "sweep matters here" from "this instance is chaotic", and FOUR of
# the six exclusions (ContextModel, j3037, jgiraldezlevy, urq45) are on the
# measured perturbation-FRAGILE list, with sweep ratios 1.54-2.54 sitting INSIDE
# their own 8-23x bands (probe 1579386, where the perturbations were
# walk/bump/chrono -- none of them sweep).
#
# That matters because the excluded set is almost exactly the set of instances
# where R-d retirement costs Tier C, so "R-d helps on the subset" (1.005x vs the
# OFF arm's 1.081x) may be an artefact of the derivation rather than a finding.
# T3's kissat half is now the binding phase-3 exit constraint and it is scored on
# this list, so the list needs to be trustworthy.
#
# THE CRITERION IT REPLACES IT WITH.  An instance is excluded only when the sweep
# delta exceeds that instance's OWN replicate band -- i.e. sweep-sensitivity has
# to stand out against the instance's intrinsic variance, not against zero.
#
# ARMS: kissat and cadical, each with and without sweep, x3 repeats = 12 arms
# over Tier C's 30 instances = 360 tasks.  Every arm is a reference solver, so
# --no-check (the baseline-ablation setting, matching the proof-free kabl window
# this replaces).  Ratios are within-instance, so they survive contention better
# than absolute PAR-2 -- but run it on a quiet partition anyway.
#
# ANALYSIS: tools/bench/subset_rederive.py (written alongside) reads the run and
# emits the new list plus a per-instance table of sweep-delta vs replicate band.
set -u
cd "$(git rev-parse --show-toplevel)"
K=tools/solvers/bin/kissat
C=tools/solvers/bin/cadical

args=(--tier C --slurm --throttle 48 --no-check --solver "$K" --name k_a)
for r in b c; do args+=(--checked-arm "k_$r=$PWD/$K"); done
for r in a b c; do args+=(--checked-arm "knosw_$r=$PWD/$K --sweep=0"); done
for r in a b c; do args+=(--checked-arm "c_$r=$PWD/$C"); done
for r in a b c; do args+=(--checked-arm "cnosw_$r=$PWD/$C --sweep=false"); done

echo "submitting 12 arms x 30 instances = 360 tasks (proof-free ablation)"
exec python3 tools/bench/run.py "${args[@]}"
