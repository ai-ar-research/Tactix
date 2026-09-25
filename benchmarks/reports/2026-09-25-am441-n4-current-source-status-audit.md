# AM441: current-source N4 production-status reconciliation

Date: 2026-09-25. This is a source/test/documentation audit, not a solver
change or an acceptance gate. The September 7–10 isolated experiments remain
frozen historical evidence; their reports must not be read as the status of
today's production tree.

| Ledger row | Current-source evidence | Validation still missing |
|---|---|---|
| `SP-FORWARD-UNIT-CONTINUE-1` | `strengthen_apply` handles root units with `forward_assign_root`, `forward_flush_units`, and `continue`, then checks later candidates; the helper closes the partial stream and preserves sparse replay debt. Blame places this in `7a668e76`. Current Debug job `1723391` passed `forward_unit` (test 137) and `strengthen` (test 39). | General affected fuzz, live reference trajectory, checked cumulative gate. |
| `SP-FORWARD-ROOT-MARK-1` | Canonical C2 uses `subsume_material`, and has no `kernel_touch_sync_fixed` call; C1 and CCE retain that separate helper. Clause additions still call `kernel_mark_subsume_material`. Source comments and cutover are in `7a668e76`. Current job passed `strengthen`. | Affecting fuzz and reference/runtime attribution. |
| `SP-FORWARD-CALLER-CLOSURE-1` | After C2, the Eliminate caller invokes `strengthen_reconnect_and_flush`; it rebuilds the full dense image, rewinds the forward trail cursor, propagates and flushes before BVE. Candidate discovery deletes incoming root-satisfied clauses. Blame places this in `7a668e76`; current job passed `forward_unit`, `strengthen`, and `scheduling`. | The historical isolated composition `5920` regressed full runtime 1.176794× and C 1.017340×; merged-source checked cumulative performance and affected fuzz are not accepted. |
| `SP-ELIM-DENSE-ENTRY-1` | Eliminate entry calls `kernel_flush_large_watches` before occurrence construction; large clause creation/rewrite publishes sparse watches only when `large_watches_connected`; exit restores sparse mode. Entry is in `7a668e76`. Current job passed `scheduling` test 13, which observes physical disconnection and restoration. | Affected fuzz, full-view reference trace, B28+C5 and cumulative runtime. |
| `SP-ELIM-RESOLUTION-LIMIT-1` | After BVE round garbage collection, `e.meter(k) > limit` latches `ctx.eliminate_limit_reached`, and the outer Eliminate loop stops on that latch; equality is allowed. Blame places both in `7a668e76`. Current job passed `eliminate_limit` test 127 (12 phase scenarios). | Affected fuzz, causal real-instance trajectory, checked cumulative gate. |
| `SP-SLOT-REDUCE-EDGE-1` | `note_reduce` records the completed Reduce conflict and `want_phase` declines Probe and Eliminate at exactly that conflict; reset uses the sentinel. Blame places this in `6d235f4a`. Current job passed `scheduling` test 13, including the focused before/same/next/reset case. | Candidate fuzz, actual phase-boundary coverage and full scheduling gate. |

The previous ledger said five of these were “open in production,” and called
the sixth an AM381 candidate without acknowledging it had merged. The living
ledger and `docs/implementation-notes.md` now separate *implemented* from
*accepted*. This audit did not modify rule semantics, scheduling values,
proofs, checkers, binaries, or benchmark pins. Job `1723391` had passed
186/187 tests when inspected; its final `demo3_scripted_default` remained
running. Extended paired AES proof-checker tasks `1723443_0/1` also remained
running. Neither pending job is counted as complete.

N4, the combined N1–N4 goal, H2's canonical no-option N2 proof, and full Tier
D remain open. Do not launch Tier D until the required fixes and gates pass.
