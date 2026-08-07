---
name: calculus
description: Maintain docs/calculus.md (the abstract proof-rule calculus — each rule's pure precondition, effect, proof delta, soundness) and its companion docs/implementation-notes.md (the implemented guards + engineering-deviation ledger + design decisions). Use whenever a rule is added/removed/changed, whenever a guard gains or loses a conjunct, or when deciding whether a guard conjunct is a genuine precondition vs an engineering deviation. Keeps both in sync with DEVELOPMENT_PLAN.md §4/§5.2 and PROGRESS.md.
---

# Maintaining the proof-rule calculus

`docs/calculus.md` is authoritative for **what each proof rule *is*** — its
abstract precondition, effect on `σ`, proof delta, and soundness. Its companion
`docs/implementation-notes.md` holds the **engineering layer**: the implemented
guards (per-rule table) and the **engineering-deviation ledger** — every guard
conjunct *stronger* than the abstract precondition, why, and its removal path —
plus standalone design decisions. Together they keep N1 honest: a guard must be a
**pure soundness precondition**, and any divergence must be *recorded*, not hidden.

`DEVELOPMENT_PLAN.md` §4 owns the formal-model source of truth (`σ`, the
precondition paragraphs); `PROGRESS.md` owns status; `docs/calculus.md` is the
per-rule expansion; `docs/implementation-notes.md` is the consolidated
guard/deviation ledger. All must agree.

## When to use

- **Any rule change** — new rule, removed rule, or changed semantics / guard /
  proof-delta / instance surface. Update the rule's entry (or add one) in the
  same commit as the code, exactly as `rule-dev` requires the code change and
  `revise-plan` requires the §5.2 catalog line.
- **Any guard edit** — a conjunct added or removed. Re-run the classification
  (below) and update the `implementation-notes.md` guard table + ledger.
- **A soundness question** — "is this guard conjunct a real precondition or an
  engineering artifact?" This is the skill's core judgement (see the test).
- **Closing a deviation** — when a kernel change (e.g. true-level assignment)
  lets a guard drop a conjunct, remove that ledger row and tighten the affected
  rule entries, in the same commit.

## The abstract-vs-engineering test (the core judgement)

For each guard conjunct, ask: *would this rule be **unsound** without it, as a
rule in the abstract calculus over `σ = ⟨F,R,τ,ρ,π⟩` (`H` is outside `σ`, rev 59)?*

- **Yes → abstract precondition.** It constrains `F`/`τ`/`ρ` in a way the
  inference genuinely needs (e.g. `Analyze` needs a recorded conflict;
  `!inconsistent` is terminality; `Probe` assumes `p` atop `M` so needs `M`
  consistent).
- **No → engineering deviation.** It is imposed by an implementation choice, not
  by the abstract rule. Record it in `implementation-notes.md` with a **root
  cause** and a **removal path**. The ledger's §2 "Open" table is the live list;
  read it before inventing a tag. As of rev 83 the only remaining *guard*
  deviations are:
  - **D-ROOT** — `level == 0` on every inprocessing rule, because the rules are
    coded against the root formula `U`. An accepted *framework* choice (plan §4
    defines inprocessing on `F|U`), not currently planned for removal.
  - **D-NOOP** — S1/S2's `¬conflict_valid` / `level>0` no-op (GC forwards
    `pending.ref` but not `conflict_ref`). Benign, low priority.
  Everything else open is an **effect branch** (D-CONG-SKIP, D-CONG-TAUT) or a
  **dosing/scope** row (D-STRENGTHEN-BUDGET, D-SUBST-UNBOUNDED, D-CCE-*,
  D-C1C2-VICTIM), not a guard conjunct. **D-LEVEL and D-SPLIT are CLOSED** —
  inprocessing rules now drain the root closure at apply entry
  (`inprocess_reach_root`) instead of demanding a fixpoint in the guard; if you
  find a skill or comment citing them as live, it is stale.

**Within-rule SCOPE is not a guard conjunct, and the distinction is load-bearing
(§4).** A filter that only narrows *which candidates the rule offers* — a tick
cap, an irredundant-only candidate set, a size floor — is scope; it can never make
an inference unjustified, and it does not go in the guard. But per N1 it must not
silently shrink the **instance surface** either: if it encodes a WHEN/WHICH
decision, it belongs to the policy (rule-dev skill).

Two traps: (1) don't restate the coded guard as the "abstract precondition" —
strip the engineering conjuncts first. (2) a control flag not in `σ`
(`conflict_valid`, `pending`) mentioned in a guard is almost always an
engineering deviation, not an abstract precondition — the exception is a rule
that genuinely *consumes* that flag (`Analyze` consumes `conflict_valid`).

## Procedure

1. **Read** the rule's guard + apply + proof calls, and its current calculus
   entry. Confirm the implemented guard is quoted verbatim (with file:line).
2. **Classify** each guard conjunct with the test above. Prefer verifying
   runtime-behaviour claims with a throwaway probe over reasoning alone (the
   extraction that seeded this doc compiled probes against the debug objects).
3. **Write** the abstract entry in `calculus.md`: Precondition (abstract) /
   Effect / Proof δ / Soundness only — no guard conjuncts or deviations in the
   rule body. Keep entries tight and in the existing house style.
4. **Update `implementation-notes.md`** — the per-rule implemented-guard row
   (conjuncts tagged `[abstract]` or `[eng:<ledger-id>]`, e.g. `D-ROOT`) and, if
   any deviation was added/removed/moved, the ledger.
5. **Cross-check** `DEVELOPMENT_PLAN.md` §4 (precondition paragraphs) and §5.2
   (catalog phase + proof-delta type) and `PROGRESS.md`. If the abstract rule
   itself changed (not just the implementation), that is a plan edit → go
   through `revise-plan` too.
6. **Run `ctest`** before committing: parts of `implementation-notes.md` are
   machine-checked as tests (`check-option-coverage`, `check-effort-specs`,
   `check-config-wiring`, `check-stat-ids`), so a docs-only edit — especially one
   that drops or renames a ledger/§13 row — can turn the suite red.
7. **One commit** with the code, the calculus entry, and the plan/PROGRESS sync.
   Reference the affected rule id(s) and any ledger row touched.

## Invariants this skill protects

- Every implemented guard is either `==` its abstract precondition, or every
  extra conjunct appears in the `implementation-notes.md` ledger with a root cause
  + removal path. No silent strengthening.
- Every rule in the `TACTIX_REGISTER_RULE` registry has a calculus entry.
- The proof-delta named in an entry matches the actual `proof->...` calls
  (`add_rup` / `add_derived_unit` / `add_empty` / `del` / `push_witness` /
  none) — including the LRAT-vs-DRAT asymmetries (e.g. a failed-literal unit is
  `add_rup`, not `add_derived_unit`, so DRAT sees it).
- The calculus never weakens N1/N2/N3/N4: it *documents* deviations so they can be
  removed, and a removal only lands with the fuzz + corpus evidence that the
  relaxed guard stays sound.
