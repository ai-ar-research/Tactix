# AM347: checker wait and PBP input boundaries

Unaccepted tooling-only checkpoint, 2026-09-24 UTC. No solver/checker binary,
rule, action boundary, scheduling, model reconstruction or certificate-routing
change. AM346 remains the solver candidate; full combined acceptance is open.

## Causes and repairs

AM343 terminal handoff H1 establishes a fail-closed tooling exception, not a
solver failure. drat-trim's signed-int seconds allowance accepts up to
2,147,483,647 seconds, but Python's poll selector converts its timeout to a
signed-int number of milliseconds. The outer captured wait consequently
overflows above about 2,147,483.647 seconds. No shipped benchmark allowance is
that large, but an accepted explicit allowance must still work.

`run_captured` preserves the existing `subprocess.run` path for ordinary
allowances. Larger allowances use repeated `communicate` waits bounded by
`floor(INT_MAX/1000)` seconds and a **single monotonic deadline**. Partial
stdout/stderr are retained by communicate, not concatenated again. Timeout
and exceptional exits kill/reap the child; successful and nonzero exits retain
their actual status and full decoded output. The helper is used by shared
routed/DSR/PBP checks, tier-runner checker calls, and fuzz.run_cmd, including
its unchanged CPU backstop. It does not shorten the caller's allowance or
change DRAT's ceil(timeout) inner `-t`. Runtime's measured-process path already
handles large allowances and is unchanged.

AM344 terminal handoff F1 establishes an independent false rejection:
VeriPB 3.0.2 rejects a blank/whitespace-only line before the CNF header, although
Tactix and the validated checker-spelling reader accept it. `pb_stages` passed
the original path to VeriPB and the checker spelling only to CakePB. Both now
receive the same validated spelling. It preserves the original clause/literal
sequence, IDs and multiplicities; neither the original file nor proof is
rewritten. CakePB still independently verifies the elaborated certificate.
This implements the existing original-formula contract without a new format,
solver parser relaxation, checker workaround accepting failed proofs, or scope
change to the approved startup PBP strategy.

## Evidence

Main logs and authority: `benchmarks/runs/20260924-am347-checker-boundaries`.

- Pre-repair fixed tests reproduce Python `OverflowError` at allowances
  2,147,484 and 2,147,483,647 and VeriPB's header error on leading blank lines.
  Logs: `red-timeout-v2.log`, `red-pbp.log`. The initial timeout test accidentally
  reused the adapter's output filename as its input; its failure is retained
  separately as a test-fixture error, not causal evidence.
- **26 bench-oracle tests pass**, including real routed/tier checks at the
  millisecond boundary and maximum accepted DRAT budget; forced multiple wait
  intervals; exact large stdout/stderr and nonzero exit; one deadline; timed-
  out child reaping; missing command; and unchanged normal delegation.
- **36 proof-dispatch tests pass**, including actual fuzz auto dispatch,
  routed PBP and runtime auto with named/anonymous scratch on three leading
  blank/comment/CRLF layouts and SAT negative siblings. They check originals
  remain byte-identical. The CRLF test originally compared text-mode reads
  (which normalize newlines); its corrected assertion compares bytes.
- **19 binary/runtime/anonymous tests**, **7 runtime integration tests** and
  the model-reader test pass. An old runtime unit expectation deliberately
  required VeriPB's original path; it now checks the validated path, exact
  original clause sequence, unchanged original bytes and both required stages.
- **Full harness selftest passes**, including lying/negated-model wrappers,
  truncated proofs, shrinking and conversion/native-LRAT controls. The two
  reference legs have 5 + 4 timeouts: 241/250 reached an oracle, not 250/250.
  Those timeouts are lost coverage, not successful checks.
- The exact AM344 handoff PBP rejects on raw VeriPB input and verifies in
  **six repaired pipelines**: check_routed, runtime named and runtime anonymous,
  each at 60 and 2,147,483,647 seconds. Both CNF and proof hashes remain
  unchanged. Evidence: `bounded-pbp-v2/results.json`, `check-handoff-v2.py`.
  Initial diagnostic v1 hit unsupported O_TMPFILE on the repository filesystem;
  v2 uses ephemeral anonymous scratch on a supporting filesystem. No acceptance
  or code change is attributed to that diagnostic setup failure.

No acceptance contract or reference checker invocation status was weakened.
The existing wrong-CNF, malformed-input, truncated-proof, abnormal-status and
failed-final-checker controls remain active and pass.

## Frozen validation authority

Manifest: `...-am347-checker-boundaries/manifest.json`.

| item | authority |
|---|---|
| base | `dfc8c56336a678310b5cc1424bb419142288f151` |
| tooling patch | `039885d2a87ce856492aed415b192ad96f8e1a073f8fdb0e5f2c1b6bdefbcae7` |
| tooling archive | `c5c8ae78d4dd03f78a5a3c5a314e158004101d454aab6c327a2122aefbee741f` |
| solver candidate | AM346 manifest `79812563...`, Debug `7e2abefb...`, Release `4e8a5306...` |

All seven checker binaries are unchanged. The archive is a tooling overlay
on the frozen AM346 source, not a newly built solver. Explicit Opus owner
**3916573**, session **60027**, owns independent validation via
`claude-fuzz-request.md`; campaign `...-am347-checker-boundaries-claude-fuzz`.
It covers old/new bounded controls, all executing affected surfaces, checker
and scratch/child hygiene, and focused default/random/layout validation.
Main reads only status, terminal report and explicitly bounded handoffs.
Pending evidence cannot be transferred into combined acceptance.

## Remaining work and hygiene

AM344's terminal report has no failure in its changed DSR surfaces, 8,000
focused cases and the F1 handoff repaired here. Its 169 repeated-input LRAT
failures are known debt under that older snapshot; AM345's owner remains live.
AM345 has a **new bounded solver memory-runaway handoff**:
`...-am345-lrat-restatement-claude-fuzz/repro/g1271/HANDOFF.md`.
Debug 722f1eb5, seed 345501271, explicit DSR, 97 variables/562 clauses,
deterministically hits the 32-GiB cgroup limit in isolated job 1716761.
The proof is empty, no checker/adapter runs, and the CPU limit is not causal.
Release and AM346 behavior are not established. Main owns debugging this next;
no successor benchmark gates may be launched on an assumption it is repaired.

AM346's Opus owner remains live: broad jobs 1716764--1716767, exact-case
1716768, focused 1716771, sanitizer 1716772, startup audit 1716773. They are
execution/reporting evidence only until terminal results exist. The preceding
pinned full CTest (87252, PID 3502786) still has its demo running after 178
passing tests. AM346 full CTest (77542, PID 3829651) also has its demo running:
178 passed, one main-command configuration failure, separately corrected
micro-proof test passed (AM346 report). Neither is called a completed full pass.
AM343 ncc diagnostic task 1716522_2 is still checking.

SLURM audit maps AM345 remaining tasks and AM346 jobs to their campaigns;
unrelated sdsl jobs/interactive allocations are untouched. No benchmark
material was deleted. Existing 272 AM337 gate cleanup records, frozen pins,
baseline authorities and active evidence remain preserved. The selftest's
temporary scratch was removed by its successful cleanup handler; the bounded
diagnostic evidence is retained. Documentation checks pass at the existing
hard budgets; PROGRESS stays 350/350, no compaction performed.

Next: diagnose the explicit AM345 memory handoff, consume independent AM346/
AM347 and remaining AM345 reports, finish full tests, then complete combined
fuzz and all four routed gates. The two late rows and all original acceptance
requirements remain in scope, with the user's declined techniques left closed.

Andrew deliverables: `report-to-andrew/20260924T030700Z-checker-boundaries/report.tex`
and `report.pdf`, compiled twice.
