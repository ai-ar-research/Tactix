# BVE fixture repair and direct debugging ownership

The user instructed Codex to stop Claude and debug directly, then made that
ownership permanent. DEVELOPMENT_PLAN §8.2 owns the new policy; mandatory
Claude debugging/review is removed, but Claude-owned fuzz execution remains.
The approved7096 process was interrupted and its launcher completed with an
`aborted_streaming` result, not a completed review or a correctness pass.

## Reproduced failures and fixes

- **Search invariant after model extension:** the original fully instrumented
  Debug fixture again aborts at `assigned_count + k.unassigned == k.nvars`.
  `kernel_extend_model` intentionally fills retired/unassigned model values
  without changing search-trail accounting. The test now checks search
  invariants before completion, then checks complete opposite-polarity values,
  external-model agreement, unchanged trail/counter, original-clause
  satisfaction and idempotent reconstruction. The invariant is not weakened.
- **Missing DRAT units:** the old proof fixture called `add_derived_unit` for
  manually derived RUP units, although that helper intentionally emits only
  LRAT for propagation-derived units. Successor7101 calls `add_rup` with the
  same literal and two-clause hint chain. No proof serializer change.
- **Negative-control syntax:** the old helper emitted a literal backslash-n
  rather than a newline in corrupted proofs. Successor7101 emits valid line
  delimiters, so rejection exercises a false empty lemma rather than that
  malformed delimiter. Original inputs have no unit-propagation refutation.

## Evidence and boundaries

[7100](../runs/20260910-7100-bve-fixture-repair/summary.json) links the corrected
fixture against the original fully ASan/UBSan-instrumented7087 Debug objects.
The original fixture abort is reproduced; the corrected view, full BVE and
BVE-memory tests pass. Leak detection remains enabled, with sanitizer failures
fatal. Running outside the ptrace sandbox resolves the earlier LeakSanitizer
environment failure; no leak check was disabled. The original7090 failure is
preserved, not rewritten into a pass.

[7101](../runs/20260910-7101-bve-view-proofs/summary.json) validates96 positive
proofs (48 DRAT,48 cake_lpr),96 corrupted-first-lemma rejections and two
known-bad setup rejections. Parent7030/candidate7087 proof bytes and fixture
outputs match exactly. Root padding, duplicates, literal order, dense/sparse
public BVE and forced arena growth remain covered. Both arms use their original
immutable Release objects; no solver is rebuilt or changed.

Only the new7100 review copy's test differs from7087. Original source, pins,
failed fixtures, logs and frozen reports are retained. The test-only patch and
hash-bound commands/logs are preserved with7100; current source fuzz is still
required before adoption. This closes the two fixture failures and sanitizer
environment issue, not the prototype's B/C, repeated runtime, full50 or fuzz
gates. No new solver jobs, production changes, promotion or material deletion.

## Next action and test consideration

Resume B28+C5 with the corrected7100/7101 gate dependencies, then repeated
Rank31/35/38 exact-work/proof and RSS controls. Continue the approved BVE
data-path prototype only from valid controls; no speedup is claimed here.
The checked7030 full50 score stays1.123176512x Kissat; the remaining reduction
to1.01x is819.2859 seconds. No reference rerun or timing change.

The fixture change has explicit new reconstruction regressions and an actual
before/after Debug control. The proof helper has positive and negative oracle
controls for both formats. The ownership-only plan edit changes no solver
behavior; documentation size, references, boundary, strategy and option checks
plus the existing benchmark/SLURM tool suites cover its coupled handoff.
