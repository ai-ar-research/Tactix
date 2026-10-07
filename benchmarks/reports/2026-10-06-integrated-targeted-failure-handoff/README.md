# Cumulative targeted failure handoff — 2026-10-06

Candidate source: `4096d41b3`. The [unaltered Claude owner response](owner-verdict.json) dispositions cumulative acceptance as **FAILED**, despite passing broad DefaultPolicy and random-controller legs and the changed affine/X20 surfaces.

The 13,155-task targeted matrix (job `1751475`) found a deterministic native `NaryCspEquality` lifecycle defect: retained-owner recapture after root mutation returns `InvalidRequest`, terminating the controller with UNKNOWN. The bounded [debug handoff](../../runs/20261006-integrated-4096d41b3-fuzz-owner/handoff/HANDOFF.md) owns reproducer commands, hashes and transcripts. No proof/model rejection or sanitizer defect was reported for this failure.

The owner reports 26,650 graded SAT and 16,203 graded UNSAT runs, with all known-bad controls rejected. Coverage exclusions, timeouts and deliberate-stop UNKNOWN rows remain explicitly excluded in the owner response. CLI predecessor restore reach was below its declared minimum; continuation job `1751480` is scheduler-completed and awaits owner grading in the same session. This is an interim failed disposition, not terminal coverage acceptance.

Next: repair X18 stale-owner classification/admission without changing valid public requests or deterministic replay; validate focused model/proof and sanitizer checks, then have Claude rerun affected targeted kinds and three reproducers. Full Debug and B28+C5, Rank1–50, WV100 and WV2–89 remain unstarted for this cumulative checkpoint.
