# AM373: X5 witness identities survive variable compaction

Date: 2026-09-24. Parent `983ee7a9`. This is a general N1 public-action repair and N2 model-reconstruction checkpoint, not combined N1/N2 acceptance or Tier D.

## Finding and repair

X5 permits a finite cube over the existing variable universe, including a variable outside the selected clause. A completed elimination or fixed-root archive can remove such an identity from the current internal map without removing it from the original model's external universe. `tactix_apply_condition` previously internalized every witness literal and rejected the tuple if any witness variable lacked a current internal identity. Even an active witness literal could be interpreted as the wrong variable by a direct instance after compaction. The exact compacted fixture failed on parent source at `condition_apply_explicit` before the repair.

`ConditionInstance` now keeps candidate clauses in current internal coordinates and witness cubes in stable external coordinates. The guard checks duplicate external variables, the existing external universe, archived root values, and current root values; it maps only live witness identities into the local clause-preservation implication. An unmapped witness variable cannot occur in a live clause, so it has no local preservation obligation. The tagged reconstruction segment and rule journal retain the complete external cube, including holes, without remapping through a missing identity. This changes no proof lemma or deletion vocabulary and introduces no benchmark, variable-number, size, or producer special case. Root/quiescence and live-candidate boundaries remain.

## Fixed evidence

- `test_condition` retires and compacts external variable 3 while external variables 4 and 5 renumber internally. A legal cube `{1,3,4,5}` applies to live `(1∨2)`, and reverse reconstruction produces a total model satisfying `(1∨2)`, `(¬1∨4)`, `(¬4∨5)`, and the original tautological mention of 3. Duplicate and out-of-universe cube controls decline. A second fixture archives a positive root value on external 6: a cube containing `¬6` declines and one containing `6` applies. The parent fails the first compacted application.
- `condition_compacted_model_check.py` independently checks the emitted compacted model against `tests/condition_compacted_original.cnf` and rejects a corrupted model. Its CTest passes in Debug, Release, and ASan+UBSan. The public C fixture also checks a discarded-only input identity in a cube and the reconstructed SAT result.
- The existing 22-case public certificate/model oracle passed on the changed source in Debug, Release, and ASan+UBSan: 20 original-CNF UNSAT proofs across binary/ASCII DRAT, native LRAT, and binary/ASCII DSR; two original-CNF SAT models; all empty-proof and corrupted-model controls reject. The direct compacted fixture adds the new path's independent original-CNF model check. Debug's focused rule/API/journal/proof/online/doc set passed 11/11. `tools/fuzz/selftest.sh` passed its clean stand-ins, deliberately broken model/proof wrappers, native/conversion checks, and shrinker control. The documentation checks pass with six existing near-cap warnings and no errors.

## Limits and next action

These fixed paths do not prove universal N1/N2. A separate immutable-source Opus affected-scope campaign must vary compaction, archived roots, long mixed cubes, interleaved rules, and independently checked proof/model outputs in Release, Debug, and sanitizer modes. The concurrent AM372 owner tests `6fd0e384`, before this change, and its result cannot validate AM373 by transfer. X5 still lacks an automatic producer and typed online batching; the broader fuzz, N3/N4, Rank/B+C/WV and online-learning gates remain open. No Tier D job was submitted.
