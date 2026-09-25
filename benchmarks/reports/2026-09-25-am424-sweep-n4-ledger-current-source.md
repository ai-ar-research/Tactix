# AM424: current-source Sweep N4 ledger correction

Date: 2026-09-25. Source `a3be3e52`; this is a codebase/ledger audit, not a new solver behavior or N4 acceptance claim.

Two live scheduling-parity rows had kept their 2026-09-08 **isolated-candidate** wording after both repairs entered production in the 2026-09-12 online-strategy refactor (`7a668e76`). The frozen September 8 reports remain correct as historical observations, but they no longer describe the current source.

| Row | Current source and focused evidence | Residual gate |
| --- | --- | --- |
| `SP-SWEEP-SMALL-1` | `sweep_process_seed` enters the actual Kitten import/initial-solve/model lifecycle for a live empty or one-clause environment; an absent fixed/retired environment still declines. `test_sweep_small_env` covers one-clause widths 2/3/17/270, empty/singleton, fixed/retired, repeated sessions and the strict outer versus inclusive inner budget. `git blame` dates the production path and test to `7a668e76`. Current Release and ASan+UBSan focused tests pass. | No current-source full cumulative Rank/B28+C5 acceptance or affected-scope fuzz was supplied by this audit. The historical native trace and timing are not silently transferred. |
| `SP-SWEEP-3` | `sweep_revalidate_retained` filters front and remaining tail **once at call entry** with `sweep_schedulable`, before environment mutation, retaining same-call representative rescheduling and public pure-seed legality. `test_sweep_retained` covers holes/duplicates, sparse/dense occurrence caps, root-fixed/retired seeds, completion and no per-pop recheck. Source and test are from `7a668e76`; current Release and ASan+UBSan focused tests pass. | Full `flags.sweep`/incomplete/fresh-admission/completion parity, current-source fuzz and checked cumulative performance remain open. |

`docs/scheduling-parity-ledger.md` and `docs/implementation-notes.md` now say **implemented, acceptance open** rather than claiming the shortcut/omission is still in production. The ledger's five-row recent status and overall N4 verification are not promoted. Documentation size, citation, option-coverage, inventory, proof-rule sync, section-reference and rule/policy-boundary checks pass. No solver or checker was run on a real instance and no benchmark result was reinterpreted as a current gate.
