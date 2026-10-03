# Organic winner-technique integration

This living subplan owns the portfolio-convergence workstream linked from
[DEVELOPMENT_PLAN.md](../DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration).
Current implementation status and the next action belong to PROGRESS.md;
concrete versioned controller contracts remain in controller-interface.md.
The mechanisms below are planned changes, not claims of implementation.

## Objective and fixed targets

The ultimate goal is to solve substantially more instances than the virtual
portfolio of **LymphoSAT, Satsuma+Kissat, Kissat-sc2024 and AE-Kissat-MAB combined**.
Integrate their useful techniques into Tactix's continuing solver state, then
improve the algorithms and their composition beyond the references where useful.
Performance convergence and learning readiness are joint acceptance criteria:
each capability must be usable by DefaultPolicy, random and future learned selection.
Parity is an intermediate milestone, not the final success criterion. Each
increment must respect N1–N4 under plan §1.1; retain the formal T4/T5 targets.
Use the owning API/proof/parity checks and ledger; no blanket N1–N4 pass is claimed.

Use the historical no-MAB `d170b4a5` full Tier-D campaign as the user-approved
performance approximation for the current implementation (`375b621b` at entry).
Do not rerun that baseline, the full comparison, or unchanged Tactix merely to
refresh this approximation. Develop against current source; the approximation
does not transfer correctness acceptance to changed code.

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
They remain in scope after the 36-instance checkpoint; solving all 44 still
does not establish superiority beyond the virtual portfolio.

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
3. Add or extend native passes, certified proof rules, model constructors and
   local-search procedures. Prefer useful intermediate consequences and
   guidance as well as complete solutions; expose eligible actions to both
   DefaultPolicy and the random controller through shared typed execution.
4. Establish proof/model feasibility and a bounded mechanism review before
   implementation. Integrate one complete increment at a time, with faithful
   reference controls, targeted causal validation and cumulative preservation.
   Build learning-ready capabilities from the start; performance alone does not
   accept an increment. O6/reward/training remain deferred; selection stays random.

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

Eligible structural searches/model constructors publish a separately owned
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

Investigate LymphoSAT's constructive/local-search SAT techniques and its UNSAT
reasoning together. Candidates include equivalence-chain, XOR/parity, Tseitin
and counting mechanisms identified by the existing
[PB surface audit](../benchmarks/reports/2026-09-25-am402-winner-pb-rule-surface-audit.md);
their relevance to a particular miss must be established, not inferred from names.
For SAT, validate complete reconstructed models against the original CNF;
guidance-only work changes heuristics, while permanent transformations still
owe certificates and reconstruction. Preserve useful partial structural work.

For UNSAT, develop the algorithm and its compact certified derivation together.
**DSR remains the default**, with DRAT where a witness-free clausal derivation
permits it; direct SR need not expand to DRAT. Express reusable deductions through
RUP/RAT/SR, including certified auxiliary definitions where needed, and introduce
a new public rule only for a missing semantic capability. Proof-size, generation/
check cost and memory are part of feasibility. Checker failures/timeouts motivate
more compact or checker-friendly proofs, but do not by themselves prove that
proof size is the cause; retain the observed failure classification.

**Keep pseudo-Boolean proof (PBP) support as an option for particular rules**
when necessary reasoning or impractical clausal certification justifies it.
Do not reject a valuable technique solely because efficient DSR encoding is not
available. Establish the need per mechanism from its derivation and cost, with
DSR preferred when practical; routine proof generation must not all become PB.
The existing complete-startup PBP route is available within its declared boundary.
A new live PB rule needs a reviewed typed derivation interface, proof-format
requirements and a complete independently checked original-CNF certificate chain,
including preceding/following clausal work and transformations. PB commands cannot
be spliced into DSR or cause an undeclared format switch after publication.
Choose a coherent PB-capable whole-run certificate or a formally checked bridge
before implementing mixed reasoning; exact architecture remains a design decision.
Preserve replay, interruption, public applicability and model reconstruction;
an unsupported proof mode must decline before mutation, never execute unverified.

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
Selection sees current measurements without invented retrospective credit or a
fixed reward formula. Measure instrumentation cost as well as solver/proof cost.
Demonstrate each new capability through both default and random selection with
effective reach; performance-only private paths are incomplete integration.

## Verification, completion and remaining decisions

The 36-instance checkpoint requires all 36 independently checked on a candidate,
under the saved 5,000-second solve/32-GiB envelope; use the historical complete
14,400-second checker pipeline for comparable target acceptance. Raw UNSAT,
checker crashes and clean deadline censors do not close a target. Investigate
the 13 certificate-side misses separately, retaining their frozen classifications.
The next checkpoint includes all eight MAB-only misses, while retaining prior
checked coverage. Existing broader WV/T4 requirements remain distinct.

Every integration increment and cumulative candidate requires continued
Claude-owned DefaultPolicy/random-policy proof/model fuzz under plan §8.2,
with effective action reach, negative controls and failures resolved before promotion.
Require API/eligibility, identity/compaction, repeated actions, interruption,
allowance/overshoot, stale candidate, phase-memory and replay checks, and full Debug at acceptance.
For symmetry, include learned/checker-retained clauses and earlier breaking;
for guidance, test private seeding, saved/best preservation, target supersession,
fixed/retired/new variables, and unsupported phase profiles without hidden override.
Keep B28+C5, Rank1–50, WV100 and WV2–89 as regression gates for accepted increments;
retain exact ceilings, memory/coverage exceptions and bounded checking from plan §8.4:
1,800 seconds for the whole pipeline and at most `max(30% of baseline,20s)`
extra completed check time. This regression exception never verifies a target.

Future targeted candidate experiments, fuzz and preservation gates must follow
their skills and the user's experiment authorization; recording this plan launches
none. Do not recreate the baseline campaign. Measure actual work/cost and
individual regressions; saved asynchronous timings are not causal speedups.
Beyond the fixed gaps, evaluate generalization and additional checked solves on
the union/held-out inputs against the frozen four-arm portfolio, preserving every
reference-covered result as the intended parity floor. Define the broader
evaluation protocol and a numerical meaning of "substantially more" before
claiming the ultimate objective; do not invent that threshold now. The choice
of first mechanism and concrete schedules follows the initial attribution review.
