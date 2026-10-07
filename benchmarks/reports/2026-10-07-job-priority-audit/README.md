# Job-priority audit — 2026-10-07

The user asked whether all running jobs were necessary. At 20:13 UTC, the following superseded campaigns were canceled; completed results and failure evidence remain intact.

| Jobs | Reason |
|---|---|
| 1754322 | Old noL-enabled targeted fuzz; noL is retired and current noL-free qualification is running. Claude owner1ace9500 was directed to report-only closure, with no replacement jobs. |
| 1754296, 1754403, 1754404, 1754405 | Four original-seed alternative-policy regression campaigns. The checked43.021s Rank401 result remains a control; the intended direction is actual Satsuma+Kissat branching parity. |
| 1754534 | Synthetic creation-order Rank401 rerun. Its Rank16/23 controls were negative; actual Satsuma handoff tracing supersedes it. |
| 1753421 | Thirteen remaining proof checks on the older noL-enabled0e6b95c91 full-44 campaign; they cannot qualify the current candidate. |

Slurm accounting records44 canceled allocation records (some aggregate pending array ranges), releasing328 allocated logical CPUs. This is not44 individual canceled tests. Pending grouped ranges also prevent191 task launches. No canceled or provisional proof is counted as accepted or as closing a target.

Retained at the audit snapshot: current noL-free Default/controller/targeted fuzz, full Debug, the last WV2 proof check, the current Rank569 experiment and focused Rank1004 checker-capacity comparison. The unrelated interactive allocation1751450 was untouched. Follow-on work prioritizes bounded actual Satsuma/Kissat tracing and a coherent candidate review; broad duplicate campaigns are not needed.

`decision.json` records the exact intent; `after-queue.psv` is a snapshot, not a current status feed. `cancellation-accounting.psv` also preserves completed/failed records from these families. The initial JSON pre-cancel capture failed to parse Slurm's structured numeric fields; no pre-cancel JSON artifact is claimed.
