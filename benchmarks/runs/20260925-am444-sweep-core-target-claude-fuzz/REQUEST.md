# AM444 Claude-owned affected fuzz: Sweep LRAT core target replay

Owner: Claude Code with explicit `--model opus` (never Fable). Use immutable
candidate commit `421a38a1` and parent `d4c82409`; do not execute the mutable
root checkout. Read `CLAUDE.md`, plan §8.2, and the repository `fuzz`,
`proof-check`, and `slurm-jobs` skills. The causal repair and exact negative /
positive artifacts are in
`benchmarks/reports/2026-09-25-am444-sweep-lrat-core-target-repair.md` and
`benchmarks/probes/am444-h1-journal/`.

Own preparation, immutable source/build/harness/checker pins, SLURM jobs,
independent grading, and terminal `REPORT.md` under this directory. Keep
`OWNER_STATUS.md` current. Do not modify solver source, maintained docs, other
campaigns, commits, or pushes. Inspect only the explicit handoff artifacts of
AM442; do not reinterpret its internal campaign rows. Preserve other active
jobs and pins, including AM427.

Fuzz the changed target-specific Sweep proof path across: many saved core
clauses from one embedded refutation; different unit targets sharing a prefix;
learned and original core units; stripped root literals; unbounded and bounded
proof-step limits; interruptions and resumed Sweep; public Sweep actions and
DefaultPolicy/random-policy search. Count actual reach of each path in
Release, Debug with proof shadow, and ASan+UBSan. Check every SAT model against
the exact original CNF. For every UNSAT, independently check native LRAT with
pinned CakeLPR and native DSR through pinned no-option dsr-trim→LSR→Trestle;
exercise DRAT and negative proof controls where relevant. A similar verdict
distribution or a checker exit code without the exact success status is not
acceptance. Compare parent and candidate on the exact H1 handoff and on
generated cases; classify known pre-existing H2 separately.

Check for proof-size and peak-memory growth when many unit targets share long
prefixes, with a matched parent/candidate work counter. Do not claim N3/N4 or
Tier-D parity from this affected campaign. Arrays run at full width without a
throttle or node constraint unless a documented correctness/resource bound
requires one. Run the harness self-test if its oracle or acceptance logic
changes. Report exact case counts, reachability, model/proof checks, negative
controls, checker hashes, incomplete legs, and final verdict. For a new
failure, hand Codex one bounded original-CNF reproducer, command, artifact,
failure class and parent comparison.
