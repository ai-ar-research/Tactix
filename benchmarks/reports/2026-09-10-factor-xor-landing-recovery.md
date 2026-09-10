# Factor/XOR checkpoint: patch-publication recovery

Landing6941 staged only the paths in frozen audit6940, then stopped at the
unchanged `git diff --cached --check` gate. The emitted6969 compile-scope.patch
ends with a normal unified-diff context line containing one space; Git correctly
reports that patch-file line24 as trailing whitespace and a new blank EOF line.
No commit was created, and no source or measured result changed.

Recovery6972 preserves the raw patch unchanged and hash-bound locally. It emits
a separate zero-context replay patch from the same hash-bound source pair and
verifies it with `git apply --check --unidiff-zero` against actual6730. The new
artifact is staged; the raw context-bearing artifact is not. No check is waived,
no frozen artifact is rewritten, and no working file is deleted.

Fresh audit6973 carries6940's immutable evidence, reports and campaign bindings,
refreshes published check states, and binds the recovery. Fresh landing6974
allows only the already-staged audited session subset, adds the newly audited
paths and retains the whitespace gate. No denied push retry or production change.

The scientific decisions, tests, cleanup and local Andrew TeX/PDF deliverable
remain exactly those of [the Factor/XOR report](2026-09-10-factor-xor-cost.md).
This is a publication-format repair, not a solver change; existing documentation
checks plus exact-source replay validation cover it. No additional solver test
or benchmark run is warranted. PROGRESS owns the latest landing and next action.
