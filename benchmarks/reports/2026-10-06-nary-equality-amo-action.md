# Certified n-ary equality AMO action

## Result

`NaryCspEqualityAmo` is a disabled-by-default learner-selectable action at
registry ID 44, StepId 33 and C v12 parameter tag 45.  It consumes one exact
retained ternary X17 owner only after all three aligned-domain equality
definitions have been published and remain complete and live.

For each pair of equality variables `e_a,e_b` and each value literal `v` in
their shared exact-one domain, the action emits the RUP helper
`(~e_a or ~e_b or ~v)`.  The authenticated domain cover then makes
`(~e_a or ~e_b)` RUP.  All three pairwise binaries enter the live solver and
the value helpers are reverse-deleted.  No variable is introduced and original
model projection is the identity.  A partial retained definition, changed X17
owner, stale premise, existing binary, unsupported proof mode or exhausted
bound declines before publication.

Typed replay and C configuration deep-copy the exact three domains.  Bounded
native discovery offers only owners with all three equality records; the
Kernel-owned session suppresses committed domain keys.  Raw feedback reports
attempt outcomes, recognition/proof work, proof additions/deletions/literals,
journal bytes, binary yield and session commits.  Propagation remains a
separate action and DefaultPolicy is unchanged.

## Validation

The final code series is `cee1456b3` plus fail-closed exact-definition check
`95a75ae19`; combined patch SHA-256 is
`430c6c312cd74fb53085d47011f0c0d5611db5e49533782051adfcdb083ceb8e`.
CPU-Q job `1751013` passed 8/8 focused ASan/UBSan tests covering the action,
model projection, stale/duplicate/session behavior, controller/native
production, replay, C v10/v11 preservation and v12 deep-copy/execution.

The retained DSR SHA-256 is `0cc5ad40...`.  Both forward and backward
`dsr-trim` verification succeeded, and pinned Trestle verified both resulting
LSR streams UNSAT.  The fixture CNF is `75c4df72...`, provenance is
`dabb75a1...`, forward LSR is `7824e9b5...`, and backward LSR is `e2ac1da1...`;
full hashes remain in the immutable job artifact manifest.

This establishes correctness and public reach, not a target recovery.
Follow-up CPU-Q array job `1751018` used source `0af5009ee` and the explicit
ordered SR/X17/equality startup prefix for a 30-second Rank401 comparison.
Both arms remained UNKNOWN.  The X20 arm applied two AMO actions, reduced
conflicts from 141,860 to 75,697 and reduced the raw DSR from 9,867,400 to
4,753,514 bytes.  It also selected eight rather than eleven later X17 prefix
actions because adding X20 changes the random controller's eligible-action
trajectory.  The result is therefore promising integrated behavior, not an
isolated causal estimate and not grounds for DefaultPolicy promotion.
Focused CPU-Q job `1751020` independently rebuilt the same immutable source
and passed `controller_native_producers` 1/1, including explicit X20 selection,
`all`, configuration fingerprinting and replay; the Debug binary SHA-256 is
`7f20f153...`.
Effective both-policy fuzz, full Debug and the four regression campaigns
remain open for the next coherent integration checkpoint.
