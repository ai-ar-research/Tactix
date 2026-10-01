# Controller checker-pin repair checkpoint

## Result and scope

The clean DSR checker built from the pinned upstream plus exactly the maintained
patch verifies the bounded F2w witnessed-proof reproducer, the earlier
witness-free F2 proof and the bitmask-growth fixture through canonical and
forward/every-lemma DSR/Trestle checks. It is byte-identical to the canonical
AM549 benchmark checker. This repairs the selected oracle build; it does not
certify a complete fuzz campaign.

Claude's [bounded handoff](../runs/20261001-controller-fuzz-owner-12e3c4f0/FAILURE_HANDOFF.md)
reports nine DefaultPolicy witnessed-proof audit failures on the old harness
checker and verification of all nine on the canonical checker. Codex directly
checked the one handed-off 35-variable/53-clause case, not the private campaign.
The owner must rerun affected acceptance with the verified pin and deliver a
passing terminal report. Historical failures remain failures.

## Exact provenance

- Upstream: `0b8cc0ef4d228c059994923cbc66abeb9a42ea14`.
- Maintained patch: `tools/checkers/patches/dsr-trim-0b8cc0e-forward-elaboration.patch`,
  SHA256 `edd70e504e573fbf9801a1c9f80a05fae04e2bcadd763a1e2a1b2b923df3afc6`.
- Build source: `tools/solvers/variants/dsr-trim-controller-oracle/BUILD_SOURCE.json`;
  compiler GCC 13.1.0, upstream git archive, `patch --batch --forward -p1`
  after a successful dry-run, then `make -j8`.
- Source hashes: bitmask.c `f26eacf79dab63b2f13bf3a1df7723438c27612433a0aa4ef378b5b3c49c591d`;
  dsr-trim.c `03fd2e875945edb1374032ef7da3427c36203a88c20fcb693054425b598266ff`.
- Frozen checker: `build/pin-dsr-checker-controller-clean-patch/dsr-trim`,
  SHA256 `3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee`;
  adjacent `PIN.json` binds source, patch and verification evidence.
- Same executable hash as `20260928-am549-no-mab-hybrid-wv100/pins/dsr-trim`.
  The clean build retains the bitmask-growth repair; the canonical executable
  must not be described as unpatched merely because its source was previously
  uncertain.

The current root reference clone has additional sparse dependency/hint
optimizations: its diff is 128 insertions/four deletions, versus the maintained
patch's one bitmask insertion and forward-elaboration changes. Old global
checker SHA256 `028240565e37185ce6064fe0f044d28ed70fb8b8823e03fdb2cc02988cbacc74`
is therefore not attributable to the maintained patch alone. Attribution to a
specific optimization remains unproven; selecting the exact maintained build
avoids those additional changes without dropping any required oracle.
The root clone and shared checker symlinks were preserved for active jobs.

## Direct controls and limits

[Positive evidence](../probes/scratch/controller-checker-correct-patch/result.json)
binds the three exact CNF/proof pairs and the clean checker provenance. All
three passed backward/core-trimmed and forward/every-lemma DSR elaboration,
expansion and Trestle, with a 30-second local fixed-proof allowance.

[Negative evidence](../probes/scratch/controller-checker-correct-patch/negative-controls.json)
shows an unused-invalid-lemma proof accepted by backward/core trimming but
rejected by the clean forward checker at its first addition. Trestle rejects a
truncated witnessed LSR. Neither timeout nor generic error was counted as a
successful negative control.

An initial isolated archive build omitted the patch: nested `git apply`
returned success while skipping upstream paths. Its SIGSEGV results are
unpatched-build evidence, not evidence against the maintained patch. The
original binary/metadata and explicit correction are preserved under
`controller-checker-clean-build/unpatched-build/PROVENANCE_CORRECTION.md`.
The corrected build uses an application mechanism resolving paths within the
archive and records the verified patched source hashes.

[Codex decision](../runs/20261001-controller-fuzz-owner-12e3c4f0/CODEX_F2W_DECISION.md)
supplies the owner with the immutable pin, unchanged acceptance obligations,
and precise historical/repaired controls. No solver change or local solver
experiment was used for this diagnosis.
