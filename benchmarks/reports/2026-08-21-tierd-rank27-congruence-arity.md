# Tier-D rank 27: restore reference-wide Congruence AND scope

## Outcome

Tactix's first portable rank-27 divergence was not a heap tie, reinsertion
order, or Vivify schedule. Its opening Congruence pass extracted AND gates only
through arity 64, while Kissat and CaDiCaL both ship an arity limit of
1,000,000. The smallest repair changes that one scope value and adds a boundary
regression at arity 65.

The repair is correctness- and regression-clean, and it restores all 132
opening equivalence classes missing from Tactix. It does **not** close rank 27:
the checked witness remains UNKNOWN at the 1,200 s debug limit. The next
investigation must therefore begin from the corrected opening state rather
than repeat the closed arity or heap hypotheses.

## Frozen witness and accepted predecessor

- Immutable rank: 27, expected UNSAT.
- Input: `tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz`.
- Compressed SHA-256:
  `5435b1b271b049ed9291ab1b9b297afb33b8d2d3c50ff1b48bfaed4ddf7bb4e9`.
- Frozen Kissat time: 278.187 s. The frozen reference was not rerun.
- Accepted predecessor binary SHA-256:
  `b6c991e099f4fa9cc635bacb20d7aa44ff3fdc23d0af78974b12998cecd6fb6f`.

These identities come from the frozen ranks-21--40 manifest and the accepted
Vivify-admission closure. No rank, hash, filename, deadline, or literal enters
production behavior.

## Localization

The retained rank-27 trace was exact through decision 69,892. Decision 69,893
selected variable 392,211 in Tactix and 392,214 in Kissat at the same score,
which initially made stable-heap shape the visible boundary. Paired diagnostics
then ruled out the consumer:

- conflict 1532, its bumped variables, and its top-32 heap were exact;
- 35,887 heap reinsertion events through conflict 1532 matched exactly;
- the heap implementations use the same strict comparison and child choice.

The producer state already differed after opening Congruence. Kissat merged
58,722 variables: 55,658 from AND gates, 2,995 from XOR gates, and 69 from
paired binary equivalences. Tactix merged 58,590: 55,526 AND, 2,995 XOR, and
69 binary. Thus the entire 132-variable deficit belonged to AND extraction or
closure.

Diagnostic job `1636967` measured Tactix's closure funnel. Diagnostic job
`1636968` separated Kissat's four AND-merge call sites: its opening 55,658
split into 53,446 extraction-time matches and 2,212 later ordinary AND
rewrites; ITE-to-AND contributed zero. Job `1636970` dumped both final opening
representative partitions. They differed on exactly 132 external variables.
The first was `93231`: Tactix left it singleton while Kissat proved
`93231 == -93230`.

The input clauses define that pair through matching 72-input gates. Selected
gate job `1636972` showed no corresponding high-fan-in Tactix gate because
`CONG_ARITY_LIMIT = 64` rejected it before the closure worklist. This source
site is also pre-recorded in `docs/scheduling-parity-ledger.md` correction C7:
both reference solvers ship 1,000,000, and the ledger explicitly names
`64 -> 1e6` as the parity repair.

## Repair

`src/rules/congruence/congruence.cpp` now sets the AND arity scope to
1,000,000, citing both reference defaults and retaining Tactix's independent
round work budget as the effort governor. No scheduling order, proof rule,
hashing scheme, representative choice, or instance classifier changed.

`tests/test_congruence.cpp` constructs two congruent 65-input AND gates and
requires their outputs to merge. Arity 65 is the first value rejected by the
old scope, so the fixture is formula-general and boundary-exact.

## Validation

Release configuration passes 72/72 tests, including native LRAT/DRAT proof
tests and the new wide-AND regression. No unit test or fuzz leg exposed a crash
or soundness defect, so the plan's Claude repair-delegation trigger did not
fire.

The required matched iteration guard is
`benchmarks/runs/20260821-0035-BC-congruence-arity/` (SLURM job `1636973`):

| arm | Tier B | Tier C | total checked | PAR-2 / wall |
|---|---:|---:|---:|---:|
| accepted predecessor | 28/28 | 5/5 | 33/33 | 2228.698 s |
| repaired candidate | 28/28 | 5/5 | 33/33 | 2239.817 s |

The candidate/predecessor ratio is 1.004989. All 34 SAT models and 32 UNSAT
proofs across the two arms verify. Peak candidate RSS is 3.228 GiB. Every task
reports deleted scratch and derived artifacts; the aggregate finds no
certificate residue.

Post-repair diagnostic job `1637040` directly confirms the corrected opening
counts: 55,658 AND merges, 2,995 XOR merges, and 69 paired-binary merges,
58,722 total. Its deliberate 15-second timeout deletes the partial proof and
leaves no residue. The compact campaign is
`benchmarks/runs/20260821-0125-tierd-rank27-repaired-congruence-count/`.

The immutable target campaign is
`benchmarks/runs/20260821-0052-tierd-rank27-congruence-arity/` (job `1637039`).
It times out UNKNOWN at 1200.004 s. The runner observed a 128,974,848-byte
partial certificate before cleanup; it deleted that scratch, and the campaign
contains no certificate residue. Scheduler peak RSS was 496,616 KiB.

## Limitations and next action

This increment closes the AND extraction-scope divergence, not rank 27. It
does not establish that Tactix and Kissat remain semantically exact after the
corrected opening Congruence/Substitute state. The old decision-69,893 split
was downstream of the 132-variable population gap and is no longer a valid
frontier for the repaired solver.

Next, rebuild a semantic-trace pin from this source and compare Tactix with the
retained diagnostic Kissat build from the corrected 58,722-merge opening state.
Localize the first new semantic, scheduling, or representation split, and run
the full B plus predeclared C guard again before another rank-27 witness.
