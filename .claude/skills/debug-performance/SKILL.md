---
name: debug-performance
description: Systematically diagnose WHY the solver is significantly slower than a baseline (Kissat / CaDiCaL) on a specific instance or family — ruling out causes in order (engineering/raw-speed, search heuristics, missing proof rules, or other) with stats decomposition and baseline ablation, then routing the fix. Use whenever tactix is ≥~2× a baseline on some instance(s), a gate/report flags a large per-instance loss, or before concluding "our search is bad" / "we need rule X". Produces a diagnosis + routes to rule-dev / revamp / a heuristic experiment / revise-plan.
---

# Diagnosing a performance gap

When we're far slower than Kissat/CaDiCaL on an instance, the wrong move is to
*guess* the cause and start tuning. This skill decomposes the gap into a small set
of measured terms and **rules out causes in order**, so the fix targets the real
lever. It exists because a real investigation (2026-07-20) mistook a **missing-rule**
gap for a search-heuristic one — see "The cardinal lesson" below.

## The four candidate causes (rule them out in this order)

1. **Missing proof rule** (an inprocessing/preprocessing pass we haven't built —
   Factor/SBVA, Sweep, …). The formula the baseline searches has been *simplified*
   in a way ours hasn't.
2. **Search-heuristic quality** (decisions, clause learning/minimization, DB
   retention, restart/mode cadence) — a bigger search tree for the *same* formula.
3. **Engineering / raw speed** (BCP throughput, clause layout, allocation) — same
   search, slower per step.
4. **Other** (a scheduling bug, a pathological interaction, wrong verdict path).

## The diagnostic ladder

**Step 0 — quantify.** Run tactix and the baseline on the instance(s), same
limits, and get the wall-time ratio. Use the **pinned** baselines
(`tools/solvers/bin/kissat`, `.../cadical`) — *not* whatever `kissat` is on PATH
(it may be missing or a different build).

**Step 1 — decompose via stats.** Capture both solvers' end-of-run stats
(tactix prints them without `-q`; kissat prints them by default; kissat labels are
`c conflicts:`, `c decisions:`, `c propagations:`, `c restarts:`, `c reductions:`).
Collect **conflicts, decisions, propagations, restarts, reductions, learned** for
each, and compute the ratios plus these derived quantities:
- **conflicts-to-solve** — the primary discriminator (search-tree size).
- **props / conflict** — the engine term.
- **decisions / conflict** — decision/phase guidance.
- **conflicts / restart** — restart cadence.

**Step 2 — localize the dominant term.**
- If **props/conflict ≈ 1×** but **conflicts ≫ 1×** → it is **not** engine speed;
  the gap is search-tree size (cause 1 or 2). (Wall-ratio ≈ conflicts-ratio ×
  props/conflict-ratio — attribute it.)
- If **props/conflict ≫ 1×** → an **engineering** gap (BCP/layout). Route to
  **revamp** (§5.4), with a profile.
- If **conflicts/restart** matches the baseline → restart cadence is **not** the
  lever (our restarts just scale with the conflict count); don't touch it.

**Step 3 — rule out a MISSING RULE before blaming heuristics (the crucial step).**
A large conflict gap does *not* imply weak heuristics. **Ablate the baseline's
rules that we lack** and re-measure: run the baseline with each such pass disabled
(`kissat --factor=false`, `--sweep=false`, `--substitute=false`, `--congruence=false`,
…; one at a time). If disabling a rule makes the baseline **as slow as us (or
worse)**, that rule is the cause — the gap is a **missing rule**, and our search is
fine. Map each disabled flag to the catalog rule we're missing (plan §5.2).

**Step 4 — confirm our search quality directly.** Compare *our* conflicts to the
**baseline-with-that-rule-disabled**:
- ours **≈ or <** baseline-minus-rule → our core search is **competitive**; the
  whole gap is the missing rule. Route to **rule-dev** (implement it) + **revise-plan**
  (schedule/prioritize it).
- ours **≫** baseline-minus-rule → a **real search-heuristic residual** on top of
  the rule; size it, then route to the specific heuristic (decision bump/decay,
  clause minimization, DB retention) as an **adopt-or-revert** experiment on Tier
  B/C (never Tier A).

**Step 5 — generalize.** One instance ≠ the general case. Repeat across the loss
set; note which cause dominates where (structured/symmetric instances — puzzles,
miters — are especially *rule*-sensitive: BVA/sweep/congruence; random-ish
instances are more *heuristic*-sensitive).

## The cardinal lesson (why this skill exists)

**Do not conclude "our search is bad" from a high conflict count alone.** In the
worked example, tactix took 13.7× kissat's conflicts on `mp1-blockpuzzle`, which
*looked* like a search-heuristic gap. Ablation showed the opposite: with
`--factor=false`, kissat took **9.9M conflicts — more than our 4.19M**. The entire
gap was the missing **Factor (BVA)** rule; our search was actually *better* than
kissat's factor-less search. Always run Step 3 before Step 4. Simplification passes
(BVA, sweep, congruence) can cut conflicts 10×+ on structured instances — a
magnitude that is easy to misread as "bad heuristics."

## Tooling notes

- **Pinned baselines only** (`tools/solvers/bin/{kissat,cadical}`); bare `kissat`
  may be absent from PATH (silent empty output).
- macOS `/bin/bash` is **3.2** — **no associative arrays** (`declare -A` is a
  silent no-op that scrambles indices). Use plain variables / a function, or
  `python3`.
- Respect the **one-evaluation-at-a-time** rule (§8.1): ablation runs are
  evaluations; don't run them concurrently with a Tier run or each other if timing
  matters.
- A hard instance may need a generous `ulimit -t`; a baseline that *times out*
  when a rule is disabled is itself the finding (that rule is load-bearing).

## Output

A short written diagnosis: the wall-ratio, the decomposition (conflicts vs
props/conflict), the ablation result (which cause dominates, with the numbers),
and the **routed owner** (rule-dev / revamp / a named heuristic experiment /
revise-plan). For a re-prioritizing finding, write it to
`benchmarks/reports/YYYY-MM-DD-*-profiling.md` and update the PROGRESS resume
anchor. Do not start the fix inside this skill — hand it to the owning skill.
