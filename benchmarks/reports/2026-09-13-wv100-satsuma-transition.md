# WV100 Satsuma transition screen — 2026-09-13

This screen tests whether the 57 Satsuma+Kissat-owned WV100 rows share a large,
early symmetry transition. It does not rerun a winner for timing, count as a
Tactix solve, or establish a checked proof route.

Job `1687657` ran Satsuma 1.3/Dejavu 2.1 binary
`0680c895a2167cff07dc1e89e0299344738080493aceaf61ab57420417f32ee5`
on `cpu-dense-hwulab-q` with four CPUs and 32 GiB. It invoked `satsuma fix`
with BSR and reduced-output-unit support. The job completed in 1:41; all 57
invocations returned zero. Its aggregate SHA-256 was
`41de958d01c75c88bc960aef77cb49f0ae31d6b39252b7d5db7a39a037bb6855`.

Input CNFs, residual CNFs and DSR proofs were anonymous file descriptors and
were closed after every invocation, including exception paths. The 57 bounded
JSON summaries occupied about 1 MiB; no formula or proof file remained. This
predates and does not redefine the subsequent eight-CPU/300-second focused
experiment default.

## Result

- 52 of 57 residuals were the immediate contradiction `p cnf 1 1`.
- Maximum preprocessing wall time was 48.970 seconds; median was 0.220 seconds.
- The anonymous DSR streams totaled 97,672,168 bytes; the maximum was
  13,416,447 bytes.
- A priority classification of the final Satsuma counters gives 51 structured
  row/row-column cases, five Johnson cases and one remaining general
  automorphism/Schreier case. Some structured cases also exercised Dejavu or
  Schreier, so this is an implementation-order classification rather than a
  claim of exclusive mechanisms.

The five nontrivial residuals were:

| WV100 rank | Instance stem | Residual | Satsuma wall |
|---:|---|---|---:|
| 37 | `DSC125.9.25` | 3,125 vars / 14,144 clauses | 0.127 s |
| 67 | `DSC125.9.30` | 3,750 / 38,808 | 0.165 s |
| 82 | `latin_square_10.15` | 13,500 / 3,489,290 | 11.024 s |
| 88 | `xor_op_n36_d3` | 2,520 / 346,196 | 48.970 s |
| 91 | `worker_30_60_25_0.9` | 1,015 / 4,346 | 2.957 s |

The screen supports starting with structured symmetry because it produces the
largest observed transition class. It does not show that the residual alone is
UNSAT, that the DSR stream checks, or that native DRAT/LRAT expansion has
acceptable cost. Those remain pre-implementation falsification gates in the
WV100 mechanism protocol.
