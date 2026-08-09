# Default-strategy application-order audit

**Date:** 2026-08-09
**Scope:** the complete shipped-default lifecycle, not only the N4 option
ledger. This is a static and fixed-fixture audit; it changes no solver
behaviour.
**Reference pins:** Kissat `8af8e56f174b`; CaDiCaL `7b99c07f0bca`.

## Result

The high-level CDCL precedence and the split probe/eliminate clocks follow
Kissat. The authored inprocessing list is no longer accidental. The fresh
call-site walk nevertheless reopens N4: three deduplicated, actionable root
causes remain.

1. **`ORDER-DEFAULT-1` (high):** Tactix implements Reorder but ships it off;
   Kissat ships `reorder=2`, meaning both modes.
2. **`ORDER-LUCKY-1` (medium):** Tactix offers Lucky once, before its initial
   inprocessing phase. Both references offer an early and a late Lucky pass,
   bracketing preprocessing.
3. **`ORDER-FOLLOWUP-1` (medium):** the scheduler represents pipelines as a
   flat list and cannot express “run Substitute/Decompose only if the producer
   changed the formula.” Its two unconditional preprocessing Substitute slots
   are therefore neither Kissat's nor CaDiCaL's pipeline.

Everything else below is an exact match, a stated Kissat tie-break, or an
accepted stronger/weaker deviation. In particular, the extra Tactix proof
rules are not multiplied into separate findings: their common cause is the
project's larger rule inventory.

## Method and evidence boundary

The audit followed executable calls from each solve entry through startup,
conflict handling, the no-conflict event chain, and both inprocessing groups.
Option headers were checked only after locating the call-site predicate. The
existing N4 ledger was used as a cross-check after reconstruction.

Primary Tactix evidence is `DefaultPolicy::solve`
(`src/policies/default_policy.cpp:880-1040`), `AUTHORED_SLOT_ORDER`
(`:384-403`), preprocessing admission (`:857-867`, `:1059-1078`), and the
phase driver (`:1430-1533`). Primary reference evidence is Kissat
`src/search.c:179-224`, `src/preprocess.c:14-120`, `src/probe.c:14-103`, and
`src/eliminate.c:426-545`; and CaDiCaL `src/internal.cpp:276-331,742-805,952-1003`,
`src/probe.cpp:920-987`, and `src/elim.cpp:1015-1108`.

The shipped option defaults relevant to the reopened findings are Kissat
`reorder=2`, `lucky=1`, `luckyearly=1`, `luckylate=1`,
`preprocessrounds=1`, `proberounds=2`, and `fastel=0`
(`kissat/src/options.h:62,77-79,94,99,117-119`). CaDiCaL likewise ships
`lucky=1`, `luckyearly=1`, `luckylate=1`, and `preprocesslight=1`
(`cadical/src/options.hpp:152-155,161-162`).

## Canonical lifecycle matrix

“C” means conditional; “U” means an unconditional call once the enclosing
event is admitted. State names in the last phrase make adjacent hand-offs
explicit.

| Position / trigger | Kissat shipped order | CaDiCaL shipped order | Tactix current order | Call / repeat / adjacent state | Verdict · severity |
|---|---|---|---|---|---|
| Solve entry and root closure | inconsistency → early Lucky → preprocessing (which initially propagates) | `already_solved` root propagation → local search → early Lucky inside `preprocess` | inconsistency → root propagation → Lucky | Lucky is C in all three. Tactix passes the propagation fixpoint into Lucky. | deliberate reference-compatible placement · low |
| Preprocessing envelope | early Lucky → one reduced probe round → late Lucky → classify | local search → early Lucky → quick preprocess → late Lucky → local search | Lucky → one reduced probe-group phase → search | References both bracket preprocessing with Lucky. Tactix has no late call; its `lucky_tried` boolean forbids another opportunity. | **missing conditional wire `ORDER-LUCKY-1` · medium** |
| Initial simplification pipeline | Congruence; if changed Substitute(eager); Backbone; Sweep; if changed Substitute(eager); otherwise one final Substitute; Factor. `fastel=0`. | ExtractGates; if changed Decompose; Backbone; Sweep; if changed Decompose; Factor; then default-on FastElim | Congruence → Substitute → Backbone → Sweep → Substitute → Factor; no elimination phase at conflict zero | One outer round in all relevant paths. Tactix calls both Substitute slots U once admitted; reference consumers are C and Kissat calls at most one final non-eager Substitute when neither producer fires. | **missing conditional wire `ORDER-FOLLOWUP-1` · medium**; FastElim exclusion is a deliberate Kissat choice |
| Search propagation / conflict hand-off | propagate → integrated analyze/learn/backtrack/assert | propagate → integrated mode-specific analyze/learn/backtrack/assert | Propagate → Analyze → policy observations/retier → Backjump; next loop propagates assertion | Tactix's `pending` carries learned clause, glue, asserting literal and jump level from Analyze to Backjump. The hand-off is mandatory before another Propagate. | exact semantic order; split rule boundary is deliberate · low |
| Satisfied check | after propagation/iteration, before scheduled events | after internal/external propagation, before limits/events | after propagation/conflict handling, before events | U at a propagation fixpoint. | exact match · low |
| No-conflict event precedence | Reduce → SwitchMode → Restart → Reorder → Rephase → Probe → Eliminate → limits → Decide | limits/termination → Restart → Rephase → Reduce → Inprobe → Elim → Compact → Condition → Decide | Reduce → SwitchMode → Restart → Reorder → Rephase/Walk → Probe → Eliminate → stop → Decide | One event per loop because every applied event continues. | deliberate Kissat tie-break · low; stop placement is an accepted weaker responsiveness deviation |
| Reorder trigger/default | root, scheduled after Restart and before Rephase; `reorder=2` enables both modes | no counterpart in this event chain | same position and root gate, but environment default is mode 0/off | Conflict watermark is preserved across calls. | **missing default-enabled mechanism `ORDER-DEFAULT-1` · high** |
| Rephase / Walk composition | scheduled rephase rotation; Walk occupies reference-defined slots | Rephase before Reduce; local search also appears before and after preprocessing | restart-to-root if needed; six-slot Best/Walk/Inverted/Best/Walk/Original rotation | Walk receives a tick budget and hands its phases back to Rephase. | deliberate Kissat-derived composition · low |
| Probe/eliminate precedence and clocks | Probe before Eliminate; independent NLOGN/NLOG2N clocks; both refuse same-conflict Reduce | Inprobe before Elim; independent clocks; Inprobe needs a newer Reduce | Probe before Eliminate; independent clocks and material watermarks | Phase entry restarts to root, propagates, connects dense occurrences, then disconnects and flushes on exit. | exact shared precedence; Kissat cadence choice · low |
| During-search probe pipeline | Congruence → Substitute → Backbone → Vivify → Sweep → Substitute → Transitive → Backbone → Factor; up to 2 rounds, stop if active count unchanged | Deduplicate → Decompose → Ternary → C Decompose → Probe/HBR → C Decompose → Gates → C Decompose → Backbone → Deduplicate → Sweep → C Decompose → Vivify → Transred → Backbone → Factor | Congruence → Substitute → HBR → Backbone → Probe → Vivify → Sweep → Substitute → Transitive → HyperTernary → Backbone → Factor; up to 2 outer rounds | Tactix propagates after every successful entry and repeats while active count changes. Extra rules are separate proof-rule applications, not hidden internal calls. | deliberate hybrid plus accepted stronger rule inventory; conditional-edge portion is the same `ORDER-FOLLOWUP-1` root cause · medium |
| Elimination entry | Forward subsume/strengthen before each BVE round | Ensure a complete Subsume phase, then BVE round | SubsumeForward → Strengthen → BVE | Forward pair shares one allowance in Tactix; occurrence state feeds BVE. | exact Kissat choice · low |
| Elimination internal cycle | at most 2 rounds; Forward → BVE; repeat if incomplete/productive and budget remains | at most 2 rounds; BVE, then first productive of Subsume, Block, Cover causes next BVE round | up to 2 flat outer rounds: Forward pair → BVE → Instantiate → BCE → CCE | Tactix's post-BVE rules are default-on extensions; it does not mimic CaDiCaL's first-productive `continue` chain. | accepted stronger/weaker deviation · medium; reopen only on correctness or cumulative regression |
| Per-entry propagation and phase cleanup | Simplifiers maintain their own root closure; probe/eliminate wrappers backtrack and flush | Wrappers backtrack, propagate, and reset/rebuild watches | successful rule → Propagate; phase exit GC only after dosed change, then flush retired redundant, consume retirements, disconnect | The next rule sees root consequences of the prior successful rule. | exact state-handoff intent · low |
| Phase repeat / fixpoint | Probe max 2, active-count fixpoint; Eliminate max 2 plus completion/budget | Technique-specific repeats; Elim max 2 plus first-productive follow-ups | Probe and Eliminate max 2; break when active count unchanged and no pending subsume hold | U outer cap; C repeat. | accepted coarser fixpoint deviation · low |
| Admission, delay, effort | Per-technique enable, private meter, phase anchor, 10M minimum; phase/material watermarks | Per-technique tick/propagation budgets, thresholds/delays, distinct clocks | Private meters, shared phase anchor, 10M minimum except Substitute, per-entry delay and material marks, split clocks | Limits are calculated at entry; Substitute's over-budget first round suppresses later rounds. | mechanism present; family-specific accepted deviations remain in implementation notes · low-medium |
| Compact | checked inside every Reduce; compact above 10% inactive | separate event after Elim, interval and 100‰ threshold | checked immediately after every Reduce above 10% retired | No logical formula state is changed; identities are remapped before next event. | deliberate Kissat choice · low |
| Intentionally absent reference actions | Classify; no failed-literal Probe equivalent in this reference version | pre/post local search; default FastElim; duplicate-binary cleanup; internal Decompose edges | no Classify/local-search envelope; no FastElim; explicit Probe/HBR and proof rules instead | These are not silently treated as matches. | Classify is internal heuristic bookkeeping; local-search envelope and FastElim are accepted Kissat tie-break omissions; duplicate cleanup/decompose are partially covered by Tactix rules |

## Findings, deduplicated by root cause

### `ORDER-DEFAULT-1` — enable the implemented Reorder default

`want_reorder` reads `TACTIX_REORDER` and defaults to zero
(`src/policies/default_policy.cpp:142-153`), while the authored CDCL position is
correct (`:993-997`). This is not an order-only cosmetic difference: the
default-enabled Kissat event never occurs. Existing documentation calls the
off setting “reasoned,” but the governing N4 rule says Kissat wins a comparable
default conflict. The old SAT-side regression is evidence to price the change,
not evidence that the shipped reference mechanism is absent.

Classification: **missing default-enabled mechanism**, high.

### `ORDER-LUCKY-1` — restore the late Lucky opportunity

Kissat calls Lucky on both sides of preprocessing (`search.c:184-189`), and
CaDiCaL does the same across `preprocess` and `solve`
(`internal.cpp:794-804,990-996`). Tactix's sole `lucky_tried` latch is consumed
before its conflict-zero probe phase (`default_policy.cpp:902-910,1026-1029`).
Thus simplification can create a formula that the late reference pass would
solve, but Tactix proceeds directly to search.

Classification: **missing conditional wire**, medium.

### `ORDER-FOLLOWUP-1` — represent outcome-conditional pipeline edges

Kissat calls eager Substitute only after a productive Congruence or Sweep and
otherwise calls one final non-eager Substitute (`probe.c:45-67`). CaDiCaL uses
the same producer/consumer shape with Decompose after productive gate
extraction, Ternary, Probe, and Sweep (`probe.cpp:936-952`; startup at
`internal.cpp:767-775`). Tactix only stores admission bits on flat slot rows and
therefore marks both preprocessing Substitute entries unconditionally
(`default_policy.cpp:857-867,1059-1078`). The existing test freezes that list,
not the reference condition.

The root cause is one missing scheduler concept, not a separate gap for every
producer. A later design should distinguish an ordinary scheduled entry from a
consumer edge carrying at least `{producer outcome, eager/final mode}`.

Classification: **missing conditional wire**, medium.

## One-at-a-time implementation sequence

No item below is implemented by this audit. Every accepted item is a separate,
cumulative increment over the preceding accepted pin.

| Rank | Handle / owner | Smallest increment | Required reach and trace | Correctness gate | Cumulative comparison and reopening condition |
|---|---|---|---|---|---|
| 1 | `ORDER-DEFAULT-1` — **DefaultPolicy + tests** | Make both-mode Reorder the shipped default while retaining explicit off/stable-only arms. Do not change `Reorder` itself. | Add `reorder_occasions`, `reorder_applied`, and a focused event-order trace proving Restart wins ties, then Reorder wins over Rephase. | Reorder unit tests, scheduling/config wiring, rule-policy boundary, full proof/model suite and fuzz with actual Reorder reach. | Compare cumulative A/B/C against the audit pin; reopen/off only for N2 failure or a predeclared cumulative performance rejection, recorded as an explicit N4 deviation. |
| 2 | `ORDER-LUCKY-1` — **DefaultPolicy + tests** | Replace the one-shot latch with named early/late opportunities bracketing the initial probe phase; preserve root propagation before each applicable call. | `lucky_early_calls`, `lucky_late_calls`, `lucky_late_sat`; fixed CNF where preprocessing changes the formula and the late pass is observed. | Lucky tests, startup-order test, DRAT/LRAT and SAT-model checks, full suite and fuzz. | Compare cumulatively against rank 1 on A/B/C; reopen if the late call is unreachable on a qualifying fixture or proof/model trajectories fail. |
| 3 | `ORDER-FOLLOWUP-1` — **scheduler + policy operation + tests** | Add outcome-conditioned follow-up edges, first only for preprocessing: C Congruence→Substitute, C Sweep→Substitute, else one final Substitute. Keep rule guards and public instances unchanged. | Per-edge offered/taken counters plus a scheduler trace distinguishing the four producer-outcome combinations and eager versus final Substitute state. | Scheduling, slot-pose, config-wiring, boundary, proof/model suite; fixed fixtures for neither/each/both producers. | Compare cumulatively against rank 2 on A/B/C; reopen if the edge abstraction leaks into proof rules, duplicates Substitute, loses a required final call, or regresses the cumulative gate. |
| 4 | `ORDER-FOLLOWUP-2` — **scheduler + policy operations + tests** | Reuse the accepted edge abstraction for during-search follow-ups where Tactix has a sound counterpart; separately decide placement of Probe/HBR and HyperTernary. | Producer→consumer counters and full slot trace with state deltas; no claim of parity from call count alone. | Targeted proof checks for every consumer edge, N1 instance coverage, full suite/fuzz. | Price after ranks 1–3; reopen if a conditional edge changes public rule availability or a producer's result cannot faithfully represent reference “changed.” |
| 5 | `ORDER-EXTRA-1` — **documentation/tests** | Keep the flat post-BVE Instantiate/BCE/CCE extension unless measurement or correctness evidence rejects it; document that it is not CaDiCaL's first-productive cycle. | Existing per-rule reach plus one trace showing their post-BVE order. | Proof/model suite with each extra rule reached. | Reopen on N2 failure, zero reach, or cumulative regression attributable to the extra tail. |

The benchmark contract is the one in `PROGRESS.md`: identical manifests and
allocation shape, solved count before PAR-2, moved instances named, every
Tactix proof checked, and every scored SAT model independently checked.

## Validation performed for this audit

- Cross-checked both reference pins and every call/default cited above against
  the local trees.
- Ran the existing scheduling and slot-pose fixtures, including the authored
  order, split clocks, material gates, and current preprocessing admission.
- Ran configuration wiring, option coverage, rule/policy boundary, source
  citation, inventory, and documentation-size checks.
- Ran the full existing CTest suite. No solver source or test expectation was
  changed; a trace-only observation therefore cannot be mistaken for a fix.

## Completion criterion

This audit is complete as an inventory, not as an N4 closure. N4 may be marked
closed again only when the three root causes above are implemented or explicitly
accepted with their required evidence. Any future default-enabled reference
action without a row here reopens this audit; any new rule added to the
inprocessing registry must receive both an authored position and an explicit
disposition in this lifecycle matrix.
