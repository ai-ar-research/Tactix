# Residual handoff ordering audit

The measured Rank401 improvement is consistent with a search-order change, not an expensive symmetry pass or destroyed saved phases. The enabled handoff runs only while `k.h` is absent. Both arms then construct fresh saved/target/best arrays initialized to zero. Early Lucky suppresses phase saving. No pre-existing H phase state is overwritten by this path.

The handoff replaces parser activation order, hence VMTF order/stamps and initial EVSIDS scores; it then replaces the compact import permutation used by random decisions and several scheduling orders. Permanent first-import ranks remain unchanged and still govern heap insertion/Reorder tie order. This is a hybrid heuristic, not a complete fresh solver import.

Two deviations from the stated fresh-reparse equivalence are proven:

1. Binary direction is reconstructed from old activation ranks. Original `(3 4 5), (1 3)`, followed by root4=true, leaves serialized residual `(1 3), (4)`. Fresh parsing activates `[1,3,4]`; reconstruction yields `[3,1,4]`. The preferred next variable changes from3 to1. The four-case probe checks both prefix orders and both binary orientations; two mismatch, with all phases zero.
2. Native DSR and DRAT have already discarded binary proof-ID storage at parser header/import completion. `BinaryIdMap::get` therefore returns sentinel1 for every binary. Residual reconstruction nevertheless treats that value as the original clause ordering key. On original clauses `(4 5 8), (6 7 8), (1 3)`, then root4=true, fresh parsing yields `[6,7,8,1,3,4]`. DSR/DRAT instead yield `[1,3,6,7,8,4]`; LRAT retains source ID3 and matches fresh parsing. This affects both activation and compact import order. It is not a logical proof defect.

Debug job1754437 passes the deterministic non-solver assertions, including effective native DSR/DRAT/LRAT setup. Logs and identities are retained beside this report. Earlier job1754354 passed the four orientation cases;1754378 compiled but a wrapper hashing typo stopped it before execution. The authoritative extended run is1754437.

## Reference and implementation provenance

Kissat `import.c:adjust_exports_for_external_literal` assigns internal order on first raw import; `internal.c:kissat_add` and `flags.c:activate_literal` activate retained clause literals in their actual presentation order, assigning queue stamps and scores. Ordinary compaction transports saved/target/best phases (`compact.c`) rather than reseeding them. CaDiCaL's `compact.cpp` likewise maps phase arrays.

The locally retained Satsuma1.3 source (`tools/solvers/variants/satsuma-sr82-generator-detail-trace/src/satsuma.h`) transfers with `keep_original_order=true`, outputs formula clauses, then symmetry-breaking clauses, then fixed units. Its process boundary really does give a fresh Kissat import a new order. Tactix's binary orientation inference cannot reconstruct that information once discarded, and the global fixed-unit suffix also does not reproduce all possible iteration-grouped suffix orders.

Commit f50e46c0c introduced the conditional handoff deliberately, after the DRAT binary archive removal already existed in a646ca611/107c461b6. The handoff test explicitly codifies prior-rank binary orientation. AM149's manifest cites empirical trajectory preservation on selected controls; the ledger describes complete Rank/B28+C5 acceptance as then open. The conditional helper/graph-root admission was therefore an intentional heuristic, not a new phase-preservation repair. The claimed exact reparse is stronger than the implementation supports. Correcting these order differences changes a historically measured heuristic and still needs causal/regression qualification.

Existing component evidence must not be repeated or treated as a winner: Rank401 activation-only timed out300s, import-only solved214.471s; Rank16 activation-only299.832s, import-only timed out300s; Rank23 activation-only59.035s, import-only246.162s. Both-original wins Rank401 around43–45s while both-residual helps some controls. No component universally dominates.

## Next diagnostic, not a production promotion

A bounded prototype (job1754450) captures presentation creation ordinals separately from proof IDs, preserving oriented external binary endpoints. It arms only inside the existing4096-variable/262144-input-clause admission envelope, caps retained records at262144, and declines the whole exact handoff if metadata is incomplete. It never restores the LRAT proof-ID archive. On a complete capture it defines a concrete residual serializer: live clause creation order, current large-clause literal order, retained binary orientation, then external-order fixed units. Small fixtures must match fresh parsing across proof modes before any timing. This is exact for that declared serialization, not general Satsuma process parity.

Matched prototype/control Rank401/16/23 runs should retain full-CNF model checks, record complete/unsupported status, and distinguish activation/import changes from proof work. Only a favorable general result warrants designing persistent provenance machinery. An original-seed default remains a separate option if its full Rank gate passes. No family/instance dispatch or production policy change is justified by this report alone.
