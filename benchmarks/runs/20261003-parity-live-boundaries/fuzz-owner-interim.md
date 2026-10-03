# Owner status — winner graph-constructor / shared-guidance fuzz (PRELIMINARY)

Status: **RUNNING** — no terminal evidence yet; nothing accepted.

- Owner: headless Claude session, PID 2213643 (started 2026-10-03 15:16 local).
- Snapshot: `source-snapshot.tar.gz` sha256 1f2a3b98c423afa5c5cff40f48500e31d95cb685a353b072f124b25c86566ffb
  (base d85b30b4 + WIP, action-space v2). Verified: 446/446 hashed paths match
  both the root checkout and the tarball. Unhashed build inputs (`examples/`,
  `third_party/`) are unchanged from d85b30b4.
- Pin: `build/pin-winner-graph-fuzz-debug-1f2a3b98` (Debug, proof shadow ON).
  `tactix` sha256 59830ca4a77c3ca09bd7b17b8a15745f91f1b281ea9dde1c2951786e27a316fe.
- Checkers: frozen collection SHA256SUMS 895f1989…, byte-identical to the
  accepted clean collection (dsr-trim 3b5c7737…).
- Coverage plan frozen before the legs were launched. Its hash is in the
  private campaign's `status/plan-freeze.sha256`.
- Codex-owned probe 1747574: not inspected or touched.

## Job handles (status monitoring)

| Job | Purpose | State |
|---|---|---|
| 1747585 | pin build | COMPLETED |
| 1747588 | labelled smoke checkpoint (8 cores, not counted) | COMPLETED, job status 0 |
| 1747609 | labelled focused smoke 2 (graph rows + driver controls, not counted) | running |
| 1747613 | L1 DefaultPolicy leg, 100,010 seeds (2060000000+) + corpus | COMPLETED: 0 failures |
| 1747614 | L2 random-controller leg, 100,010 seeds (2061000000+) + corpus | COMPLETED: 0 failures |
| 1747627 | full targeted job: controller-foundation rows plus graph/guidance rows, 50 physical cores, 9,939 phase-1 tasks | running (selftest OK) |
| 1747628 | mining job: coverage replay of L2/L1 seeds with feedback, never fresh credit | running |

Interim leg results (not yet the terminal report):
- L1: corpus 51 entries, 0 failures. Entry 022 is the declared crash-only
  entry. 100,010 cases, 0 failures: SAT 57,225, UNSAT 40,148, non-terminating
  2,637 (2.6%, 5 s).
- L2: corpus 51 entries, 0 failures. 100,010 cases, 0 failures: SAT 56,408,
  UNSAT 37,894, non-terminating 5,708 (5.7%, 5 s).
