# Public authenticated GF(3) Gaussian batch action

## Disposition

**IMPLEMENTATION CHECKPOINT; BENCHMARK ACCEPTANCE REMAINS OPEN.** Commit
`cd956aea` exposes the previously private authenticated GF(3) Gaussian live
publisher as public rule X14 `Gf3GaussianConsequence`. No solver benchmark,
fixed-44 recovery run, WV/WV2 run, or regression campaign was launched for this
checkpoint. The saved `d170b4a5` comparison remains untouched.

## Public action contract

The append-only identities are registry ID 37, disabled reference StepId 26,
and C parameter tag 40 in additive API version 6. DefaultPolicy excludes the
rule. Explicit programs and the opt-in random-controller producer use the same
typed parameters, shared executor, replay serializer, and cumulative feedback.

Discovery offers exactly one deterministic `g3b1:` identity. It commits the
complete simultaneous Gaussian closure: every supported unit or conflict,
every `Source`/`Scale`/`Add` provenance step, every authentication reference,
and every exact source clause. A per-consequence action would be incorrect at
this engineering boundary because publishing the first root unit closes the
assignment-free guard and can strand the other consequences. The batch keeps
all deductions from one authenticated closure reachable in one atomic action.

Offer generation is mutation-free. Apply captures the current formula again,
rebuilds the complete algebraic and Boolean certificate, exact-matches the
selected identity, and only then publishes all supported units or the empty
conflict. Corrupt, stale, cross-kernel, interrupted, exhausted, and unsupported
proof requests decline before solver, proof, journal, or session mutation.
Duplicate keys are retained in one Kernel-bound session. Identity generation
and total retained session identity bytes have separate limits and accounting.

## Proof and model behavior

The action publishes only entailed unit clauses or the empty clause. Bounded
case splitting produces ordinary RUP additions; intermediate clauses stay in
the proof and never become branchable solver variables. No RAT definition,
auxiliary search variable, PB step, or reconstruction entry is introduced.
No-proof, DSR, and DRAT modes are supported. LRAT and PBP decline before
mutation. Nonunit Gaussian rows remain unsupported.

The provenance journal binds each action and semantic identity to its typed
limits, exact source population, authenticated equations, reduced rows,
original source combinations, proof events, installed clauses, and work. Raw
feedback separates source capture, bridge, certificate, validation,
revalidation, proof, journal, identity, session, unit, conflict, and terminal
status counters.

## Validation

Independent review verified the mutation boundary and complete-batch behavior.
The current Debug build passed 7/7 selected integration tests:
`gf3_gaussian_live`, `gf3_gaussian_public`, `api_instances`,
`online_all_actions`, `inprocess_program`, `random_online`, and `scheduling`.
The public test covers multi-unit atomic publication, corrupt/stale identities,
cross-kernel session rejection, both byte limits, typed serialization/replay,
online execution, random reach, C v6 deep-copy/overlength rejection, and DSR
publication. A fresh ASan/UBSan build passed the live and public tests 2/2 with
`detect_leaks=0` because LSAN is unsupported under the environment's tracing.

The feedback inventory reports 699 native fields and no wall-clock fields. The
effort-governor checker classifies X14 as an explicit typed action with an
authored disabled default row. The fuzz reach ledger keys it on the CLI
`gf3-gauss` attempt counter, which moves only when an exact batch crosses the
shared executor. Catalog, calculus, implementation-ledger, API-version, and
proof-rule-guide synchronization checks pass.

## What this does and does not establish

This completes the public/controller boundary for chained GF(3) unit and
conflict reasoning. It does not yet show recognition or recovery on a
LymphoSAT-owned instance, improve the partial campaign, or satisfy effective
both-policy fuzz and the four regression gates. The earlier partial campaign
remains sound but incomplete: six direct structured MDP rows recovered with
checked models; `cabp` was slower, Break and Timetable timed out, and phase
guidance was never offered or used. No wrong verdict or checker failure was
observed there.

## Next obligations

Expose Tseitin connected components as exact typed payloads without hidden
component selection. Then, when benchmark execution is authorized, run the
three trajectory controls and targeted owner subsets before B28+C5,
Rank1--50, WV100, and WV2--89. Acceptance still requires effective default and
random fuzz with X14 reach, independent proof/model checking, all fixed 44
targets, the 1,800-second proof-check cap and slowdown rule, and a current
N1--N4 audit.
