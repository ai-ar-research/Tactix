# AM524: uniform immediate MAB misses both Phase 1 solver gates

The user directed a full end-to-end trial of AE-Kissat-MAB enabled at every residual search entry, without the former delay or suppression logic. On immutable local source commit `38796762876d72dd64c4402012d49d5444cbdbdc` and pinned Release SHA-256 `6643ecb4409f086bffe94288ca4ab0126f013ce3e2727c06b98889a7129eb717`, the Rank 1–50 and B28+C5 solver phases are complete. Independent proof/model checkers continue, so these are solver-phase findings, not accepted final certificates.

| Gate | Uniform immediate MAB | No-MAB comparator | Earlier delayed-MAB checkpoint | Agreed ceiling | Solver gate |
|---|---:|---:|---:|---:|---|
| Rank 1–50 | 47/50 expected verdicts; 11,341.334 measured solver seconds; 14,941.316 PAR-2 seconds | 50/50 provisional solves; 7,582.072 seconds; checker completion pending | 50/50 checked; 7,553.134 seconds | 50/50, 8,521.52175 seconds | **Fail** |
| B28+C5 | 33/33 expected verdicts; 1,221.359 solver seconds | 33/33 checked; 968.588 seconds | 33/33 checked; 905.892 seconds | 33/33, 999.87615 seconds | **Fail** |

Rank SAT rows 19 (`knight_18`), 26 and 34 each returned UNKNOWN at the 1,200-second solver limit. The no-MAB pin solved these in 133.334, 291.969 and 672.538 seconds respectively, with original-CNF model checks where final. A clean timeout is a solver-coverage failure; it is not a proof-checker rejection. Immediate MAB recovered Rank 29 from the no-MAB 348.839 seconds to 46.496 and Rank 30 from 280.552 to 120.916, but several other rows regressed: Rank 16 121.121 to 332.791, Rank 21 137.504 to 447.688, and Rank 46 222.806 to 414.282. The source shows the general owner's bandit, not a benchmark-specific selection; per-row gains do not rescue aggregate or coverage failure.

B28+C5 retained all 33 verdicts but its 1,221.359 seconds exceed the 5%-leeway ceiling by 221.483 seconds. It recovered the no-MAB slow rows 17 (131.586 to 43.042) and 19 (74.140 to 48.898), while losing time elsewhere; row 33 alone took 422.818 seconds versus 314.368 on no-MAB. This is a broad search-trajectory tradeoff. No source, public X7 action, checker route, or benchmark selection was changed in response.

The terminal dependent scorers `1732457` and `1732458` will record final proof/model coverage when their arrays finish checking. WV100 `1732445`, WV2–89 `1732446`, full Debug `1732496`, and Claude Code Opus AM522 fuzz remain active. They cannot reverse the Phase 1 failures but will complete the user's requested usual-set comparison. No Tier D production run is authorized by these results. The frozen campaign manifests and checkers are documented in [AM521](2026-09-28-am521-uniform-immediate-mab-launched.md).
