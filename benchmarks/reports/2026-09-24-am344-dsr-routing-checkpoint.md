# AM344: DSR routing rejection and clause-preserving reader spelling

Unaccepted tooling-only checkpoint, 2026-09-24 UTC. No solver, checker binary,
rule, guard, scheduling or success-denominator changes. No promotion.

## Cause and repair

The Python DSR router indexed past EOF in truncated binary literal varints;
unterminated ASCII lines or invalid integer fields raised ordinary ValueError.
These escaped the runtime proof-checking boundary and became INFRA_ERROR,
obscuring a rejected certificate. The new DSRFormatError class is caught only
at that boundary. check_routed returns a route rejection; runtime records a
rejected route stage and not-verified, retaining the solver's UNSAT verdict.
Filesystem failures still propagate as infrastructure errors.

Routing and binary-to-DRAT rewriting now share a bounds-checked uint32 varint
reader. Diagnostics identify the offending byte or ASCII line. No output is
written by a failed binary rewrite. ASCII encoding recognition now admits its
ordinary whitespace (including tabs). This parser is not a substitute proof
oracle: once a witness selects DSR, the pinned checkers still decide validity,
including later malformed bytes. PBP and valid DRAT routing are unchanged.
The reference encoding is implemented in dsr-trim-sr/src/global_parsing.c;
the independent checkers remain mandatory.

Native dsr-trim formerly received original layout while Trestle received the
validated checker spelling. The tier runner's native DSR arm passed original
layout to both. The reference parser's newline/comment handling is limited
for legal CRLF/mixed/split layouts. Both elaborator and verifier now receive
the same validated spelling through the shared stage builders; the tier runner
creates it too. Original CNF bytes, clause order and IDs, literal order and
multiplicity, and tautologies are preserved. No deduplication is introduced;
solvers always consume the original input. Backward and every-lemma pipelines
retain their independent checking and negative controls.

## Validation and authority

The two new malformed-route tests initially produced 15 exceptions and one
wrong-stage assertion before repair. They now reject the same malformed bytes
without invoking a checker or writing rewritten proof output. Missing-file
controls remain OSError. Valid varint boundaries through uint32 maximum retain
their bytes in the rewrite; tab-separated ASCII routes correctly. These are
parser/rewriter tests, not proof-acceptance claims about huge variable indices.

Real-checker regressions cover three legal layouts and two DSR encodings
through fixed native checking (backward plus every-lemma), tier checking and
runtime witnessed DSR. All 18 positive surface cases verify; the corresponding
18 SAT/wrong-CNF cases reject. Original bytes are asserted unchanged. Existing
fuzz-dispatch assertions now require checker.cnf for both DSR CNF readers.

Final suites: proof-dispatch **31/31**, bench proof-oracle **23/23** (including
two pinned-binary tests), bench
runtime **14/14**, anonymous scratch **3/3**, runtime integration **7/7** and
assert-style model tests pass. Full harness selftest passes, including known-bad
model/proof wrappers and shrinking. Its reference runs reach 145/150 Kissat
and 96/100 CaDiCaL verdicts; nine timeouts are lost coverage, not checked cases.
Logs: `benchmarks/runs/20260924-am344-dsr-routing/logs/{fixed,binary-oracles,selftest}.log`.
All 293 frozen tooling hashes match after testing. Seven documentation CTests
pass; the size checker reports zero errors and six warnings (PROGRESS 338/350).

Immutable authority `benchmarks/runs/20260924-am344-dsr-routing/manifest.json`:

- base `d16f119aaa80f53c4fab20b826180d106fe73a8d`;
- patch `ec4214844b26c0e1823ebecb0c9ce2d2cbf3568774044b41c32a6dcaaed03730`;
- tooling archive `6ce9470de21794eae4f1daa050cbf3fdf0a612b80d397b4ef14c23ece9aa140b`;
- unchanged AM341 Release cbe99910, Debug 722f1eb5 and seven reference checkers.

The explicit Opus owner (PID 2591052, session 74882) owns independent affected
validation in `...-am344-dsr-routing-claude-fuzz`. Its frozen request requires
old negative controls, all changed checker surfaces, original-CNF models,
2,000 focused generated cases per build/policy (8,000 total), random replay
and terminal reporting. This is pending, not accepted. Codex owns debugging;
only published status/report/bounded handoffs are consumed.

## Remaining work and hygiene

AM341 owner status reports four native DSR rejections in traced runs and replay,
including unchanged Release. Its bounded triage 1716560 must supply the exact
handoff for main-agent debugging. Release Lucky floor is still open (23/50 in
the first array); top-up 1716566 belongs to that owner. Full Debug CTest remains
178/179 passed with the demo running, session 3825; do not restart it.
Debug random job 1716500 has ended in SLURM, but queue completion alone is not
owner acceptance. AM342 affected validation continues independently.

AM343's original owner exited 0 without a terminal report. Recovery Opus PID
2470638 / session 88761 resumes its SAME jobs and scope, preserving tooling
1716530, oracle 1716565 and smokes 1716568/1716569. Main diagnostic 1716522 still
has ncc checking after UNSAT in 242.680 s; its other three rows verify with
complete cleanup. Neither this diagnostic nor any old gate transfers to a new
full-tier acceptance claim. Repeated-literal routed LRAT mismatch remains open.
After causal repair and independent fuzz, complete successor full gates before
the two late rows' requested AM333 matched-work comparison. Exclusions remain.

Active/pending jobs map to the preserved AM341/342/343 owner campaigns and the
main diagnostic; the AM344 owner is preparing separately. Unrelated t4-ws500
1714784 and sweep-repeat 1716485 map to /home/hwu/sdsl, and interactive
allocations are untouched. No benchmark evidence, pins, cache, authority,
directory or job was deleted/cancelled. Successful selftest scratch is cleaned
by its maintained harness. All 272 terminal AM337 results retain their complete
cleanup markers; live proof artifacts remain owned by their campaigns.
Documentation size debt is reported without automatic compaction.

Andrew deliverables: `report-to-andrew/20260924T020100Z-dsr-routing/report.tex`
and `report.pdf`, compiled twice. PROGRESS owns subsequent status.
