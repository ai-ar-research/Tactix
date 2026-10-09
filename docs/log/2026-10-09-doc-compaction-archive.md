# 2026-10-09 documentation compaction archive

Frozen verbatim excerpts removed from living documents by the 2026-10-09
`compact-docs` pass (source text as of commit `555ecb4b0`). Each excerpt is a
record of what the owning document said then; it is never edited. Current
dispositions stay in the owning living document, which links here.

## `docs/scheduling-parity-ledger.md` §13.2 — SP-JUMP-1 closure record (SCHED-C8)

The living row keeps the current disposition and a summary of this record.

  **▶ CLOSURE RECORD (SCHED-C8).** Three of this row's own premises were wrong, and they are the substance.
  **(a) "both references"** — only kissat has it. cadical's `search_assign` (`propagate.cpp:102-136`) stores the reason clause verbatim, and it is the same function that builds `lrat_chain`; the option set has no `jumpreasons`. So the reference that also emits LRAT does not do this, which is the shape of (c) below.
  **(b) "effectively UNCONDITIONAL", "true for virtually every formula"** — overstated. The per-mille correction (§13.5 C3) is right and the 99 % reading stays dead, but the binary fraction is the *second* test: `classify.c:9` sets `bigbig = false` outright whenever `clauses <= smallclauses` (**1e5**, `options.h:131`). The propagation site's gate is therefore a **formula-SIZE** gate.
  This gate is now ported exactly. It was not inert on the P4B fixture `mp1-klieber2017s-0500-023-t12`: its roughly 30k clauses keep Kissat's propagation path direct, while the former unconditional Tactix jump changed conflict 1's learned clause. The learned-binary path remains ungated, matching `assign.c`.
  `9vliw_bp_mc` is **179,492 clauses, 88.8 % binary**, so the gate is still true there; its older regression remains valid evidence about the enabled mechanism, not about whether the classifier wire is optional.
  **(c) "zero cost"** — true for kissat, **false for us, because of N2.** A virtual resolvent is a reason
  naming a clause the checker does not hold, and every LRAT consumer of a binary reason has to expand it back
  into its two real antecedents: `chain_record` (`analyze.cpp`), `kernel_reason_walk` (`kernel.cpp`, so
  Vivify/Substitute chains), and the **delete-binary live-reason guard** (`kernel_delete_binary` ×2 — the pair
  `(lit ∨ jumped_over)` is cited by the chain but is no longer the recorded reason, so without the extension
  TransitiveReduce/Substitute/Factor could delete a clause a live chain names; pinned by
  `tests/test_binary_deletion.cpp::test_jumped_reason_protects_its_antecedent`; the default-on wiring now
  makes ordinary fuzz and tier legs exercise that branch too). `emit_root_units` needs nothing — the jump is taken only above level
  0, and there is an assert saying so.
  **The expansion cannot be done at record time, and finding that out cost a fuzz-RED round.** The obvious
  form — chain the resolved-away literal's reason recursively, right there in `chain_record` — is **wrong**:
  whether that literal needs an antecedent at all depends on the FINAL clause, and if it ends up in the clause
  the checker already has it assigned, so citing its reason is a **satisfied hint** and `lrat-check` stops
  propagating. That is the same trap the shrink UIP is already guarded against (`analyze.cpp`, "its reason
  would be a satisfied — illegal — hint"), and reason jumping reaches it from a second direction. The shipped
  form defers to `chain_expand_jumped`, run from `chain_emit` against the final clause; it iterates, so
  kissat's jump CHAINS still work. 12 of 1500 native-LRAT fuzz cases rejected before this, 0 after.
  That is the honest price — a `[wire]` in kissat's architecture, N2-critical surgery in ours, and it is why
  porting it default-ON on the strength of "free" would have been wrong.
  **The prior measurement, and why it is not a verdict.** This mechanism is not new here: it was implemented,
  measured and **REVERTED in phase 1** (`e26492b`, 2026-07-11, "binary-reason jumping (kissat jumpreasons)") —
  `summle` 66 → 48 s (**−27 %**) but `9vliw` 11.3 → 22.6 s (**+2×**). Its recorded escape hatch, "needs the
  bigbig-style classifier to gate it, as kissat does", does not explain `9vliw` because the gate is ON there,
  but it was nevertheless a real missing wire and materially explains `mp1`. What survives from the old
  measurement is a single-draw laptop-era
  reading on a phase-1 kernel with no chrono, no OTFS, no reason-side bumping, no dynamic tiers and no
  inprocessing — every one of which changes the trajectory this mechanism
  perturbs. That was the pre-port reason to stage the arm OFF, not the current
  ship state. **Current ship state:** default ON, following Kissat;
  `TACTIX_JUMP_REASONS=0` is the direct-antecedent rollback. Reopen on a
  proof/model failure or cumulative evidence strong enough to justify an
  accepted deviation.
