# Symmetry singleton-unit admission checkpoint

This checkpoint implements the user-directed rule that a valid, fully checked
symmetry-breaking unit is never withheld by cut-admission policy. It is a
reviewable development checkpoint, not an accepted solver candidate.

## Change

- Large-domain generator fixing publishes each checked unit directly. It no
  longer buffers a pass to discard a singleton under
  `RequireCompoundProgress`.
- FixAndCut always treats its clausal-fixing and negation-orbit units as
  forcing. It also derives forcing from the live root meaning of a validated
  binary, so a binary with one root-false and one open literal cannot be
  mislabeled nonforcing.
- `RequireCompoundProgress` retains its serialized value and continues to
  withhold a lone binary that remains nonforcing. `RequireForcingMember`
  continues to reject an all-nonforcing batch. Public numeric/API values and
  replay formats are unchanged.
- Existing X7 validation, exact automorphism and chain checks, root closure,
  work budgets, and application-time stale/inapplicable rejection are
  unchanged. The historical singleton-decline counters remain present for
  compatibility and are now producer-zero.

`tests/test_online_inprocess.cpp` covers both former suppression sites under
the compound setting: a one-generator large-domain pass and a one-row
FixAndCut unit are enumerated, applied through the public X7 path, and observed
at root. Existing tests continue to require that a complete all-nonforcing
batch is declined by `RequireForcingMember`, while a multirow nonforcing batch
is admitted by compound progress. A proof fixture emits the singleton through
the same public application path for forward every-lemma dsr-trim/Trestle
checking, with a non-UP invalid witnessed-unit negative control.

## Frozen attempts and current authority

The exact current prepared authority is
`benchmarks/runs/20261007-symmetry-singleton-unit-admission-r4`:

- base commit `06b63069bd122994db8c3046a84ad4db731d205a`;
- exact source archive SHA-256
  `1249dc7f7b5bd8c3a401d944fa0d74a9aaed080c64e46f8fee685c5a70ae1eee`;
- wrapper SHA-256
  `1a1a42367b0b8c928b9f1c2329a8950e0939c47663b3492c72140dd82a787e9e`.

It is prepared but deliberately **not submitted** because the user requested a
natural stopping point and root asked to stop repeated full builds. The prior
attempts are preserved as negative harness evidence:

- `1755490` was canceled after review found that its wrapper required the
  positive representative spelling, while FixAndCut validly selects the
  negative representative.
- `1755498_0` reached the new positive unit tests, then failed a new synthetic
  nonforcing control because the test required `Complete` at an intentional
  one-instance cap. `1755498_1` was canceled as superseded.
- `1755527_0` confirmed that relaxing the status alone was insufficient: a cap
  on an auto-discovered multirow chain does not guarantee one completed
  nonforcing row. That invalid synthetic control was removed; the pre-existing
  exact nonforcing-batch controls remain. `1755527_1` was canceled.

No production assertion or proof rejection was observed. These attempts do
not constitute a green focused gate: neither reached the standalone proof
fixture, and no sanitizer task completed. Dependency-held clean Release and
full Debug jobs `1755536`/`1755537` were canceled before start and produced no
pin.

## Remaining gate

Run the frozen r4 array unchanged. Acceptance requires all five focused
binaries in Debug and ASan/UBSan, the produced compound singleton DSR accepted
with forward every-lemma dsr-trim/Trestle, the invalid witnessed-unit control
rejected, and all documentation checks green. Then freeze fresh Release and
full Debug pins from that exact source. Rank899's separate handoff must also
prove that the concrete large-domain producer publishes `+40310`, independently
verify its retained proof prefix, observe root growth with initial FastEliminate
suppressed, and compare the complete residual ordering/activation queue before
any causal timing claim.
