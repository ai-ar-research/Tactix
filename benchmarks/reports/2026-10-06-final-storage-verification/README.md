# Final storage verification

After the user-requested aggressive cleanup, `du -s --block-size=1 .` reports
**219,930,677,760 allocated bytes (219.93 GB)** with empty stderr. Initial usage
was 745,753,146,880 bytes. The three cleanup inventories record 528,110,623,232
bytes of removed generated files; repository delta differs because independent
verified builds, evidence and fuzz preparation were created during the sweep.

The standing [plan storage gate](../../../DEVELOPMENT_PLAN.md#81-benchmark-tiers)
is 700,000,000,000 bytes, so usage is safely below it. Totals and provenance are
in [usage.json](usage.json); exact removed paths are in the
[initial](../2026-10-06-resume-affine-repair-cleanup/cleanup.json),
[generated-build](../2026-10-06-generated-build-storage-cleanup/cleanup.json) and
[nested-build](../2026-10-06-nested-build-storage-cleanup/cleanup.json) inventories.
Main solvers, source archives, independent checkers and required evidence remain.
