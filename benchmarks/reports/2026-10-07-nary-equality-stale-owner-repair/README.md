# Native equality stale-owner repair

The cumulative `4096d41b3` fuzz campaign was rejected for a native `NaryCspEquality` offer returning InvalidRequest after search changed its retained owner. The [terminal Claude report](../2026-10-06-integrated-4096d41b3-fuzz-owner/FINAL_REPORT.md) owns the failure and rerun scope.

Live recapture now classifies lost retained structure as Stale; malformed coordinates remain Invalid. Native enumeration reauthenticates each eligible retained owner within the remaining page and action recognition allowance, preserving exact exhaustion/interruption statuses and skipping owners outside the caller's domain bound. No shared controller settlement or proof semantics change.

Focused Debug and ASan/UBSan each pass **9/9**, including equality, live/public actions, independent proof/model oracles, X20 and restart-controller tests. ASan uses leak detection and UBSan halt-on-error; proof shadow remains enabled. New fixtures cover root assignments, deleted premises, mutation-free stale declines, malformed requests, bounded enumeration, interruption and smaller domain limits. Logs are [Debug](debug-ctest.log) and [sanitizer](asan-ctest.log).

The completed storage scan is **452,944,463,759 bytes** (452.94 GB), below the 700-GB cleanup gate, with no scan errors. Current campaign evidence and active pins remain protected.

These focused results do not accept the repaired cumulative checkpoint. Claude must rerun the affected x20/driver targeted kinds and all three minimized reproducers on the immutable new commit; the owner determines retained-evidence applicability. Full Debug and B28+C5, Rank1–50, WV100 and WV2–89 remain pending.
