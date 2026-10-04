# Certified UNSAT verticals: public Direct PHP and private GF(3)/Tseitin frontends

## Disposition

**IMPLEMENTATION CHECKPOINT; NOT BENCHMARK ACCEPTANCE.** Three independent
increments advance theory reasoning into certified Boolean/proof actions. Direct
PHP now completes one public controller vertical. GF(3) Gaussian elimination and
Tseitin exact-XOR discovery reach authenticated live publication privately, but
still lack public controller boundaries. No target, WV/WV2, Tier-D, or regression
benchmark was run, and the saved `d170b4a5` comparison was not rerun.

## Direct PHP public terminal action

Commit `e4d76b336` appends `DirectPhpRefutation` at registry ID 36, disabled
StepId 25, and C parameter tag 39. One canonical request carries exact snapshot,
recognition, certificate, and replay bounds. CLI and C API solvers own a
Kernel-bound noncopyable session. The online serializer/replayer, explicit
controller, and random controller execute the same atomic action; DefaultPolicy
does not schedule it.

The adapter recognizes a complete direct `(H+1)`-pigeon/`H`-hole encoding and
publishes its recursive RAT/RUP refutation in no-proof, DSR, or DRAT mode. Success
is terminal UNSAT and cannot have a propagation follow-up. Raw feedback records
every status, capture/recognition/replay effort, and proof additions, deletions,
literals, fresh variables, events, and maximum event width.

The CLI statistics surface prints canonical action attempts separately from
successful applications and decline classes. The fuzz reachability ledger keys
`DirectPhpRefutation` on attempts, which move only after the public action
crosses its execution boundary; it can therefore require real random-policy
reach rather than treating registration as coverage. The effort ledger records
the action as typed, public, default-disabled and explicitly governed.

Input finalization intentionally releases non-LRAT binary IDs. Such binaries now
receive tagged deterministic capture-local replay identities. Those tokens are
zeroed at the `Proof` boundary, where DSR/DRAT deletes by literal multiset; they
are never exposed as LRAT references. The C API test exercises parser-finalized
input and DSR publication end to end.

## GF(3) chained Gaussian live publication

Commit `86d142984` adds a private live adapter around the existing authenticated
Gaussian bridge and RUP certificate. It captures an exact live GF(3) owner and
supports chained `Source`/`Scale`/`Add` elimination, rather than only eliminating
one original equation pair. It independently rebuilds and compares the complete
certificate and physically revalidates source occurrence, clause, and proof
identity before atomic publication.

Only derived units or an empty conflict enter the solver. Intermediate RUP
clauses remain proof-only; no Boolean auxiliary or PB step is introduced. DSR,
DRAT, and no-proof modes are supported; LRAT/PBP and proof-backed split
publication decline. The transaction binds every nested limit and its journal
sink. The journal records exact source premises, authenticated equations,
reduced row/RHS, derivation and original-source combination, plus every proof
event and installed clause. Nonunit Gaussian rows remain unsupported.

The chained fixture proves that no original equation pair suffices: from
`x+y=0`, `y+z=0`, and `x+z=1`, the adapter derives `x=2`, `y=1`, and `z=2` and
independently RUP-checks their Boolean units. A separate inconsistent system
publishes the empty clause and returns UNSAT.

## Tseitin autonomous discovery

Commit `048442c40` adds a private bounded frontend that captures the complete live
binary and large-clause population, derives exact width-2--4 XOR groups, and
passes an immutable plan to the existing live X7 adapter. It rejects opposite
parities for one scope, incomplete encodings, bad edge degree, even total charge,
residual edge touches, stale state, interruption, and budget exhaustion. Group
identity and traversal are deterministic. The live adapter independently
recaptures and authenticates the supplied plan before ordinary X7/DSR
publication.

The cautious first frontend accepts one connected XOR population. It declines a
formula with multiple disconnected populations rather than selecting a hidden
component. A future public action must expose component identity, parameters,
replay, feedback, and scheduling explicitly.

## Validation

Each increment passed its focused Debug and ASan/UBSan matrix. After combining
all three commits, fresh `-O1 -D_GLIBCXX_ASSERTIONS` and ASan/UBSan builds each
passed the same 17/17 focused tests covering Direct PHP
action/live/public/API/random execution, GF(3) bridge/certificate/live
publication, Tseitin admission/discovery/live/planning/transaction, scheduling,
serialization, API instances, and the inprocessing program. Sanitizer execution
used `detect_leaks=0` because LSAN is unsupported in this environment. The
documentation/configuration suite also passed, including action inventory,
feedback, registry/policy, effort-governor and fuzz-reach conformance checks.

That combined run exposed two stale expectations left by the earlier affine
scheduler repair: one C API fixture invoked Lucky-8 outside its restored
structured-root lifecycle, and the inprocessing test still expected three
stages rather than the new leading affine stage plus three structural stages.
Commit `5c23c1f4c` updates the tests to assert the intended lifecycle; both then
pass. No solver correctness defect was involved.

## Remaining obligations

- Add a public GF(3) action/session with typed outcome instances, exact replay,
  raw feedback, repeated eligibility, and controller scheduling.
- Add a public Tseitin component payload/session without hiding component
  selection inside discovery.
- Measure whether these mechanisms recognize and recover relevant fixed-44,
  WV100, and WV2 rows; no recovery is claimed here.
- Run current-source full Debug, effective default/random fuzz with new-action
  reach, B28+C5, Rank1--50, WV100, and WV2--89 before acceptance.
- Preserve the 1,800-second checker cap and completed-proof slowdown rule, and
  audit N1--N4 on the intended enabled configuration.

The immediate benchmark prerequisite remains the repaired affine scheduler's
three trajectory controls (`cabp`, Break, and Timetable) when benchmark execution
is authorized. These implementation commits do not authorize or replace that
measurement.
