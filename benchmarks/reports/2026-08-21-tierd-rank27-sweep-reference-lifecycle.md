# Tier-D rank 27: Sweep reference lifecycle repair

## Outcome

The residual post-Vivify divergence was not another candidate-size policy.
Tactix used one quantity for two different jobs: the current Sweep call's
semantic allowance and the later scheduler's accounting of host work. After
the first productive environment, a whole variable-width-arena collection
added roughly 18.8 million representation words to that live allowance and
ended the call. Kissat's corresponding limit reads only cumulative
`kitten_ticks`; its in-place representation has no arena relocation to charge.

Tactix now separates those meanings. The live round ceiling and each embedded
environment's remaining allowance read `sweep_semantic_effort`, the cumulative
Kitten counter. The scheduler continues to read `sweep_effort`, which prices
both Kitten and representation work. A connected dense Sweep call carries one
ordered substitution/occurrence ledger across productive environments and
collects the arena once at call exit. Sessionless calls retain immediate
collection because their owned occurrence snapshots contain raw relocatable
references.

Release passes 72/72. Candidate-only job `1637687` preserves the localized
semantic consequence boundary: 343 equivalences, 686 emitted binaries, 369
retirements, and the same final conflict-3069 slot state. Sweep falls from
53.072 s and 1,100,956,356 representation words to 24.570 s and 4,720,714
words. The optimized call visits 2,306 retained seeds rather than the 2,303 in
the prior candidate and retained Kissat boundary; the extra three produce no
environment consequence or final-state change and remain the next scheduling
detail.

Matched 28B+5C job `1637688` passes all 66 tasks. All 34 SAT models and 32
UNSAT proofs verify, with no missing, malformed, cleanup-bad, or residual
certificate task. The previously failed candidate `T105.2.0` task now proves
UNSAT in 1043.578 s. Immutable rank-27 witness `1637757` remains UNKNOWN at
1200.004 s; its 196,083,712-byte partial proof was hashed and deleted. Rank 27
therefore remains open.

## Frozen identities and discipline

- Immutable expected-UNSAT input:
  `tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz`.
- Compressed SHA-256:
  `5435b1b271b049ed9291ab1b9b297afb33b8d2d3c50ff1b48bfaed4ddf7bb4e9`.
- Accepted binary SHA-256:
  `b6c991e099f4fa9cc635bacb20d7aa44ff3fdc23d0af78974b12998cecd6fb6f`,
  source `7b527fbc711d664898af8e9c5a9655c3bdf4204b`.
- Candidate binary SHA-256:
  `f9206a95ec91448daf626fc056c4d71e910d6e36f61b1f9362bad317856adbb9`,
  base `7eb4610fecac5f6cb6efd59677ae0ad451c61971`, source-diff SHA-256
  `b0198edb57cfe29e27b092bf874bab84a5ea9aa153a973532bd84055539431d7`.
- Retained Kissat trace SHA-256:
  `a6ffd4e0d4fa27e5c07c45a1b53a3f1e049bad3bd869b49dd929a9adf842e59c`.

The branch and `origin/main` began synchronized. Surviving worktrees and the
unrelated SLURM job were inspected and left untouched. The frozen rank-21--40
manifest, inherited closure reports, and accepted candidate pin were read
before changes. No frozen reference solver was rerun. All solver work ran on
SLURM: candidate-only localization used 16 GiB/300 s; the guard and witness
used the pre-Phase-4.5 16 GiB/1200 s contract.

## Localization from the common state

The retained Kissat trace and the first Tactix semantic-dose diagnostic agree
on the second Sweep call's 2,303-seed consequence boundary: 343 equivalences,
686 binaries added, and 369 variables eventually retired. Tactix nevertheless
spent 53.072 seconds in Sweep and charged 1,100,956,356 representation words.

The exact ceiling diagnostic ruled out a too-small slot allowance. At the call
boundary the semantic dose was 19,189,800 ticks. The first productive batch
then called `SweepSubstitutionSession::collect`; Tactix's whole arena contained
about 18.8 million words. The former live-loop meter was
`sweep_ticks_sub + sweep_ticks_repr`, so that representation bridge consumed
the remainder even though Kissat's outer loop compares only
`statistics.kitten_ticks` with `sweeper.limit.ticks`.

Changing only the meter restored the intended semantic tail, but collected the
complete arena once per productive environment. It matched the retained 2,303
seeds and 343 equivalences, yet charged 1.10 billion words, took 53 seconds in
Sweep, and failed the mandatory guard: candidate `T105.2.0` timed out at
1200.004 s with a deleted 1,182,793,728-byte partial proof. No Tier-D witness
was run from that failed guard.

Source inspection then showed the safe transaction boundary already existed.
`SweepSubstitutionSession` owns an ordered dense occurrence ledger. Its
inherit/remove/collapse/sync operations update both that ledger and the live
dense index after every environment. `sweep_env_build` consumes the ledger when
it is present. Therefore the next environment sees prior substitutions without
relocating the complete arena; collection is required only once when the dense
call ends. A sessionless call lacks that ledger, so its immediate collection
and generation-based occurrence refresh remain correctness requirements.

## Small general repair

The production change is confined to Sweep's effort and representation
lifecycle:

1. `sweep_semantic_effort` returns cumulative embedded Kitten ticks and owns
   the live round cutoff, trace spending, and remaining per-environment budget.
2. `sweep_effort` remains embedded ticks plus representation words and remains
   the policy scheduler's cost.
3. A productive dense environment updates the shared substitution ledger but
   does not collect the arena. The resumable call collects once after the
   reference-ordered environment sequence.
4. A sessionless/public environment still collects immediately, begins a new
   session, and refreshes raw arena references.

No input identity, benchmark name, hash, rank, literal, deadline, or tuned
constant enters the solver. The repair follows the reference source's
Kitten-only live ceiling and Tactix's general representation invariants.

The focused regression constructs two disjoint productive environments under
a connected dense index and requires at least two equivalences, at least two
substitutions, and exactly one arena-generation increment. The existing
sessionless regression retains the opposite required behavior. The first test
draft omitted `kernel_dense_connect` and therefore correctly observed the
sessionless path; fixing the fixture made the intended distinction explicit.
No unit test or fuzzing revealed a soundness defect or crash, so the plan's
mandatory Claude repair-delegation trigger did not fire.

## Diagnostic result

Campaign
`benchmarks/runs/20260821-2100-tierd-rank27-sweep-deferred-bridge/`, job
`1637687`, stopped after conflict 3069 under the candidate-only 300-second
contract.

| measure | per-environment collection | one dense-call collection |
|---|---:|---:|
| retained seeds | 2,303 | 2,306 |
| equivalences | 343 | 343 |
| emitted binaries | 686 | 686 |
| retired variables | 369 | 369 |
| representation words | 1,100,956,356 | 4,720,714 |
| Sweep time | 53.072 s | 24.570 s |
| final active / unassigned | 429,499 / 429,303 | 429,499 / 429,303 |
| final binary clauses | 751,105 | 751,105 |

The three extra visited seeds are zero-effect scheduling observations. They do
not change the call's semantic consequences or final formula snapshot, but the
count is not claimed exact. It is the next bounded localization point.

## Full B+C guard

Campaign `benchmarks/runs/20260821-2145-BC-sweep-deferred-bridge/` ran the
accepted pin and candidate over every Tier-B fixture and the inherited fixed
five mechanism-relevant Tier-C fixtures. Job `1637688` completed all 66 tasks.

| arm | valid | SAT / UNSAT | aggregate / PAR-2 | peak RSS |
|---|---:|---:|---:|---:|
| accepted pin | 33/33 | 17 / 16 | 2205.151 s | 3.234 GiB |
| candidate | 33/33 | 17 / 16 | 2924.991 s | 3.081 GiB |

The ratio is 1.326436. There are no solve losses relative to the accepted pin,
so the correctness iteration guard passes, but the aggregate is not an
accepted performance pin. `T105.2.0` is the decisive recovery: accepted proves
UNSAT in 790.936 s; candidate proves UNSAT in 1043.578 s. Both proofs verify
and are deleted. `mp1-Nb5T15` candidate remains checked SAT at 713.950 s.

## Rank-27 witness and next action

Only after the full guard passed was campaign
`benchmarks/runs/20260821-2230-tierd-rank27-sweep-reference-lifecycle/`
submitted as job `1637757`. It returned UNKNOWN at 1200.004 s. The runner
recorded a 196,083,712-byte partial proof, SHA-256
`b5383d3018f5ca82db6cc2e8faa538a404e9de892e8d94e13f9057bc9e054a15`,
then deleted it and all derived scratch. Certificate residue is zero.

Rank 27 is not added to the nine closed ranks. The next localization retains
the closed Vivify scope/deduction, TransitiveReduce source order, and Sweep
semantic-consequence/transaction boundary. It begins with the three zero-effect
retained seeds, then proceeds to the earliest downstream common-state
divergence. Any further source repair again owes the complete 28B+fixed-C guard
before another Tier-D witness.
