# Tactix — a proof-rule-driven SAT solver (working name)

A sequential CDCL SAT solver architected around an explicit **proof-rule API**:
at any point the embedding client can query which proof rules are currently
applicable and apply a chosen rule instance; a default policy schedules rules
automatically to act as a competitive solver. Every rule application emits a
DRAT/LRAT proof delta, and I/O follows SAT Competition main-track conventions.

**Status: planning.** See [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) for the
architecture, benchmark tiers, fuzzing/proof-checking harness design, and
milestones.

Reference solvers (Kissat etc.) are kept as local clones and are deliberately
untracked (see `.gitignore`).

## Layout (planned)

```
src/            kernel, rule families, policies, proof backend
tools/fuzz/     fuzzing harness: generators, oracles, shrinker, regression corpus
tools/bench/    benchmark tier runner, PAR-2 scoring, reports
benchmarks/     tier instance lists (tracked) + local cache (untracked)
.claude/skills/ project workflows for Claude Code (benchmarks, fuzz, proof-check, ...)
```
