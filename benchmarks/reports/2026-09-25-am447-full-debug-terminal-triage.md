# AM447: terminal old-pin full Debug suite

SLURM job `1723391` (AM431, immutable source commit `c776cc58`, source SHA-256
`19f30629...`) finished `FAILED`, exit `8:0`, after 4:36:03. Its serial Debug
CTest log completed all 187 tests. **186 passed; only `config_wiring` failed.**
The last and longest test, `demo3_scripted_default`, passed in 15,259.36 s.
The failing static audit reported `sweep_incomplete` unclassified in the
AM431 source snapshot's `tools/docs/check-config-wiring.py`.

The live tree classifies `sweep_incomplete` as rule-owned lifecycle state in
`NOT_CONFIG` (commit `ecfc82d1`), and its current `check-config-wiring.py`
passes (96 fields, zero failures). Thus this terminal failure is a stale
documentation-audit omission in the immutable AM431 snapshot, not a failure of
the 186 solver tests. It does **not** make the old-pin suite green. The fresh
AM444 full Debug job `1723644`, pinned to post-repair `421a38a1`, is the
relevant cumulative rerun and was pending when this report was written.

The paired AES checker array `1723443_0/1` is separately terminal and fully
verified; its original-CNF DSR-to-LSR-to-Trestle results are in AM443. The
AM431 result and the AES result do not close N1--N4.
