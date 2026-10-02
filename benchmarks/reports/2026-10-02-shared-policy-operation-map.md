# Shared policy execution: initial operation mapping

This code audit starts the feedback-preparation goal. Checkout `c640c649` has
unchanged solver source from accepted `12e3c4f0`. The
[inventory](2026-10-02-shared-policy-operation-map/inventory.json) records source
digests and direct execution/support call sites. It is lexical evidence,
supplemented by the manual mapping below; it does not establish runtime reach,
shared-execution completion or new correctness/performance acceptance.

## Existing mechanisms and missing representations

| Operation | Existing DefaultPolicy path | Existing random-controller path | Migration requirement |
|---|---|---|---|
| Solve entry and session ownership | configure/reset, stop publisher, analysis-tier callback, optional arena order and delayed H attachment | stop publisher, owned H transfer, immediate controller attachment | Shared session/execution ownership with policy-selected lifecycle; preserve delayed cold-state/H attachment in default |
| Entry trail normalization | `restart_backtrack_to(0)` without scheduled-restart statistics | `full_restart()` with scheduled-restart statistics and strategy epoch | Typed restart purpose must preserve each path's current counters |
| Startup certificate selection | Lucky parity/count before certificate closure, then commit DSR if not terminal | startup Lucky IDs can be offered through their existing certificate guards | Explicit startup actions and certificate-boundary service; preserve original-input PBP versus DSR and decline-before-mutation rules |
| Structural startup model/proof actions | SignedCardinality, ExactCover, IndependentSet, Fermat, Cerny, LookupCircuit, Simon, Stconn, BalancedSignature, Parity, ExactOne, DirectCSP and Onehot native calls | typed Lucky semantic instances and configured parameters | Shared typed Lucky execution; verify native-instance correspondence rather than route every opportunity through canonical Lucky |
| Early/late ordinary Lucky | two scheduled opportunities, root-publication handoff, delayed H | ordinary eligible root Lucky | Policy owns opportunity/order; executor owns identical application/publication mechanism |
| Structured root program | `OnlineStrategySequence`, ordered stages, producer-sensitive continuation allowance | per-family `OnlineStrategyExecutor` with fixed atomic doses | Shared atomic execution/continuation; preserve separate policy-selected composition and discovery profiles |
| Startup BCE cleanup | conditional native BCE after unique-literal AMO producer outcomes | ordinary eligible BCE | Expose preparation and configured execution without replacing the default producer-dependent decision |
| Cold-state/residual guidance setup | delayed materialization; conditional residual activation order; H attachment and seed | controller-owned H attached at invocation entry | Shared support services and explicit policy lifecycle, not forced identical timing |
| Sparse root collection | repeated root collection/propagation until no Unit result | no authored matching startup sequence | Explicit support action with Refuted/Unit/Collected result and original identity preservation |
| Propagate | static native apply plus conditional root-assignment publication | registry apply plus registry publication/accounting | Shared static/dynamic execution with explicit publication/accounting ownership; no duplicate consumption |
| ConflictAnalysis | native Analyze, policy tier/glue updates, chronological/reuse target choice, native Backjump | registered fused canonical Analyze+Backjump, then controller clocks | Preserve one atomic public transaction; support default advice/target choice at existing internal points without exposing pending clauses as learner boundaries |
| Reduce | preparation, tier pose, ranked references, selected prefix, optional variable compaction, scheduler update | typed registered root invocation | Shared explicit victim payload and preparation; retain default ranking, precomputed compaction decision and exact cleanup order |
| Mode/configuration | native SwitchMode and reference clocks/random sequence at its scheduled boundary | all 20 named combinations through root ResumeSearch | General execution represents native mode changes; learner configuration changes stay at explicit full-root boundary |
| Scheduled Restart | next-variable policy used for reuse threshold, target capture if trail shrinks, `restart_apply_to(keep)`, clock updates | due clock then full restart, target capture, strategy epoch and root-loop entry | Shared parameterized mechanism; retain default next-variable/RNG behavior; integrate ordinary partial continuation into random controller |
| Root preparation backtrack | Rephase/Probe/Eliminate call `restart_backtrack_to(0)`, deliberately no scheduled restart credit | currently root preparation bundled with full scheduled restart | Explicit purpose/counter distinction; root entry is mandatory for root-only actions, not for every ordinary restart |
| Reorder | policy chooses queue/heap/combined target and updates its clock | registered eligible root operation | Shared typed target operation; retain scheduling and policy state |
| Rephase | slot pattern, root preparation/propagation, reference warm-up, optional structural seed, Walk or phase type | typed Best/Original/Inverted plus separate Walk/guidance root families | Shared constituent mechanisms and explicit composition; retain default warm-up and seed provenance |
| Warm-up | temporary assignments with active ranking, propagation beyond conflicts, root backtrack retaining saved phases | no corresponding controller composition | Support-service transaction with its own explicit identity/status; avoid changing proof-rule semantics or reporting speculative assignments as durable reward |
| Walk and local search guidance | native configured effort, branch RNG seed, structural seed rotation | typed native producers/configuration | Shared parameterized mechanisms; no hidden changes to generator or seed lifecycle |
| Probe/Eliminate entry/exit | flag-dependent root preparation, flush/connection scopes, cache lifetime, sparse resume | executor-local native caches and retained pending scopes | Extract neutral scope/publication services; keep default cache lifetimes and cursor/order exactly |
| Dosed rule instances | ranked batches, BVE invocation session, ledgers, candidate cursors | canonical/explicit instances through shared online executor | Common atomic invocation mechanism with representable native sessions, payloads, accounting and stop reasons |
| Vivify | cumulative tier-share ceiling, empty-band merge and separate irredundant delay | canonical native application with caller ceiling | Represent exact native dose plan; substituting independent fresh allowances changes default behavior |
| Sweep | `sweep_apply_exact_limits` with named configured profile | typed Sweep limits through online invocation | Share exact profile execution without retuning discovery or internal scope |
| Substitute | mandatory first round, productive-round continuation, one final watch reconnect | native canonical/explicit execution, different caller-selected granularity | Shared composed transaction and continuation; preserve default zero-allowance first-round and reconnect semantics |
| Strengthen/forward closure | partial scan, root publication, reconnect/flush, shared forward allowance | online typed application/native continuation | Preserve closure boundary, meter and touched-variable resumption; distinguish safe feedback from internal partial state |
| Dense/garbage/variable support | collection, occurrence scopes, compaction and watch rebuilding | native executors may own corresponding scopes | Explicit shared support machinery, with call timing and scope ownership chosen by policy |
| Decide | selected literal, native `external=false`, static apply, policy diagnostics | selected literal, native `external=false`, epoch/coverage/trace | Shared typed internal decision; using the public explicit instance accidentally increments API-decision counters |
| Termination/interruption | finish event/valid kernel, phase cleanup, default repeated-solve lifecycle | terminal transcript, retained committed kernel, explicit root normalization on re-entry | Shared status/feedback/session completion; preserve each policy's scheduling state and no rollback/fallback |

The default scheduler's current operation order is stateful. The code already
shares rule mechanisms, but merely wrapping `DefaultPolicy::solve()` as one
opaque action would leave the architectural gap intact. Likewise, treating
an entire Probe/Eliminate phase as one learner action would hide its authored
instances and per-application feedback. The execution layer must represent its
atomic applications and necessary support transactions.

## Migration structure

Use policy-neutral typed requests for native search, root invocations and
support operations, with one shared executor owning validated application,
continuation and mechanical publication. Default selects requests using its
preserved scheduler; random selection remains seeded and uniform under its
declared collection. Default adaptive budgets and random fixed allowances are
different policy profiles over the same mechanics.

Static execution overloads should call the shared native rule mechanisms
directly; instrumented execution adds records around those same mechanisms.
Do not allocate strings, enumerate instances or copy formula-wide state for
every default decision/conflict merely to satisfy a dynamic interface. Existing
diagnostic candidate descriptions stay lazy. Native rule statistics and new
feedback accounting must have explicitly separate ownership to prevent double
charges or accidental changes to the preserved default counters.

ConflictAnalysis needs special care: the default observes the learned clause
before choosing chronological/reuse backjump and before tier-sensitive analysis
continuations. Preserve that internal policy seam while emitting one completed
action result after the fused transaction. Ordinary external observation and
learning requests cannot enter the pending-clause state.

Restart requests include keep-level and purpose. Scheduled restart and
root-preparation backtrack share trail mechanics but currently differ in
restart-count semantics. Learner strategy changes and root inprocessing still
require full restart. Partial ordinary continuation keeps the installed search
configuration; it must not accidentally enter the root-action loop or advance
configuration/phase-installation clocks.

Feedback records bind action identity/sequence and before/after snapshots to
native work currencies, actual overshoot, duration and precise outcome. Search
interval records close at scheduled restart, terminal result or interruption;
root work is excluded. Raw counters remain available and observation remains
pure. The numerator, cross-rule effort currency, zero/terminal handling and
ResumeSearch reward boundary are open; no scalar reward or learning is added.

## Increment and verification order

1. Preserve the accepted pins, checkpoint and current deterministic sources.
   Introduce shared typed native execution, first for restart/decision, with
   focused checks for counter-purpose differences, retained prefix and root
   conflict discharge. This is scaffolding, not whole migration acceptance.
2. Migrate startup/support and search transactions, retaining default advice
   ordering and parameterized native effects. Compare action/state/proof/model
   evidence with the preserved default. Eliminate duplicated execution bodies
   when both policies have migrated; keep intentional policy differences.
3. Migrate inprocessing dose preparation, cursor/cache lifetime and constituent
   applications. Verify zero-dose/overshoot, native progress/staleness and
   interruption cleanup before broad validation.
4. Add replaceable selection/feedback seams, versioned records and offline
   analysis. Compare random requests and solver effects with logging enabled
   and disabled; time fields are recorded but not replay-equality fields.
5. Complete full Debug, Claude-owned fresh 100,000-seed legs for each policy,
   retained corpus and nonvacuous targeted coverage; complete the four fixed
   DefaultPolicy regression sets using their original resource/check contracts.
   Measure instrumentation overhead and review per-instance regressions.

Source checkpoint `12e3c4f0` and its immutable pins remain the migration
comparison point; no pin is rebuilt. Failed premises require a correction to
the migration, not a weakened gate or a permanent fallback path. Important
risks are hidden native scope state, doubled publication/accounting, changed
default warm-up/heap/RNG behavior, certificate-boundary drift and hot-loop
instrumentation cost. None has been declared resolved by this static audit.

The next implementation step is shared typed native restart/decision execution.
The overall goal remains incomplete: source migration, feedback, correspondence,
fresh validation, overhead evidence and the final Andrew handoff are required.
