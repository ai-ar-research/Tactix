# AM479 bounded DSR deletion spool — immutable measurement authority

All solver measurements ran through SLURM on `cpu-dense-hwulab-q`, 12 CPUs and 32 GiB, without a task throttle or node constraint. The input was Rank 12, `benchmarks/cache/tierD24-26/8d4b50ec0cf99097e9ab0835937afee5-hwmcc17miters-xits-iso-6s281b35.sanitized.cnf.xz`, SHA-256 `95b11c4ece1727b5ef1486f093f3237e91cd6717b4ff52ebffb82a59d82c8c4d`. Each array task sequentially ran both arms on one node in reversed order. Exact commands and hashes are in `scripts/` and `results/`.

| job | purpose | source/binary |
|---|---|---|
| 1729831 | AM337 versus current AUTO and proofless baseline | AM337 `40b0de228744fb878d6e9cb478babd3fc589eb944b01b52f93e59475516df40c`; current `c57482f1551e48b6f460398790b605b75cb9149e96d38e8406e2b1f2b5315631` |
| 1729855 | old Congruence object in current binary (negative causality control) | `e4ffa2f4399d03699ff711033bf325562548f03b3566a0a1cd16b12d27b76b88` |
| 1729863 | old proof backend object in current binary (positive causality control) | `71299a1a350dc87b0a4bd216c4dcd6f6018b4819700034c2c6641a13c7b702f3` |
| 1729869 | current proof backend counters | instrumented scratch binary `e39094b34d8b1ca426986b38631daea384cd97ebc35fb448904c622d1786a18d` |
| 1729876 | first in-memory spool; rejected because a unit after a buffered deletion did not close the transaction | `365484d6e3cbfa38ae82bfbe077c5af32f73ebc1cbd0f141779d2d1a276c5bee` |
| 1729884 | corrected spool, byte-identical; two-node balanced pair | `01c6ab392f616e7cc617edc576101adc9a8af1683a1b383e59916d29d0d76be3` |
| 1729921 | final spool, direct stream-buffer append; two-node balanced pair | `09cd3e2f1c8fee62309fe89cc616e0b8a5892ddd879d6d23b788e7b4fb53c112` |

The final proof SHA-256 is `c794f8d973045fecd759d3b64fade0e7e42a9bcc1eeaaa3d156a9ec9303620f8`, exactly the current AM475 Rank 12 proof already independently checked by DRAT-trim and CakeLPR against the original CNF (`benchmarks/runs/20260926-am475-rank5-12-diagnostic/results/011.json`). Diagnostic old-backend proof SHA-256 `0ea7cf3f4e124f11a923f804d5384fa6e5ca436ad57889861c4ad9a08458a967` also passed independent DRAT-trim (`s VERIFIED`, 303.006 s) and CakeLPR (`s VERIFIED UNSAT`, 22.318 s) in job 1729907. That job's wrapper exited 1 **after both successful stages** because it tried to stat its temporary LRAT after leaving the temporary-directory context; the exact stage output and traceback are preserved in `results/proof-check-1729907.log`. The diagnostic checker error was in result bookkeeping and did not change any checker command or acceptance result. No proof bytes are retained here; large scratch proofs are diagnostic residue only.
