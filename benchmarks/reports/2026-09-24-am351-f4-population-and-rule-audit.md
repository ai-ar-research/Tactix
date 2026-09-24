# AM351: discarded-only population repair and winner-rule/N1–N2 audit

## Status

This is an **unaccepted correctness checkpoint** on source commit
`9eebaa42a7b8309e989fcf27fe6d6b3a3df01103`, not N1–N4 or benchmark-gate
closure. The frozen source/archive, Debug/Release CLI hashes, original F4 input,
checker hashes, scripts and exact outputs are in
`benchmarks/runs/20260924-am351-f4-pop/`. No Tier-D run was submitted.

## General F4 repair

The AM346 F4 handoff's variable 3 appears only in raw tautology `7 3 -7`; one
more declared identity is header-only. It was counted by `unassigned_active`
but absent from `compact_import_order`, so Debug random-policy seed 346018908
asserted at `heuristics.cpp:1278` (old control job **1717234**). Input now
records membership in *surviving normalized* clauses independently of tumble
ordering. `finish_input` retires every unassigned identity absent from that
population, covering taut-only, root-satisfied-only and header-only variables
by one invariant; explicit N1 Decide still enumerates every unassigned
identity. The C API now finalizes input only once: repeated status/action calls
cannot retire a live variable after the parse-scoped membership bitmap is
released. This is a population repair, not a seed-, filename- or shape-specific
exception.

Debug and Release `parse`/`api_n1` tests pass, including queue order, root
unit/satisfied clauses, both tumble settings, target-phase calculation, public
Decide candidates, repeated API calls and a reconstructed SAT model. Full Debug
CTest with pinned DSR/DRAT checkers is in progress; its `sweep_proofs` and
`micro_proofs` tests have passed, but an all-pass result must not be inferred.
Opus owns the AM351 full standard fuzz matrix
in `...-am351-f4-pop-claude-fuzz/`; its status/report alone determines that
gate. The earlier AM350 owner is also active. The AM350 manifest transcribed
the `tests/test_parse.cpp` hash as 54 rather than 64 hexadecimal characters;
its owner verified the actual file against the committed tree. The frozen
manifest is not rewritten to conceal that provenance error.

## Independent exact checks

- Full-width four-task hwulab array **1717244** ran F4 random-policy seed
  346018908 on both immutable Debug and Release CLI pins in native DSR and
  LRAT. All four returned UNSAT/20 and completed 0. Pinned dsr-trim,
  dsr-compress and Trestle verified both DSR proofs **including every lemma**;
  pinned CakeLPR verified both LRAT proofs against the original 9-variable,
  53-clause CNF. Debug/Release DSR proof bytes match (SHA-256 `54c329c8…6284a2`),
  as do LRAT bytes (`cc0ae4cb…342011`).
- Array **1717251** independently replayed the AM349 owner's bounded H1 and
  H4 canonical-Congruence handoffs through a newly built public C API driver.
  Early and legal late native-LRAT attachment each refuted and passed pinned
  CakeLPR against the respective original CNF (4/4 tasks, exit 0). This closes
  those *canonical default-setting* reproductions on AM351; H1's alternate
  limits/watch setting, H4's semantic/idx142 variant, and any new owner cases
  are not claimed tested by this array.
- Four-task hwulab array **1717262** exercised a satisfiable CNF containing
  taut-only, root-satisfied-only and header-only identities. Debug/Release,
  default/random-policy all returned SAT/10; the first winner campaign's
  immutable `checkmodel` independently validated all five original clauses
  and seven output variables in each run (4/4 tasks, exit 0).
- AM350 job **1717233** previously passed its full pinned Sweep proof script
  (47 derivations, five DSR cases). The live mutable dsr-trim checkout rejects
  unchanged corpus033 due to two missing forward hints; old/new solver proof
  SHA is identical. Neither that checker defect nor the AM350 CTest failure is
  mislabeled as an AM351 solver regression. AM351 full CTest uses the frozen
  checker pins.

## Winner proof-rule inventory

The source-level comparison separates a *proof rule* from a solver schedule,
certificate format, or problem-specific complete algorithm. The maintained
catalog has **30/30 dispositions** (`tools/docs/check-inventory.py`, zero
errors); this is an inventory check, not universal soundness evidence. The
frozen first-run manifest names exactly five comparison executables:
Kissat 4.0.4, kissat-sc2024, AE-Kissat-MAB, Satsuma-Kissat and LymphoSAT
(`benchmarks/baselines/sc24-26-32g-5000s/manifest.json`, `solvers`).

| Lineage inspected | Proof-rule finding | Tactix status |
|---|---|---|
| SC2024 Kissat and AE-Kissat-MAB | Their search/preprocess loops invoke CDCL, Probe, elimination and related inprocessing; SC2024's fast variable elimination is still E1, not a new calculus rule (`winners/kissat-sc2024/src/{search,preprocess}.c`, AE `src/search.c`). | M/C/V/E families are cataloged and implemented; schedule/efficiency parity is an N4/N3 question, not closed here. |
| Satsuma-iter-Kissat | The wrapper runs Satsuma `fix --bsr --proof-file`, then Kissat on the reduced CNF (`winners/satsuma-iter-kissat-sc2026/.../src/run_base.sh:77–91`). | Direct substitution redundancy is X7, registered at `src/rules/substitution_redundancy/substitution_redundancy.cpp:773`; graph-symmetry production and full X7 acceptance remain open. |
| LymphoSAT | Its verified wrapper selects specialized SAT/UNSAT attempts and checks UNSAT certificates with VeriPB (`winners/lymphosat-sc2026/.../src/solver/core/main.cpp:496–553`). This is a family of structural algorithms and a certificate route, not one missing local proof rule. | Relevant shared Lucky strategies and PBP route exist, but not every specialist or performance signature is accepted; those remain N3/N4 coverage issues. |
| CaDiCaL reference (not a retained winner) | `condition.cpp` implements globally blocked clause elimination; `options.hpp:58` defaults `condition=0`. | X5 Condition is deliberately absent (`docs/calculus.md:596`, implementation notes §16): its conditional-autarky witness requires cube reconstruction beyond today's one-literal witness stack. Reopen if a default-on owner or cube witness surface justifies it. |

No *additional enabled proof-rule family* in the inspected retained winner
paths is established by this audit. This does **not** claim algorithmic parity:
X7 remains unaccepted, LymphoSAT-specific strategies are not exhausted, and
generic definitional extension plus the P1 compound-instance question remain
planned (§9.1). A proof format such as PBP or DSR is not by itself a missing
solver rule.

## N1 and N2 disposition

N1 is **not yet verified**. The AM351 regression proves the newly retired
identities remain reachable through explicit Decide; CTest has already passed
`api_instances`, `n1_public_policy`, `proof_rule_sync` and targeted API tests.
The registry reachability fixture applies **28/28** entries through the
chosen-instance API, 21 with a selected payload and 12 making progress.
It cannot prove every legal instance in every reachable state. Full CTest,
random-policy reachability/fuzz and the outstanding P1 compound-surface
decision remain required.

N2 is **not yet verified**. F4's four certificates and H1/H4's four C API
certificates independently verify, as do four reconstructed SAT models.
AM349's bounded H2 early-LRAT hint passes
CakeLPR but fails an internal stricter hint replay; H3 DRAT addition passes
drat-trim under its root-persistent semantics but fails a strict live-RUP
audit. These are separately classified audit-contract questions, not silently
promoted as all-path proof coverage. AM350's focused fuzz, AM351's full matrix,
all explicit proof formats, sanitizer/model checks, and complete performance
gates are still pending. A general soundness conclusion requires those
terminal results and diagnosis of any bounded failures.

The post-goal full Tier D submission remains **deferred**. When N1–N4 and the
ordered gates really close, use the first winner campaign's
`cpu-dense-hwulab-q`, 4 CPUs, 32 GiB (`8G` per CPU), 5,000-second solver and
45,000-second checker allowance, with no array throttle or node constraint.
