# AM525: uniform immediate MAB completes all four solver phases and fails the gates

The user requested a full end-to-end trial of AE-Kissat-MAB enabled at every residual search entry, without the former delay or suppression logic. All four full-width solver phases have now completed on immutable local source commit `38796762876d72dd64c4402012d49d5444cbdbdc` and Release SHA-256 `6643ecb4409f086bffe94288ca4ab0126f013ce3e2727c06b98889a7129eb717`. Independent proof/model checkers and dependent scorers still run; these are solver-phase results and do not certify the solved UNSAT rows until their canonical routes finish.

| Tranche | Immediate-MAB solver outcomes | Measured solver seconds | No-MAB provisional/checked comparator | Gate |
|---|---|---:|---|---|
| Rank 1–50 | 47/50; 3 SAT timeouts | 11,341.334; PAR-2 14,941.316 | 50/50 provisional, 7,582.072 s; final checks pending | **Fail:** 50/50 and ≤8,521.522 s |
| B28+C5 | 33/33 | 1,221.359 | 33/33 independently checked, 968.588 s | **Fail:** ≤999.876 s |
| WV100 | 98/100; 2 SAT timeouts | 3,941.759 | 100/100 provisional; final checker pending | **Fail:** 100/100 |
| WV2–89 | 82/89: 52 SAT, 30 UNSAT; 7 timeouts | 9,102.311; PAR-2 13,302.311 | 79/89: 48 SAT, 31 UNSAT; 10 timeouts, 10,080.432 measured s | **Fail:** 89/89 |

Rank SAT rows 19, 26 and 34 timed out at 1,200 seconds on immediate MAB but solved on no-MAB in 133.334, 291.969 and 672.538 seconds. WV100 SAT rows 96 and 99 timed out at 1,200 seconds but solved on no-MAB in 976.755 and 92.618 seconds. B28+C5 retains coverage but exceeds its user-approved aggregate ceiling by 221.483 seconds. These failures alone prohibit promotion even if every pending certificate checks.

WV2–89 shows a real but insufficient gain: six SAT rows (162, 166, 196, 201, 203, 224) are solved by immediate MAB where no-MAB timed out; three rows (145 UNSAT, 184 SAT, 198 SAT) go the other way. Four rows (149 SAT, 155 UNSAT, 199 SAT, 221 SAT) time out on both. Thus immediate MAB gains three net solves and 978.121 measured solver seconds on this fixed tranche, but misses 89/89. The case-level changes demonstrate search-trajectory effects, not a universal overhead or a basis for instance-specific dispatch. The earlier delayed-MAB checkpoint avoided activation on all three now-lost Rank SAT rows, but the user rejected delay/suppression as the production strategy; no such special case was restored.

Canonical automatic proof/model checking remains in flight in arrays `1732443/1732444/1732445/1732446`; their dependent scorers are `1732457/1732458/1732459/1732461`. Full Debug `1732496` and Claude Code Opus AM522 also remain pending. No source, public X7 action, checker option/route, or instance-specific policy was changed during measurement. No push or Tier D production run was performed. The launch identities and resources are frozen in [AM521](2026-09-28-am521-uniform-immediate-mab-launched.md); the first irreversible Rank failure was recorded in [AM523](2026-09-28-am523-immediate-mab-rank-gate-failure.md).
