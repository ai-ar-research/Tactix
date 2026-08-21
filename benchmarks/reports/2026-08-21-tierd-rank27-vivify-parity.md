# Tier-D rank 27: Vivify candidate, schedule, and deduction parity

## Outcome

The concern about Tactix's size-dependent Vivify policy was correct. The
solver carried an invented per-candidate size limit of 100 literals; neither
Kissat nor CaDiCaL has that policy. Starting from the latest common rank-27
semantic state at conflict 3069 exposed three further coupled differences:
Tactix gave each redundant tier a fresh budget instead of Kissat's cumulative
ceiling, expanded a jumped binary reason before semantic subsumer detection,
and computed irredundant productivity from replacement clauses rather than all
successful terminals. The last error delayed away Kissat's second
irredundant round.

The smallest source-backed repair removes the size cutoff, charges schedule
and watch rebuilding to one cumulative Vivify clock, preserves the virtual
jumped reason for semantic deduction while retaining real-antecedent proof
expansion, and counts subsumption, shrink, implied/asymmetric deletion, and
instantiation in productivity. No input name, hash, rank, deadline, literal,
or per-instance constant enters production behavior.

The release suite passes 72/72. The required matched 28B+5C guard is 66/66
valid with every SAT model and UNSAT proof checked and no certificate residue.
The final rank-27 witness result is recorded below. This repair is not promoted
as a performance pin: the candidate/accepted-pin guard ratio is 1.241, which
reopens coupling localization under the development plan.

## Frozen identities and prerequisites

- Immutable expected-UNSAT input:
  `tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz`.
- Compressed SHA-256:
  `5435b1b271b049ed9291ab1b9b297afb33b8d2d3c50ff1b48bfaed4ddf7bb4e9`.
- Accepted Tactix binary SHA-256:
  `b6c991e099f4fa9cc635bacb20d7aa44ff3fdc23d0af78974b12998cecd6fb6f`,
  source commit `7b527fbc711d664898af8e9c5a9655c3bdf4204b`.
- Candidate binary SHA-256:
  `df9c98051d255bbbc00dbf7792485516bc6eee8e3773c3640ef4c4007c639c3b`,
  base commit `1ba9f0217f8832ef9ab89f0ef5ae641c60feb7f6`, source-diff SHA-256
  `37d8fffae9d0e53e4255881b8203b035451b6f41e82825e903ff3cd517a2d912`.

The branch and `origin/main` began synchronized at the base commit. The
surviving `debug/tierd-next10` worktree and unrelated SLURM jobs were inspected
and left untouched. The six inherited Tier-D closures and frozen reports were
read before new experiments. Frozen reference performance solvers were not
rerun; only retained traces and pinned diagnostic Kissat source were used.

## Localization

The retained common-state trace reaches conflict 3069 with identical long and
binary clause populations before the Vivify schedule. Four one-at-a-time
findings followed.

1. **Candidate scope.** Tactix skipped Kissat's first changed candidate because
   it had 435 literals, above `VIVIFY_CLSLIM=100`. With the cutoff removed, the
   first 32 candidate identities, order keys, priorities, and probe poses match.
   Kissat `vivify.c` has no candidate-size test; its propagation-tick limit is
   the work bound.
2. **Cumulative tier ceilings.** Kissat advances one absolute probing-tick
   limit by the 3/3/1 redundant shares and then the irredundant share. Unused
   early work carries forward; an early overrun is already charged to the next
   band. Tactix instead handed every band an independent allowance. The new
   `work_budget_offset` and explicit bounded-zero state pose Kissat's absolute
   ceiling without changing the ordinary unbounded rule convention.
3. **Virtual jumped reason.** The first changed terminal split was irredundant
   candidate hash `1c3734085b121754`: Kissat subsumed it, while Tactix rewrote
   it to a binary. The implication chain was
   `480909:binary(96248) | 96248:binary(480954) | 480954:decision`.
   Reason jumping stores the virtual resolvent
   `(-480909 or 480954)` as the semantic reason. Tactix expanded through
   96248 too early, hiding the binary subsumer. Deduction now reads the virtual
   reason; proof walks still expand it through both real antecedents.
4. **Productivity scheduling.** After that repair, the first Vivify round was
   close but Tactix ran only seven rounds overall versus Kissat's eight. Its
   productivity numerator omitted 7,982 subsumptions and raised the
   irredundant delay. Counting all successful candidate terminals restores the
   second irredundant round.

Final trace job `1637436` reaches the repaired two-round state. After the
second irredundant round both solvers have exactly 574,378 irredundant and 479
redundant long clauses. Aggregate Vivify outcomes also match exactly: 7,982
subsumed, 182 shrunken, 323 implied deletions, 0 asymmetric deletions, and 17
instantiations.

The binary snapshot still differs by 34 after the two calls. Bounded jobs
`1637438`, `1637440`, and `1637441` separate it. Twenty-one binaries are the
already-separate TransitiveReduce difference between the calls. Thirteen are
schedule-time root cleanup against Tactix's inherited retained-root
representation. All 69 binary-producing probe terminals match in effect.
Five candidates are labelled strict shrink by Tactix and binary instantiation
by Kissat, but each emits the identical pair; for example candidate
`b34e59b48820cebb` emits `(-435735 or -435727)` in both. Thus the residual is
not a different Vivify deduction from common input.

## Implementation and focused regressions

Production changes are confined to general Vivify/kernel-policy state:

- `src/rules/vivify/vivify.hpp` deletes the invented size constant;
- `src/rules/vivify/vivify.cpp` admits every live large candidate, includes
  schedule/watch work in the dose, consumes cumulative ceilings, and uses the
  virtual binary reason for semantic deduction;
- `src/kernel/kernel.hpp` carries the cumulative offset and distinguishes a
  bounded zero ceiling from an unbounded call;
- `src/policies/default_policy.cpp` implements cumulative 3/3/1 plus
  irredundant shares and all-terminal productivity.

Focused tests cover a 101-literal candidate, a precharged zero cumulative
budget, unused early-tier carry, jumped-reason subsumption, and
subsumption-only productivity. The configuration-wiring inventory classifies
the two new policy-owned fields. Temporary diagnostic output was removed from
production before final validation.

No unit test, fuzz leg, or campaign exposed a crash or soundness defect, so the
plan's mandatory Claude repair-delegation trigger did not fire.

## Correctness and performance guard

Release passes 72/72 tests. Matched job `1637369`, campaign
`benchmarks/runs/20260821-1300-BC-vivify-productivity/`, ran the accepted and
candidate pins on all 28 Tier-B fixtures plus the inherited fixed five Tier-C
fixtures under two CPUs, 16 GiB, and a 1,200-second solver timeout, without a
node constraint or array throttle.

| arm | Tier B+C valid | SAT / UNSAT | aggregate | peak RSS |
|---|---:|---:|---:|---:|
| accepted pin | 33/33 | 17 / 16 | 2233.697 s | 3.233 GiB |
| repaired candidate | 33/33 | 17 / 16 | 2772.060 s | 3.098 GiB |

All 34 SAT models and 32 UNSAT proofs across the matched pair verify. There are
no invalid, malformed, missing, cleanup-bad, or residual-certificate tasks.
The candidate ratio is 1.241019. The largest named remaining coupling witness
is Tier-C `mp1-Nb5T15` (712.115 s versus 427.820 s); Tier-B `sted5` is 199.798
s versus 36.035 s. `mod4block` is 109.431 s versus 93.951 s. Under plan section
8.3, this regression is evidence of a missing coupling, not permission to
restore the size hack or weaken the faithful source repair.

## Rank-27 witness and next action

Only after the matched guard passed was immutable rank-27 witness job `1637437`
submitted under the 16 GiB/1200 s contract. It returned UNKNOWN at 1200.004 s,
so rank 27 is not added to the closed set. The runner recorded a 252,706,816-
byte partial proof (SHA-256
`c2566396f0a7ebbaf1549354d5d3a9ee8a7f594355747451259a02d4ce96d350`),
deleted it and all derived scratch, and published zero certificate residue.
Scheduler peak RSS was 453,336 KiB. Evidence is
`benchmarks/runs/20260821-1430-tierd-rank27-vivify-parity/`.

The Vivify issue is closed at the common-input candidate, scheduling,
deduction, and binary-effect layers, while its broader performance coupling is
explicitly reopened. The next UNSAT-first action is to continue rank 27 from
the first portable post-Vivify divergence if the witness remains open; in
parallel sequence, localize the predeclared `mp1-Nb5T15`/`sted5` guard
regression before accepting this candidate as a performance pin. Any further
source change again owes the complete 28B+5C guard before a Tier-D witness.
