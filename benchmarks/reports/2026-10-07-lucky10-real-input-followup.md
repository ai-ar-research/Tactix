# Lucky10 real-input reach follow-up

## Correction to the bounded overfit audit

The frozen [LymphoSAT-derived mechanism audit](2026-10-07-lymphosat-overfit-audit.md)
conflates two balanced-signature recognizers. Its statement that the direct
balanced-signature image covers no real Oddball front end is correct for the
proof-oriented semantic recognizer, but it is not a correct statement about
production Lucky10 reach. Consequently, the audit's recommendation to treat
Lucky10 as a Default-removal candidate is not supported by the retained
evidence.

The proof-oriented recognizer under
[`balanced_signature_recognition.hpp`](../../src/rules/semantic_extension/balanced_signature_recognition.hpp)
requires a complete direct image: selector covers, state pairs, definitions,
AMO clauses, and every forbidden tuple, with source provenance. The production
constructor under
[`balanced_signature_model.cpp`](../../src/rules/lucky/balanced_signature_model.cpp)
instead reconstructs the LymphoSAT owner layout from external variable blocks
and root assignments. For TTF, it additionally observes one live binary or
root-false guarded signed-row pair for every ball. It then searches the
reconstructed template and accepts SAT only after completion and whole-live-CNF
model validation. These are different admission and result contracts.

## Invocation and composition path

Lucky10 runs once during initial Default startup. The current policy first
performs startup propagation, then the affine root opportunity and other
terminal constructors, invokes Lucky10, and only later enters structured-root
processing and ordinary Lucky/search
([`default_policy.cpp`](../../src/policies/default_policy.cpp)). The historical
forced-TTF checkpoint used the same startup position. Therefore its observed
reach is not caused by preprocessing later exposing a direct encoding.

The real-input bridge is a layout/template reconstruction:

1. `infer_shape` derives candidate ball count and width from the imported
   diagonal root assignments and validates the expected pair/signature block
   boundary.
2. `detect_forced_ttf` scans current live binary and irredundant clauses. A TTF
   row is forced only when the two orientations of row `b+1` for ball `b`
   occur as a binary pair or under a guard already false at root.
3. TTO rows use the full row-and-sign neighborhood. Forced TTF rows fix the
   row and search signs only; a cheap coordinate-parity check declines
   impossible fixed-row systems before any restart.
4. A partial construction is only a heuristic phase hint. A terminal SAT
   result requires `complete_assignment`, `lucky_model_valid` over the entire
   live CNF, and model publication.

This is structural within the normalized Oddball owner layout, but it is not a
complete source-authenticated composition of the factored Tseitin/cardinality
front end. Whole-CNF validation carries terminal soundness. The heuristic hint
path makes no logical claim. The external-layout dependency and incomplete
source classification remain legitimate generality and false-admission review
items.

## Retained real-input evidence

The immutable forced-TTF campaign
[`20261005-forced-ttf-oddball21-cpuq`](../runs/20261005-forced-ttf-oddball21-cpuq/manifest.json)
binds source `64de46fe9bf70ffe96fa5cebf352226c8f653cdc`, Release binary
`37d32594435cf09ca6f6ef58c6931bfdba57e5d845e4b65784c7ecfe1e696969`,
and all 21 saved Tier-D Oddball inputs. Job `1749458` produced 21/21 valid
original-CNF outcomes.

| Real-input class | Rows | Observed Lucky10 behavior |
|---|---:|---|
| TTO SAT, width 5, 51--80 balls | 10 | All ten were recognized and terminated SAT before CDCL. Every row used one restart, 8--225 signature steps, three completion rounds, zero decisions, and zero conflicts. |
| TTF UNSAT, widths 4--5, 13--29 balls | 10 | All ten were recognized as the forced-row layout. The fixed rows failed the coordinate-parity precheck, so Lucky10 used zero restarts and steps, published no candidate, and left the proof-producing UNSAT search to CDCL. All ten final proofs were independently accepted. |
| `112_5_ttf` SAT | 1 | Forced-row sign search terminated SAT in one restart, 34 signature steps and three completion rounds, with zero decisions and conflicts. The original-CNF model checked. |

The focused `112_5_ttf` campaign records 2.570 seconds wall time, compared with
181.942 seconds for the saved no-MAB Tactix run and 1.970 seconds for saved
LymphoSAT. Its repeated row in job `1749458` took 2.270 seconds. The older
unfixed constructor recognized this same family but searched selectable rows;
the retained no-seed run took 201.570 seconds on `112_5_ttf`. The restored
forced-row path is therefore a real terminal constructor benefit rather than
later guidance consumption.

The older 21-row campaigns further separate terminal construction from
guidance. Before the forced-row repair, all 21 rows reached Lucky10. The ten TTO
rows already terminated inside the constructor. The eleven TTF rows produced
partial candidates, and their downstream seeded-Walk effects were mixed and
sometimes proof-expensive. The forced-row repair retained the ten terminal TTO
models, made `112_5_ttf` terminal, and removed candidate publication from the
ten parity-impossible UNSAT rows. Its full campaign remained 21/21 valid. The
21-row aggregate changed from 5,711.837 seconds in the no-seed checkpoint to
4,893.444 seconds at `64de46fe9`; cluster timing and intervening documentation
commits make the per-row mechanics stronger evidence than this aggregate.

Telemetry at `64de46fe9` predates the printed `forced TTF` counter. The source
branch and zero-restart TTF records identify the behavior, and the successor
counter commit `a20223c33` makes it explicit. A later retained imported-root
pilot independently reports forced-TTF recognition on `26_5_ttf`, `19_4_ttf`,
and `112_5_ttf`; that pilot failed before its intended Walk comparison, so it
is reach evidence only, not performance or acceptance evidence
([terminal report](2026-10-07-walk-imported-root-admission-terminal.md)).

Focused synthetic coverage is broader than a happy-path fixture. The forced-TTF
generator varies binary and guarded row pairs, dimensions, clause order and
mixed components, and includes missing-row, wrong-row, invalid-guard,
typed-budget, SAT validation-poison, and disjoint-UNSAT controls
([`forced_ttf.py`](../../tools/fuzz/forced_ttf.py)). It checks original-CNF
models or proofs and deterministic controller replay. This validates the
implemented recognition boundary; it does not turn the one generator family
into evidence for arbitrary encodings.

## Revised disposition

**Keep Lucky10 and its current Default startup opportunity.** The retained
positive cohort contains eleven real terminal beneficiaries over varied
Oddball sizes, more direct evidence than the six-row MDP constructor recovery.
Removing it based on zero reach of the separate exact semantic recognizer would
discard a legitimate encoding algorithm and regress a checked SAT recovery.

Keep two reviews open without blocking that disposition:

- strengthen or more explicitly name the layout-based admission contract, and
  measure false-recognition cost on varied negative formulas; and
- review the nominal 160 x 140,000 x 36 TTO and effective Default
  160 x 70,000 x 24 forced-TTF ceilings (the typed forced-TTF cap is 240
  restarts). The retained positives finish in one restart and at most 225 steps,
  while impossible forced rows decline before search, so the large theoretical
  ceiling is not evidence of observed target overwork. It still needs a
  source-derived explanation or a bounded dose study before any increase.

This correction does not promote partial Lucky10 hints as theory facts, claim
an authenticated factored-front-end representation, or infer generality beyond
the observed Oddball encoding family. It changes the production decision from
blind removal to preservation with a narrower admission-and-cost review.
