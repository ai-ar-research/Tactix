# Tactix — a proof-rule-driven SAT solver (working name)

A sequential CDCL SAT solver architected around an explicit **proof-rule API**:
at any point the embedding client can query which proof rules are currently
applicable and apply a chosen rule instance; a default policy schedules rules
automatically to act as a competitive solver. Every rule application emits a
typed proof delta. Tactix's canonical certificate is one native DSR stream,
elaborated by `dsr-trim` to LSR and verified by Trestle; DRAT and native LRAT
remain explicit compatibility formats.

**Status: in development.** The online-configurable strategy and complete N1
surface are accepted with N2–N4 preserved. Native DSR integration is at its
B27+C5 and Rank1–40 landing gate; the next mechanism increment completes the
Satsuma-derived symmetry-breaking rule through direct substitution-redundancy
witnesses, followed by the fixed WV100 target. See
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
