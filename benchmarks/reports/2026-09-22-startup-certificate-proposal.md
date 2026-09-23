# Startup refutation certificates: evidence and proposed decision

Prepared from checkout `2bf82dca`; 22 September 2026 America/New_York.
Proposal only: no solver, calculus, or plan contract has changed.

## Retained measurements

The owner measurements below come from the frozen baseline
`benchmarks/baselines/sc24-26-32g-5000s/results.json`, selecting `lymphosat`
and the named instance. All four records have `valid=true` and a verified
`veripb-cake-pb` pipeline. Certificate sizes are bytes, not resident memory.
The original certificates were deleted after checking; the records preserve
their sizes, hashes, stage results, and checker provenance. No reference rerun
was needed or performed.

| Instance | Owner PBP bytes | AM333 DSR bytes | DSR/PBP | Owner complete check (s) |
|---|---:|---:|---:|---:|
| `FmlaEquivChain_4_8_8` | 436,975,142 | 4,503,640,567 | 10.31 | 114.937 |
| `tseitin_grid_n250_m250` | 113,986,935 | 872,274,882 | 7.65 | 251.112 |
| `tseitin_d3_n100000` | 51,756,226 | 1,741,406,734 | 33.65 | 383.788 |
| `tseitin_grid_n400_m400` | 297,807,533 | 2,625,028,748 | 8.81 | 2,432.090 |

AM333 sources: `benchmarks/runs/20260921-am333-lymphosat-models-wv100/`
`provisional/{079,082,085}.json`, and the sibling
`20260921-am333-lymphosat-models-wv2-89/results/030.json`.
The latter is verified, with 1,113.544 s solving and 3,625.216 s checking;
it misses WV2-89's 600 s solving target. These are retained measurements across
campaigns, not a fresh same-window performance comparison.

The routed Tseitin checks have now ended. In
`benchmarks/runs/20260922-am334-certificate-routing/out/{1,2,3}.tsv`, all
three are `NOT-VERIFIED`, after 40,264.2 / 40,541.6 / 40,798.2 s respectively.
Logs `1714785_{1,2,3}.log` report drat-trim failure and `s TIMEOUT`.
SLURM reports those tasks COMPLETED; the checker verdict, not the shell exit,
determines acceptance. `1714785_4` was the first ncc check, not a Tseitin task.
The ncc replacement `1715197_1` remains running at this checkpoint.
The frozen AM334 report is preserved; this paragraph supplements its pending rows.

## Owner mechanism and integration implications

Source root: `winners/lymphosat-sc2026/green/lymphosat/src/solver/solvers/`.
Read `equivalence-chain-principle/solver/README.md` and its complete
`solver.cpp`. The detector infers generator parameters from dimensions and then
compares every literal and clause in generator order. That detector is too
specific for Tactix's structural-rule contract.

The useful mechanism is local count preservation. Exact-one symmetric child
selectors choose an involution, and guarded equivalences carry leaf values
through the chosen swap. The proof enumerates complete involutive matchings,
uses PB proof-by-contradiction and RUP to establish each gate's non-increasing
true-leaf count, and sums those bounds through successive layers. Initial
negative units bound the count; final positive units contradict it. For the
4/8/8 encoding, 64 initial true leaves cannot become 4,032 final true leaves.
Tactix must recognize the selector/equivalence relationships from clause content,
retain actual original clause IDs and external literals, and support renamed,
sign-flipped, clause/literal-shuffled encodings. No generator dimensions,
filenames, variable-number arithmetic, or exact clause order may select the rule.

In `tseitin-formulas/solver/solver.cpp`, `gen_parity_sum_proof` (lines 758--1943)
requires odd total parity and each participating variable occurring twice.
It derives small XOR components using fresh variables, proves local PB bounds
with redundancy witnesses, sums both bound directions, divides by two, and
obtains contradiction. This is a certificate mechanism distinct from changing
the terminal decision: Tactix already finds these parity contradictions.
The implementation permits degrees through seven despite an older comment
claiming a smaller scope; an integration must follow checked derivations.

## Alternatives and recommendation

**A: retain DSR for every UNSAT run.** Add typed count-preservation and parity
actions whose derivations expand into clausal DSR. This retains the current
format contract. The existing parity encoding has the size/checking failures
above; a faster new clausal encoding remains possible but is not established.
There is no measured clausal implementation of the owner's counting invariant.

**B (recommended): permit a terminal startup PBP certificate.** Preserve N2's
independent proof requirement and use the owner's complete pinned pipeline:
VeriPB 3.0.2 (`2e5fa5e...`) elaborates against the original CNF, then
CakePB `6edb2f3585f1c3e1b925964c4f33e133f7279d02`
(`cake-pb-cnf` binary `c859a9c...`) independently checks the kernel PBP.
Use the frozen baseline's 24,576 MiB CakeML heap inside the 32 GiB allocation.
VeriPB success alone does not accept a certificate. See
`tools/bench/frozen_baseline.py`'s PB branch for exact invocation and pin metadata
in the baseline manifest. This adds a proof system and checker dependency; it
does not guarantee that Tactix will reproduce the owner's time or proof size.

Proposed contract for approval:

1. Add a typed automatic certificate mode, used by the default invocation,
   that can select PBP only for a complete startup refutation derived from the
   original input. Explicit DSR/DRAT/LRAT selections keep their format meanings;
   an unsupported PB-only action declines before mutation. DSR remains the
   canonical stream for every path entering preprocessing/search without a
   complete PB refutation.
2. Selection is atomic and precedes any published proof event. Discovery is
   bounded, interruptible, and formula-neutral. Validate a complete derivation
   plan before emission; failed discovery resumes ordinary DSR solving with
   the same formula. After publication starts, never switch formats or combine
   a DSR prefix with PBP. Output failure/interruption cannot claim UNSAT.
   Keep streaming bounded; do not buffer a complete certificate in memory.
3. The startup boundary must precede input-normalization and root-proof writes,
   not merely the existing Lucky call: `main.cpp` attaches the proof before
   parsing, and DefaultPolicy propagates before its parity action. Preserve
   original-CNF IDs and normalization provenance, including duplicate, unit,
   tautological, and empty clauses. Do not delay arbitrary DSR events in a
   hidden whole-run ledger. No-proof runs choose the same logical actions.
4. Expose count/parity refutations through shared registered typed actions at
   the declared startup boundary, callable by default, scripted, replay, and
   random policies. Candidate discovery and all effort limits are strategy;
   rules independently validate premises. Once DSR is committed, retain
   existing clausal parity capability; PB format availability is not a
   benchmark-dependent guard. This boundary and compatibility behavior need
   explicit N1 review in the calculus before implementation.
5. Acceptance recognizes the declared certificate's content, checks against
   the original CNF, and requires the appropriate complete pinned pipeline.
   Preserve current witnessed-DSR and witness-free-DRAT routing. Extend runner
   pins, result metadata, replay, and Claude-owned fuzz oracles together.

## Approved-change scope and validation plan

If B is approved, revise plan R1, sections 4/5 (vocabulary and boundary), the
DSR-only acceptance wording in active order/section 8.3, and the catalog/API
contract where needed. Clarify N2's canonical-format sentence without weakening
its proof obligation. Synchronize CLAUDE, calculus, implementation notes, and
affected checker consumers through their owning skills. No plan edit is made here.

Implement the certificate lifecycle and parity path first, then the general
count-preservation recognizer/proof. Test original-CNF mapping, explicit action
reachability, replay, format locking, interruption, failed writes, and ordinary
DSR fallback. Include SAT near-misses, missing/changed gate clauses, wrong
counts/parities, renamed/flipped/shuffled inputs, corrupted/truncated proofs,
and deliberate rejection controls through both PB checker stages. Claude owns
default/random-policy fuzz execution and reporting.

Finally run the four gates on immutable pins: Rank 1--50, B28+C5, WV100,
WV2-89, preserving the existing runtime, memory, coverage and independent
acceptance contracts. Stage all DSR/DRAT and PB checkers on the maintained
routed runner; no full Tier-D campaign. Do not claim new gate coverage until
every required certificate has passed. No solver jobs were launched for this proposal.
