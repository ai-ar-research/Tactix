# Tactix — a proof-rule-driven SAT solver (working name)

A sequential CDCL SAT solver architected around an explicit **proof-rule API**:
at any point the embedding client can query which proof rules are currently
applicable and apply a chosen rule instance; a default policy schedules rules
automatically to act as a competitive solver. Every rule application emits a
DRAT/LRAT proof delta, and I/O follows SAT Competition main-track conventions.

**Status: in development (phase 4 — reference-faithful scheduling).** Core CDCL + proof
production + the rule API are in place; the inprocessing rule inventory
(subsume, strengthen, vivify, probe, backbone, substitute, congruence,
transitive reduction, blocked-clause elimination, hyper-ternary resolution,
instantiation, BVE, factoring/BVA, sweeping) is implemented and scheduled, and
work is focused on faithful default scheduling and Tier-A convergence with
Kissat. See
[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) for the architecture, benchmark tiers,
fuzzing/proof-checking harness, and milestones; [PROGRESS.md](PROGRESS.md) for the
live checklist; and [docs/README.md](docs/README.md) for the documentation map.

Reference solvers (Kissat etc.) are kept as local clones and are deliberately
untracked (see `.gitignore`).

## Layout

```
src/            kernel, rule families, policies, proof backend
tools/fuzz/     fuzzing harness: generators, oracles, shrinker, regression corpus
tools/bench/    benchmark tier runner, PAR-2 scoring, reports
benchmarks/     tier instance lists (tracked) + local cache (untracked)
.claude/skills/ project workflows for Codex and Claude Code (benchmarks, fuzz, proof-check, ...)
```
