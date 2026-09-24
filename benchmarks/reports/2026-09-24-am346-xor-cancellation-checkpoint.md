# AM346: certify cancelled inputs in XOR matching

Unaccepted solver repair checkpoint, 2026-09-24 UTC. The original bounded
AM341 proof rejection is repaired and independently checked. Combined fuzz,
the other five reported cases, full test completion and successor benchmark
gates remain open. No promotion or performance claim.

## Cause and shared repair

AM341's terminal owner report is FAILED, despite passing assertion-focused
coverage. It reports six deterministic native DSR rejections with correct
UNSAT verdicts. The Release binary is unchanged from AM339, so the assertion
repair did not introduce them. Lucky19/20 firing floors were met: Release 68,
Debug 16; the never-guard-true Propagate boundary fixture remains a reach gap.

Main consumed only the explicit bounded `triage/dsr-reject/reproducer` bundle.
Its 27-variable/80-clause case has SHA256 `020421a5...`; seed 343401436 produces
the rejected binary proof `41bfb018...` (4,097 bytes). Main authority:
`benchmarks/runs/20260924-am346-dsr-population/manifest.json`.
GDB jobs 1716673, 1716693 and 1716725 preserve the same immutable Debug binary
and original failure. Both ordinary and forward dsr-trim reject; this is not
solely a backward-checker issue.

The rejected `7 -18` addition comes from the semantic Congruence matching-
contradiction action (payload 51539607565), not a newly emitted SR witness.
Its first original XOR gate has four inputs, the second two, while the shared
canonical key has two. The matching ladder quantified only that reduced key.
Inputs cancelled semantically still exist in the original defining clauses:
without assigning their representatives, the top lemma need not be RUP.
Earlier Sweep bridge additions/deletions were investigated; their surviving
kernel bridge was not established as the defect.

A direct fixed API regression confirms the cause independently: pre-repair
code emits `1 -7 8` on two equivalent XOR outputs, but a small independent
unit-propagation oracle rejects the addition. The old test executable is
retained (`pins/test-xor-before`, SHA256 `f67bc86b...`), with its assertion log
`red-fixed.log`. The reference Kissat implementation proves XOR shrinking
before matching (`add_xor_shrinking_proof_chain`, `rewrite_xor_gate`); CaDiCaL
also retains shrinking proof obligations before its matching chains.

The shared Tactix ladder now quantifies the union of cancelled representatives
from **both** gates and all but one remaining unassigned shared input.
Representatives already assigned by negating the output conclusion need no
split. Assigning the top prefix lets the original gate clauses and existing
equivalence bridges propagate the last shared parity input; lower levels
resolve the two opposite cases. Canonical and individually selectable semantic
actions use this same builder. The latter retain both cancellation sets during
enumeration. This constructs the missing proof, rather than removing actions.

N1 guards, payloads and action boundaries are unchanged. N2 uses only RUP plus
existing newest-first transient deletion; original formulas and reconstruction
are unchanged. N3/N4 scheduling, effort and default decisions are unchanged;
new runtime gates remain required. The calculus and implementation ledger
describe the proof obligation without introducing a scheduling restriction.

## Validation and frozen candidate

Authority: `benchmarks/runs/20260924-am346-xor-cancellation/manifest.json`.

| item | authority |
|---|---|
| base | `56333ea8dcc6c1740fb5206b617996f3f5d156c0` |
| source patch | `4de87c935c5656838c8f0ed186c815ffb4cf5bf7416a9c7d984511b02890aece` |
| source archive | `c662330923f77ccfd221003c1a7724c0514ab3a0eb0545715f1996032afa3455` |
| Debug | `7e2abefb468ff7d41250135c0f5defcb862e3bc6d9e33ecc45b010c8a7ad936e` |
| Release | `4e8a53068655b5a2d24d813cc9dec6e58b6f6a130e16757ec04c56382bc93b1b` |

All seven checkers remain byte-identical AM341 pins. No submitted build was
rebuilt; new mutable builds and immutable `build/pin-xor-cancellation-*`
directories are separate from the preceding full CTest build.

- Debug and Release each pass **192 fixed cases**: cancellation in either or
  both gates, signed bridges, SAT equivalence/UNSAT contradiction, canonical/
  semantic actions, proofless/DSR/DRAT/LRAT. Models are exhaustively checked,
  action availability and effects asserted, and every emitted addition is
  checked by an independent RUP oracle, including unused lemmas.
- **72 fixed UNSAT certificates** independently verify: 48 through backward
  and every-lemma DSR/Trestle, 24 through native LRAT/CakeLPR. Evidence:
  `...-am346-dsr-population/fixed-check-results.json` and `fixed-check.log`.
- SLURM **1716762 COMPLETED, exit 0**: **30/30 original handoff runs** verify
  on both frozen binaries. Eighteen native DSR runs cover auto/explicit DSR/
  ASCII, random, fresh replay and original replay; twelve strict DRAT/ASCII-
  DRAT/LRAT runs cover fresh random and replay. Matching Debug/Release proofs
  are byte-identical. The repaired binary DSR is 4,177 bytes, `728a2460...`,
  identical across original and fresh replay. The preserved old proof rejects.
  Full commands and outputs: `...-am346-xor-cancellation/validation-1716762`.
- The original CNF is retained as corpus **038**, with its required random
  seed in metadata. The direct microtest supplies the small causal fixture;
  the original random-policy reproducer is not claimed to be minimized.
- Fresh 180-entry Debug CTest remains **live** (PID 3829651, session 77542).
  At checkpoint 178 passed, one command-configuration failure, demo pending.
  The failure was main's `LRAT_CHECK=cake_lpr`: the lrat-check slot expects
  `c VERIFIED`, whereas CakeLPR correctly prints `s VERIFIED UNSAT`.
  No solver failure was shown by this invocation. Repeating `micro_proofs`
  with `CAKE_LPR` set correctly **passes in 37.11 s**. Both logs are preserved:
  `ctest-debug-all.log`, `ctest-micro-correct-env.log`. This is not described as
  a completed all-pass full invocation.
- The preceding AM345 pinned full suite also remains live (PID 3502786,
  session 87252), with 178 passing tests and its demo pending. Its pinned
  sweep check passed; the earlier sweep rejection came from a different,
  unpinned local dsr-trim. Do not modify those local checker binaries.

An explicit Opus-only Claude owner is live: PID **3831909**, session **82572**,
request `...-am346-xor-cancellation/claude-fuzz-request.md`, campaign
`...-am346-xor-cancellation-claude-fuzz`. It owns exact-six-case rechecks,
focused cancellation/action coverage, the full affected default/random matrix,
sanitizers and reporting. Main reads only status, terminal report and explicit
bounded handoffs. Preparing or submitted jobs are not acceptance.

## Other obligations and hygiene

AM343's terminal report records 8,320 focused cases without a new solver
failure, 226 lost-coverage timeouts, and bounded H1: outer Python poll overflow
above 2,147,483.647 seconds in check_routed/run.py. Repair remains due.
AM344's live owner reports F1: valid PBP with a leading blank input line is
rejected by VeriPB reading the original layout; this is separate from its DSR
spelling repair. AM344/AM345 owners and focused matrices remain independent
and active. Neither finding is waived by the XOR repair.

Job audit maps remaining AM344/AM345 array tasks to their published campaigns;
AM343 ncc diagnostic `1716522_2` remains live. Unrelated `/home/hwu/sdsl` jobs
and interactive allocations are untouched. All 272 terminal AM337 gate results
still report certificate/derived cleanup complete and no cleanup pending.
No benchmark artifacts were deleted this increment. The small causal proofs,
traces, checker outputs and old failure binary are retained as diagnostic
evidence; frozen authorities and active campaigns are preserved.

Documentation budgets pass but have no reserve: PROGRESS 350/350, calculus
680/680, implementation 560/560, plan 750/750 and parity ledger 600/600.
Only current status was updated; no compaction was performed.

Next: consume the AM346 owner result, finish full test evidence, repair H1/F1,
then complete combined fuzz and all four routed gates. Only afterward compare
the two late rows against their owner at matching work. User-declined rows
remain excluded from new technique work; the full success denominator and
N1--N4 are not narrowed.

Andrew deliverables: `report-to-andrew/20260924T025100Z-xor-cancellation/report.tex`
and `report.pdf` (local, compiled twice).
