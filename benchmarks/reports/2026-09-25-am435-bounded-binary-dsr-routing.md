# AM435: bounded-memory binary DSR routing and conversion

Date: 2026-09-25. Tooling-only repair for future large certificates; no
solver, proof emitter, checker binary, canonical checker options, or active
campaign pin changed.

Before this change, `proof_oracle._dsr_lines` read an entire binary DSR file
into Python memory to detect witnesses. `dsr_proof_as_drat` then read the
whole file again into a mutable bytearray before rewriting line tags. A
certificate near the 32-GiB task limit could consume several proof-sized
Python allocations before any checker ran. The routing/adapter is shared by
the benchmark runner and the anonymous Tier-D runtime, so this was a general
resource failure risk, not an AES-specific checker explanation.

Binary DSR parsing now streams fixed 1-MiB chunks through one varint/tag
state machine. Witness detection keeps only the current addition's first
literal and kind; the optional full-line iterator still exposes its previous
list-of-tokens API for tests/callers. The witness-free binary-to-DRAT adapter
validates the stream before opening its output, then rewrites one chunk at a
time; a witnessed or malformed proof cannot leave a partial output. The
ASCII route is unchanged. Exact line tags, literal varints, rejection of bad
tags/overflow/truncation, and no-option DSR-vs-DRAT routing are unchanged.

The new regression forces a five-byte scan chunk so a varint crosses a chunk
boundary; it checks parsed lines, content route, exact DRAT bytes, witness
rejection, malformed-stream rejection, absence of partial output, and an
instrumented input that rejects unbounded `read()` calls. Focused oracle,
binary-checker and runtime suites pass **46/46**. The required
`tools/fuzz/selftest.sh` passes: 36 harness unit tests, zero unexpected
failures on the Kissat/CaDiCaL reference legs, and the known-bad model,
truncated-proof, shrink and duplicate-input controls all trigger their
intended checks. `tests/test_campaign_certificate_cleanup.py` passes;
Python compilation and `git diff --check` pass. The reference-leg timeout
rows are explicitly unverified in that self-test and are not counted as
fuzz acceptance.

The running AM434 AES diagnostic array `1723443` copied the **old** oracle
SHA-256 `f7b6559b1108a839ed37ace518c1c880e599e72b54cb279ab474bc0e82532a3f`
into its immutable pins before this source edit; its results are unaffected.
The new routing code will be pinned only by future campaigns. AM434 checker
verdicts, H2's independent no-option DSR failure, N1--N4 and all cumulative
gates remain open. No Tier-D run was submitted.
