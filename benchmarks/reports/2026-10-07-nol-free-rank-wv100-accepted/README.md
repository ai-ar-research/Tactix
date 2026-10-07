# noL-free Rank1–50 and WV100 accepted

The maintained scorers accept both terminal campaigns on immutable1e8295106, Release27cf0cab924c52479d6c11885d284ae461d6c4911ef2e557847f58bad87e9469, with `--auto-proof`, unchanged residual seed policy and generic graph construction disabled.

- Rank1–50, Slurm1754089:50 dispositions,43 independently verified and7 clean proof censors;7,384.444 solver seconds below8,521.52175. No missing rows or gate failures.
- WV100, Slurm1754092:100 dispositions,98 independently checked and2 clean proof censors;2,645.524 solver seconds. No lost retained solve, incorrect verdict, checker rejection or other gate failure.

Exact manifest/scorer hashes and per-proof cost comparisons are in the two score JSONs. Run paths are `benchmarks/runs/20261007-nol-retired-1e8295106-regression-{rank1-50,wv100}`. A clean proof censor is an allowed regression disposition, not an independently certified target. Already accepted B28+C5 is recorded separately.

This accepts noL-retirement compatibility for these suites only. WV2–89, full Debug and Claude-owned current fuzz remain pending. Later FastEliminate and graph Window changes, and the original-seed diagnostic, are outside these pins. Cross-window totals are standing gate evidence, not isolated causal speedup measurements.
