# Organic winner-technique integration

This living subplan owns the portfolio-convergence workstream linked from
[DEVELOPMENT_PLAN.md](../DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration).
Status and next actions belong to PROGRESS.md; controller contracts to
controller-interface.md. This document mixes accepted requirements, implemented
foundations, and future mechanisms; `PROGRESS.md` is authoritative for which
increments are currently wired, tested, or accepted.

## Objective and fixed targets

The ultimate goal is to solve substantially more than the virtual portfolio of **LymphoSAT, Satsuma+Kissat, Kissat-sc2024 and AE-Kissat-MAB combined**.
**Primary technical goal:** recover every LymphoSAT-owned solve through the mechanism appropriate to its evidence. SAT-only techniques may remain bounded constructors/local search: complete candidates require independent original-CNF validation and incomplete candidates are heuristic guidance only. UNSAT capabilities and reusable deductions require source-authenticated theory reasoning followed by DSR/DRAT-certified Boolean consequences or definitions. Keep action identities append-only, serve default/random/future learned policies through shared execution and feedback, and map each owned row to its mechanism and checked outcome; detection or a related-family solve is not recovery.
Performance and learning readiness are joint acceptance criteria: every capability serves DefaultPolicy, random and future learned selection.
Parity is intermediate; each increment respects N1–N4 under plan §1.1 and retains formal T4/T5 targets.
Use the owning API/proof/parity checks and ledger; no blanket N1–N4 pass is claimed.
Use the historical no-MAB `d170b4a5` full Tier-D campaign as the approved performance approximation (`375b621b` at entry).
Do not rerun that baseline, the full comparison or unchanged Tactix to refresh it. Develop current source; historical correctness acceptance does not transfer.

The [saved portfolio report](../report-to-andrew/20261003T164617Z-tierd-portfolio-misses/report.pdf)
and its CSV/manifest identify the target inputs and original result records.
CSV SHA-256: `b2444220c04a4e1a05bbe3d21c4bc73d2e119bad625fb31a9a3ac7d773c0b4f6`.
Freeze the following subset without rerunning its collector: retain rows whose
`checked_solvers` intersects `lymphosat;satsuma-kissat;kissat-sc2024`.
This gives **36 misses: 18 SAT, 18 UNSAT; 23 solver timeouts, nine checker
timeouts and four checker failures**. Their frozen ranks are
82, 147, 190, 226, 263, 291, 323, 343, 367, 401, 453, 479, 489, 497, 511, 540,
541, 569, 627, 630, 654, 744, 754, 762, 781, 887, 896, 899, 941, 991, 1004,
1075, 1129, 1144, 1161 and 1163. These ranks identify evidence, never solver controls.
The full four-arm gap additionally contains ranks 97, 410, 499, 557, 1009,
1040, 1085 and 1118, checked only by AE-Kissat-MAB among the reference arms.
They remain in scope after the 36-instance checkpoint; solving all 44 does not establish superiority beyond the portfolio.

## Integration and investigation order

1. Build a source/evidence attribution table from saved records: reference
   mechanism, Tactix capability, scheduling, cost, certificate route and limits.
   Start with large disparities such as `noL-11-10` and `ramsey_4_4_19`, while
   keeping solver-timeout and certificate-verification workstreams separate.
   A fastest arm or benchmark name is a clue, not a demonstrated cause.
2. Extract complete, general semantic units from LymphoSAT and Satsuma+Kissat,
   and investigate Kissat/AE-Kissat-MAB mechanisms where they own residual gaps.
   Use structural recognition and explicit applicability, never filename/rank
   dispatch, hardcoded answers or external solver stages. Share clauses, root
   assignments, learned information, reconstruction and heuristic state with CDCL.
3. Retain bounded SAT-only constructors as complete/partial candidate producers;
   expose UNSAT reasoning and reusable deductions as DSR/DRAT-certified Boolean actions.
   Both paths use shared typed execution, replay, feedback and policy selection.
4. Establish proof/model feasibility and a bounded mechanism review before
   implementation. Integrate one complete increment at a time, with faithful
   reference controls, targeted causal validation and cumulative preservation.
   Begin with Oddball construction/guidance, then extend the same contract to the
   remaining owners. O6/reward/training remain deferred; selection stays random.

## Lightweight symmetry during inprocessing

Make symmetry discovery/application available at full Restart-to-root boundaries.
Policy owns interval, relevant-change admission, scheduling position and effort;
legality remains independent of those scheduling choices. An interval becoming
due does not interrupt an ordinary partial restart or change configuration there.
Charge snapshot/graph construction, detection, witness validation and application.
Check cheaply for relevant changes after an unsuccessful attempt; no repeated
whole-formula discovery on unchanged input merely because another restart occurred.

Study positions around other passes because propagation/cleanup may expose
symmetry while elimination, substitution, factoring and earlier breaking may
obscure it. Initial hypothesis: root propagation, lightweight cleanup, discovery
and validated application, propagation, then broader inprocessing. This is a
hypothesis to review against the retained startup/reference sequence, not a
universal order. Keep discovery and application adjacent; intervening changes
invalidate or require revalidation of pending candidates. Track relevant change
kinds rather than treating every mutation as equally informative.

Prefer small propagation-useful units/binaries initially. Reusing structure or
generators is distinct from fresh discovery and requires valid identity mapping
and revalidation; old symmetries cannot be assumed valid after transformations.
Validate SR against the relevant live formula **and checker-retained proof
database**, including learned clauses and prior symmetry-breaking effects.
Root level alone is not a soundness argument. Before complete validation, budget
exhaustion/interruption publishes no formula/proof mutation; an admitted atomic
commit completes proof/bookkeeping/root closure and records actual overshoot.

Retain the existing Claude review requirement for a bounded restart-time SR
schedule/evidence report before implementation or promotion. The new workstream
permits this investigation now, rather than postponing its design until the old
57-row gate; it does not waive the standing coverage or preservation gates.
The review compares later SR off/on with identical rules/seeds on B28+C5 and
Rank1–40, retaining all 57 Satsuma-owned rows under the standing gates.
Record discovery cost, effective consequences, downstream search work and proof
cost. Exact positions, interval values, allowances and cache design are decided
by that review, with source-backed or derived defaults rather than guessed constants.

## General constructor guidance and CDCL phases

The shared guidance interface remains available for existing eligible producers;
it does not authorize dedicated LymphoSAT local-search integration. Such producers publish an owned
candidate with stable external variable identities, known polarities, producer,
formula provenance and actual effort. Complete validated models finish SAT;
unsuccessful bounded searches may retain useful candidates. Recognition or an
incomplete assignment alone never certifies SAT or authorizes a formula change.
Extend existing hint producers/consumers rather than introducing a second channel.
Keep producer objectives distinct; compare candidates only with a common,
explicitly budgeted evaluation on the same current formula. Old scores are not
comparable after an objective/formula change. Map surviving identities, respect
root assignments, skip retired variables and leave unspecified/new variables to
the ordinary phase rules; heuristic guidance does not become a logical premise.

The policy chooses two independent uses at a full root boundary:

- **Walk seed:** initialize the walker's private assignment from the candidate;
  constructors and seed import do not overwrite CDCL phase memory. Walk exports
  its result through the existing rephase mechanism. Preserve the ordinary
  Best/Walk/Inverted/Best/Walk/Original cadence and its reference-owned clocks.
- **CDCL target initialization:** install known polarities as target phases,
  initially preserving focused-mode polarity behavior. Leave saved/best phases
  intact at installation and let normal CDCL target capture supersede advice;
  do not repeatedly restore the candidate at each decision or restart.

Construction remains effort-bounded and outside the decision hot path. Guidance
changes polarity, not VMTF/EVSIDS variable ranking or root/trail assignments.
The policy owns source selection and application; producers never silently
install guidance. Type/serialize/replay consumer choices, candidate identity and
parameter/effort profiles before changing the current controller interface.
Preserve unselected search configurations; target advice is consumed only by a
configuration that uses targets. No fallback, solver rollback or global root-loop
budget is introduced. Direct guidance and Walk seeding need separate attribution.

## SAT/UNSAT reasoning and proof production

Review existing and missing LymphoSAT techniques. SAT-only construction may
finish a validated model or publish partial guidance. Theory-to-Boolean transfer
applies to UNSAT and reusable deductions: distinguish implied consequences,
conservative definitions with projection, and proved satisfiability restrictions.
Chosen assignments are not implied facts.

Explain whether the advantage comes from deductions, representation, coordinated
choices or a combination. Consider selective/lazy consequences; Boolean encoding
alone does not establish efficient search. Assess generation effort, formula growth, propagation,
memory, proof/checking cost and checked SAT/UNSAT outcomes. Share structural
recognition between certified reasoning and constructive guidance where useful.
Share recognition between construction and proof paths where useful; a SAT-only
owner does not need a redundant Boolean encoding to remain integrated.

Extract structural information from LymphoSAT SAT constructors and UNSAT
reasoning alike. Candidates include equivalence-chain, XOR/parity, Tseitin
and counting mechanisms identified by the existing
[PB surface audit](../benchmarks/reports/2026-09-25-am402-winner-pb-rule-surface-audit.md);
their relevance to a particular miss must be established, not inferred from names.
For SAT, validate complete reconstructed models against the original CNF;
guidance changes heuristics only. Permanent transformations owe certificates and
reconstruction. Record the theory result before its Boolean representation,
including auxiliary projection; new variables or score bumps do not replace reasoning.

For UNSAT, the theory module owns reasoning and returns authenticated premises,
derived equations/conflicts, elimination steps or coefficients, and useful compact
consequences. The SAT proof adapter validates that object, emits DSR by default
(DRAT where witness-free), and atomically installs only the derived Boolean results;
it does not redo the theory search. GF(3) therefore reduces/eliminates first and
introduces certified auxiliaries only to compress useful results, never to eagerly
encode the whole system. Proof/check cost remains part of feasibility, and a
checker failure/timeout alone does not establish its cause.

**Do not develop a new pseudo-Boolean proof path while a compact clausal
certificate remains plausible.** DSR is the required default and witness-free
steps may use DRAT. Preserve the existing complete-startup PBP route and the
architectural option for a particular future rule, but reconsider it only after
a mechanism-specific derivation and measured clausal proof/check cost establish
that practical DSR/DRAT certification has failed.
The isolated first clausal verticals are: a normalized direct-PHP proof core for
binary pigeonhole then clique-coloring; a pure bounded sparse GF(3) Gaussian
planner with replayable source provenance before any live publication; and a
pure-Tseitin cycle-space DSR construction after exact graph/source admission.
Each begins with a proof planner and exhaustive microchecks, then one saved owner
obligation. Integration into live actions follows only after independent DSR/DRAT
checking and proof-volume preflight; replay and interruption remain mandatory.

Record every added constraint/auxiliary definition: stable identity, Boolean formula
and theory meaning, source clauses/structural premises, generating action and
parameters, variable mapping/model projection, and linked proof event or range.
Track later deletion/replacement and per-action counts, effort and proof volume.
Use a streamed derivation record linked to replay; do not retain unbounded proof
history in memory. TTF exposure specifically requires a bounded live-population
index over canonical clauses, current/raw proof identities, replacement/deletion
state and mutation generation, or source pinning through commit. A certificate
alone does not replace semantic provenance.

Investigate policy-controlled branching on introduced or recovered semantic variables,
distinguishing useful search choices from proof-only helpers. Assess EVSIDS boosts,
VMTF promotion or explicit Decide selection through shared public/replayable controls;
producers publish identities/meaning, never silently change rankings. Compare constraints
alone against added guidance; measure actual decisions, propagation and solve/proof cost.
No boost strength, repetition schedule or default is chosen without evidence. The versioned semantic-action, session, lifecycle, advice, and raw-effort contract is owned by [the controller interface](controller-interface.md#semantic-proof-actions); current implementation and integration order are owned by `PROGRESS.md`.

## Learning-ready acceptance

New reasoning, construction and guidance must use the shared typed execution and
eligible-action interfaces, with DefaultPolicy an explicit scheduling instantiation.
Do not hide useful capabilities, ordering or budgets inside a default-only solver
path. Keep legal guards and proof/reconstruction in rules; policy owns admission,
intervals, order, candidate selection and effort. Preserve repeated eligible
actions at full root and ordinary partial-trail continuation during search.
Version/serialize new actions, instances, parameters, candidate references and
continuation/stale/interruption behavior; replay selection and results deterministically.
Extend pure observation and after-action feedback with relevant raw state,
actual effort/currency, yield, status/reason, overshoot and linked search intervals.
Selection sees current measurements without invented retrospective credit or a fixed reward. Measure instrumentation and solver/proof costs.
Demonstrate effective default/random reach for every capability; performance-only private paths are incomplete integration.

## Verification, completion and remaining decisions

The primary target gate requires all 44 independently checked on a candidate; the original 36 remains a diagnostic subgroup rather than an earlier completion gate.
Use the saved 5,000-second solve/32-GiB envelope and the historical complete
14,400-second checker pipeline for comparable target acceptance. Raw UNSAT,
checker crashes and clean deadline censors do not close a target. Investigate
the 13 certificate-side misses separately, retaining their frozen classifications.
Classify WV100/WV2–89 row by row: LymphoSAT-owned rows are checked recovery obligations, while other rows are preservation controls under the standing slowdown/checker rules. Map each mechanism across them and the 44; aggregate similarity cannot hide a lost owned solve. Broader T4 requirements remain distinct.

Every integration increment and cumulative candidate requires continued
Claude-owned DefaultPolicy/random-policy proof/model fuzz under plan §8.2,
with effective action reach, negative controls and failures resolved before promotion.
Require API/eligibility, identity/compaction, repeated actions, interruption,
allowance/overshoot, stale candidate, phase-memory and replay checks, and full Debug at acceptance.
For symmetry, include learned/checker-retained clauses and earlier breaking;
for guidance, test private seeding, saved/best preservation, target supersession,
fixed/retired/new variables, and unsupported phase profiles without hidden override.
Keep B28+C5 and Rank1–50 as mandatory regression gates; apply the row-level recovery/preservation split above to WV100/WV2–89. The 44 plus LymphoSAT-owned WV/WV2 rows drive mechanism selection, but no promotion gate is optional;
rerun all four for behavior-changing integrations, including Boolean constraints,
auxiliary variables and scheduling changes. Acceptance covers the intended
enabled DefaultPolicy configuration; passing with a new mechanism disabled
establishes compatibility only, not its performance acceptance.
Retain exact ceilings, memory/coverage exceptions and bounded checking from plan §8.4:
1,800 seconds for the whole pipeline and at most `max(30% of baseline,20s)`
extra completed check time. This regression exception never verifies a target.

Future targeted candidate experiments, fuzz and preservation gates follow their skills and the user's experiment authorization; this plan launches none. Do not recreate the baseline campaign. Measure actual work/cost and individual regressions; saved asynchronous timings are not causal speedups.
Beyond the fixed gaps, evaluate generalization and additional checked solves on union/held-out inputs against the frozen four-arm portfolio, preserving every reference-covered result as the parity floor. Define the broader evaluation protocol and a numerical meaning of "substantially more" before claiming the ultimate objective; do not invent that threshold now. Mechanism and schedule choices follow the attribution review.
