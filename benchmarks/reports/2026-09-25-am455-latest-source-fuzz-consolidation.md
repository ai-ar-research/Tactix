# AM455: latest-source fuzz consolidation and Sweep LRAT target repair

Date: 2026-09-25 local. This is an unaccepted correctness checkpoint. It does
not close N1--N4 or authorize Tier D. The canonical native DSR route remains
no-option `dsr-trim` → LSR → pinned Trestle; no public X7 action, checker
option, or instance-specific policy changed.

## Why the older queue was retired

At the user's request, final fuzz acceptance moves to one fresh source pin.
AM427, AM444, and AM452 were separate earlier snapshots; a result from any of
them could diagnose a defect but could not certify the current combined code.
Twenty-six still-pending old-pin fuzz tasks were canceled first, then four
AM444 resubmissions were canceled. Old-source full Debug `1723644` was also
canceled while pending. Already-running AM427 tasks were left to finish as
diagnostics. The AM444, AM452 and AM454 Opus owners were stopped; the AM454
uncounted smoke `1723916` finished but has no counted acceptance report.
Their immutable pins and bounded handoffs remain preserved. The new combined
Opus owner and full current-source Debug are next; no old-pin result is promoted.

AM452 full Debug `1723753` reached 185/186 passes then timed out at two hours
with only `demo3_scripted_default` unfinished. Its exact H2 Release and ASan
public replay already verified both no-option dsr-trim directions and both
original-CNF Trestle checks, with byte-identical DSR SHA-256
`4075e6c59eede852dd487f0ee7ce23a97e1622e970e7759b99a7f67568288579`.
The later AM454 proof-writer boundary refinement separately passed Release
and ASan serializer tests (`1723834`), the same H2 checker route (`1723837`),
and focused Release 7/7 plus ASan 4/4 (`1723845`). Its Opus campaign was
stopped before counted legs because this AM455 source supersedes it.

## AM444 C1 handoff and general repair

The AM444 Opus owner retained a bounded `C1-sweep-retarget-lrat` handoff:
Sweep can prove a binary direction, then an earlier root consequence fixes
one endpoint and the other is emitted as a unit. The LRAT chains were
normalized under the original binary's two-target cube, but temporary
lemmas were later conditioned only on the unit. A step omitted as tautological
under the dropped endpoint could be published with an empty chain (Debug
assertion) or an invalid chain (CakeLPR rejection). The candidate-only class
is in the AM444 owner's `OWNER_STATUS.md`; its complete API UNSAT handoff is
still a required independent replay on this source.

`src/rules/sweep/sweep.cpp` now retains that already root-false endpoint in
temporary lemmas when the captured binary is replayed as a unit. It applies
the same conditioning to forward emission and reverse deletion; the final
kernel clause remains the unit. Thus the emitted temporary clauses match the
captured LRAT target cube without changing the unit consequence or public
Sweep instance. Separately, `src/proof/proof.cpp` moves DSR-unit transaction
flushes after rejected SR/external-RUP validation; a declined addition cannot
prematurely publish the pending unit. The serializer regression covers both
rejected paths and accepted SR closure.

## Independent focused evidence

The AM455 diagnostic binaries are read-only under
`build/pin-am455-sweep-retarget/`: Release SHA-256
`17d7fda73751a836dd090aa71c87b673c67b4bfd511238c9b4d1bd64603feb72`,
Debug SHA-256
`057c033ba05f1c35ad109e79c0662fdc09895ba3c6fc15ff138082f471b07dd2`.
Their source patch from `21e2d31a` has SHA-256
`92038ee3be415eef439d57672949923a97876e981a7363b6a1a4be2cd40f93b4`.
The exact old/new CLI script, stdout, stderr and LRATs are in
`benchmarks/probes/scratch/am455-sweep-fixed-target/`; job `1724272`
completed `0:0` using the preserved AM444 handoff CNF and random seed
2475268. The handoff CNF copied to `tests/fixtures/sweep-retarget-lrat.cnf`
has SHA-256
`70e00b6df8ee560596c5c9aba8c767aae19eed00085503a0d8059b89fdf41e1b`.

| Exact replay | Result |
| --- | --- |
| Old AM444 Release/Debug | Release exits SAT but CakeLPR rejects the partial LRAT at line 509, `clause not empty or singleton after reduction`; Debug aborts at the nonempty-clause/empty-chain assertion. |
| AM455 Release/Debug | Both exit SAT with 36,896-byte LRAT streams. The independent original-CNF model checker accepts the 237 clauses and 60 assigned variables. The frozen AM444 CakeLPR pin checks every emitted line and reaches only `empty clause not derived at end of proof`, expected for this SAT-path diagnostic. This is a partial-proof check, not a complete UNSAT certificate. |
| Focused CTest, job `1724291` | Release 4/4 and Debug 4/4 pass: `proof`, `sweep`, `sweep_proofs`, and new `sweep_retarget_lrat`. Frozen AM444 checker pins were supplied; job completed `0:0`. |

The new `sweep_retarget_lrat` CTest uses the original CNF, checks the SAT model,
and requires CakeLPR to consume the complete emitted LRAT prefix before its
expected missing-empty result. It fails on the old Release proof at line 509
and the old Debug binary aborts. This is a causal checker-backed regression,
not an instance-specific branch in Tactix.

## Remaining gates

Replay the AM444 API-UNSAT C1 handoff and require a complete CakeLPR-verified
original-CNF proof; a SAT-path prefix alone does not close that failure.
Launch one Opus-owned combined latest-source fuzz campaign with independent
original-CNF models and DSR→LSR→Trestle, DRAT and LRAT proofs, covering AM427
Sweep lifecycle, AM444 H1/C1, AM452 H2, and AM454/AM455 proof boundaries.
Run full latest-source Debug with enough time for demo3. All cumulative N1/N2,
N3/N4 performance and online-learning gates remain open. Do not submit Tier D.
