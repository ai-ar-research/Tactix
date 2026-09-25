# AM437 H2 unit-publication controls

`probe.py` decodes the pinned AM436 native DSR into ASCII and writes nine
checker-only variants. It checks the AM436 original-CNF/proof hashes and the
frozen AM369 checker hashes before running. Each variant is checked with
the canonical no-option `dsr-trim` in backward and forward modes; any
successful elaboration would also go through Trestle. `result.json` records
exact statuses and first errors. `artifacts.tar.gz` preserves all generated
proofs, partial LSRs, and raw checker stdout/stderr; its contents were
compared byte-for-byte with the generated files before the latter were
removed from this directory. Archive SHA-256:
`3f456edb9ca11b546c760a90d85e7c737f0e96df2bb2699f5ece3d745eb884d2`.

No solver, emitter, checker pin, or production route was changed. See
`benchmarks/reports/2026-09-25-am437-h2-unit-publication-controls.md`.
