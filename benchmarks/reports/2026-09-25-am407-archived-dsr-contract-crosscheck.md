# AM407: real-solver DSR checker-contract cross-check

Date: 2026-09-25. Checker-only hwulab job `1721174` completed `0:0` in three
seconds using two CPUs and 8 GiB. It compares the frozen AM369 no-option
`dsr-trim` with the same binary under `--delete-implied-units` on four exact
archived solver-produced CNF/proof pairs, eight checks total. Every check runs
the complete backward DSR→LSR→Trestle pipeline and a forward every-lemma
audit after successful backward checking; a `dsr-trim`-only success cannot
count. The [probe](../probes/am407-dsr-archived-contract/README.md)
preserves the exact script, input and checker hashes, per-mode logs and LSRs,
and both input and output archives. No solver, proof serializer, canonical
oracle, or checker installation changed.

| Archived pair | No option | `--delete-implied-units` |
|---|---|---|
| AM296 direct-SR proof (`am296-current` reproducer) | Reject: backward elaboration reaches Trestle `s INVALID`; its forward audit also rejects in the archived owner report | Verify, including forward audit and Trestle |
| AM296 deleted-implied-unit-reason proof (`am296-diu-harness` reproducer) | Verify, including forward audit and Trestle | Reject: no UP contradiction at proof line 13 |
| AM312 Sweep-retention regression (`...s0-005338` reproducer) | Reject | Reject: no UP contradiction at proof line 548 |
| AM312 repaired exact case (`rel-binary.dsr`) | Verify | Verify |

All seven predeclared outcomes matched; the AM312 repaired no-option check
was observation-only and verified. The first two are complementary
real-solver witnesses: the option is **not** a monotone relaxation of proof
checking. It honors deletion of an implied-unit reason, which can make a
previously invalid later SR step valid, or invalidate a later proof step that
relied on that reason remaining. The AM312 rejected proof is a separate
known regression, not evidence against the option's implementation, and
remains rejected under both contracts. Independent Trestle is essential: the
AM296 direct-SR no-option backward elaboration alone produces an LSR, but
Trestle rejects it. These are inferences from the observed outcomes and
the archived diagnoses; the job did not replay every solver state transition.

This extends AM404's hand-built and current-source controls with four older
solver-produced pairs, but still does not prove every option path sound or
settle the canonical contract. If the user approves the option, the calculus,
oracle, negative controls and broad current-source fuzz/gates must be changed
and verified together. If no-option remains required, the general
checker-visible proof-state design remains open. H2 is still rejected under
the current canonical route, post-finalization C-API addition can still
produce a wrong SAT model, and combined N1/N2 are not accepted. The 5,000 s
Tier-D run remains deferred.
