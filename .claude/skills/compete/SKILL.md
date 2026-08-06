---
name: compete
description: Check or implement SAT Competition compliance — DIMACS parsing, output format (s/v lines), exit codes, proof file handling, signal handling, packaging for submission. Use when touching the competition frontend (main), preparing a submission, or when a harness/checker rejects our output.
---

# Competition compliance (sequential main track)

Authoritative spec: DEVELOPMENT_PLAN.md §6; rules at satcompetition.github.io. Compliance bugs disqualify solvers — treat this checklist as tests to automate (`tools/compete/check.sh`), not documentation.

## I/O contract

- **Input:** DIMACS CNF as file argument. Parser must be streaming and huge-file safe (2025 runs died parsing at 20 GB on 30 GB hosts); tolerate comments anywhere, arbitrary whitespace, missing final newline. Malformed input ⇒ `s UNKNOWN`, exit ≠ 10/20.
- **stdout only** (nothing required on stderr): `c` for comments, exactly one `s SATISFIABLE` / `s UNSATISFIABLE` / `s UNKNOWN` line. (2025 slides showed real solvers shipping the typo "SATISFIALBE" — test with exact string compare.)
- **SAT:** `v` lines with the full assignment terminated by `0`; keep lines ≤ 4096 chars. The model must satisfy the *original* formula — reconstruct through ρ before printing (elimination rules!). Validate with `tools/fuzz/checkmodel` in every compliance test.
- **Exit codes:** 10 = SAT, 20 = UNSAT, anything else = not solved. These must match the `s` line under all paths, including error paths.
- **Proof:** written to the path given as the second file argument (binary DRAT default; `--lrat` variant). Stream + buffer — never hold the proof in memory. On SIGTERM/SIGXCPU/SIGINT: flush proof, print `s UNKNOWN`, exit cleanly. Flush stdout before exit, always.

## Compliance test procedure

Run `tools/compete/check.sh ./build/tactix` (build it to do the following if missing):
1. SAT instance → exit 10, `s SATISFIABLE`, model passes checkmodel, `v` line format valid.
2. UNSAT instance → exit 20, `s UNSATISFIABLE`, proof file exists and drat-trim verifies it end-to-end **in the same invocation style the competition uses**.
3. SIGTERM mid-solve on a hard instance → `s UNKNOWN`, sane exit code, proof file flushed and still parseable by drat-trim (prefix-valid).
4. Garbage/empty/comment-only inputs → no crash, `s UNKNOWN` or correct trivial answer.
5. Determinism: two runs, identical verdict and (for fixed seed) identical proof.

## Submission packaging (phase 6)

- Dockerfile + StarExec-style `bin/starexec_run_default` script; build from clean checkout.
- Solver description document (required; 1–2 pages, IEEE style — see prior years' description PDFs for format).
- Sources must be open-sourced per competition rules; verify LICENSE and Kissat attribution notes are in order.
- Rehearse the full competition toolchain locally on Tier B before submitting: solver → proof → the year's announced verified checker (cake_lpr/GRAT), within realistic memory caps (16 GB).
