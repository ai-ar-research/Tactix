# Integrated terminal Claude fuzz disposition — 2026-10-06

Source `4096d41b3`: **FAILED; not accepted**. The [unaltered terminal owner response](owner-verdict.json) and [Claude final report](../2026-10-06-integrated-4096d41b3-fuzz-owner/FINAL_REPORT.md) own the result. No campaign jobs remain live.

Both broad policy legs, affine publication/continuation and predecessor coverage, X20 coverage, checker negative controls, and graded replay/purity checks passed. The sole blocking source failure is native `NaryCspEquality` stale-owner handling, with exact reproducer and rerun scope in [the failure handoff](../2026-10-06-integrated-4096d41b3-fuzz-owner/FAILURE_HANDOFF.md).

The final restore continuation reaches 69 checked CLI runs with 100 restores (57 runs under restart/full retention); all 840 witnessed runs have matching extension/rebuild counters. Timeout, deliberate-stop, status-only, skipped, unreachable and private-harness failures remain excluded from graded acceptance as detailed by the owner. Partial-reduction rebuild is reached only in the matrix differential, not CLI.

Repair the bounded source defect, validate focused Debug/sanitizer/proof/model tests, and ask Claude to rerun affected targeted kinds plus all three reproducers. Full Debug and the four regression suites remain pending for the repaired checkpoint.
