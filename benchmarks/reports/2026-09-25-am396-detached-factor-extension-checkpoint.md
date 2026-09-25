# AM396: Factor extension while search guidance is detached

Date: 2026-09-25. Source parent `83ee5124` (solver source AM393
`c58c1f68`). This is a causal kernel-support repair checkpoint, not
Claude-owned fuzz acceptance, full-suite acceptance, N1--N4 promotion, or a
performance result.

## Failure and general repair

AM393 repaired *first* late H attachment after Factor, and remapped a retained
compact image during H-detached compaction. The AM393 Opus owner's non-counted
smoke then reached a different lifecycle: attach H once, detach it, apply
Factor, and reattach. On first attachment `input_activation_order` is consumed,
leaving `compact_import_order` as the retained decision-population image. Factor
calls `kernel_new_extension_vars` while H is absent; the old `kernel_reserve_vars`
grew kernel variables but did not append the fresh extension to that retained
image. Reattachment preserved the stale image, so the extension was absent
from compact-order consumers despite being live and unassigned.

`kernel_new_extension_vars` now appends each newly created extension and its
inverse position only when H is absent **and** a compact image already exists.
When the image is empty, the existing first-late-attachment reconstruction from
stable external identities still owns seeding. When H is present, ordinary
`heuristics_reserve_vars` remains the sole append path. This is independent of
formula, benchmark identity, Factor grid shape, and extension count. It leaves
never-activated input variables excluded and preserves the kernel/policy
ownership boundary. No proof-rule guard, effect, proof delta, or default schedule
changed.

Two new causal Factor fixtures cover attach–detach–Factor–reattach and the same
path after compaction removes an unactivated input identity. Both check the
compact order, inverse positions, active population and stable-mode phase
snapshot. Linking the new test object against immutable AM393 parent Debug
objects in the AM394 v2 build aborts at the first new assertion
(`tests/test_factor.cpp`, missing extension in `compact_import_order`);
`test_factor-parent` SHA-256 is
`7a9f45261b951bd8af3b31cc494d0713f689f455f637cd97ff8e2368b9235344`.
The parent control is retained in
[AM396 run](../runs/20260925-am396-factor-detach/README.md); it did not
mutate the active AM394 v2 build.

## Bounded validation

Current `factor` CTest passes 1/1 in Release, Debug with rule journal/proof
shadow, and ASan+UBSan. Pinned Debug focused `proof`, `api_n1`, `online_api`,
`micro_proofs`, `factor`, `factor_n1`, and `condition` pass 7/7; the
`micro_proofs` suite independently checks its native proof routes against
frozen AM369 checkers. Documentation size, progress, rule/policy boundary, and
comment-hygiene checks have zero errors; the pre-existing documentation
capacity and uncited newest-tier-run warnings remain. The still-running AM394
full Debug job `1720222` uses the **old** AM393 source pin, not this repair.

The AM393 Opus campaign owns its original frozen source and is still
calibrating. Its non-counted smoke is discovery evidence, not an affected-scope
verdict. A new immutable-source Opus affected campaign and a current-source full
Debug suite are required before this repair can count toward combined N1/N2;
retained Rank/B+C/WV gates remain required for N3/N4. Separately, AM395
confirmed the current post-finalization wrong-SAT API bug, and H2 still fails
the historical no-option DSR route. No Tier-D comparison was submitted.
