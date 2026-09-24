# AM359 — H3 DRAT derivation audit (bounded classification)

Date: 2026-09-24. Current worktree commit: `f6545dbc934eedb3bcb49ac169da5d9fb4640903`; the checked proof artifacts are the earlier frozen AM349 parent/repaired pair, **not** fresh output from this commit. This read-only check does not alter the solver, close N1/N2, or authorize Tier D.

## Question and governing contract

AM349 H3 reports a SAT-input Congruence **action proof**, not an UNSAT certificate. A strict custom audit drops every deleted clause and then requires every later addition to be RUP from that live multiset. It fails at zero-based addition step 28, lemma `(3∨4∨-5)`, after deleting the binary reason `(-9∨-11)` of a root fact. The handoff itself says a root-persistent audit accepts and that the same behavior is present in the pre-repair base. Exact input, proofs and owner reproducer are under `benchmarks/runs/20260924-am349-unary-xor-claude-fuzz/handoffs/H3-drat-reason-deletion/`.

The project contract is more specific than that custom audit: `DEVELOPMENT_PLAN.md` §5 says DRAT retains established implicit-unit behavior, whereas DSR and LRAT must publish checker-visible root facts. The pinned `drat-trim` reference documents its treatment of unit propagation and unit-clause deletion (`drat-trim/README.md`, “Clause Addition Preconditions” and “Clause Deletion Details”). Thus strict live-multiset RUP is useful as a diagnostic, but it is **not** the acceptance oracle for explicit DRAT, and H3 alone is not evidence of an N2 violation.

## Independent bounded checks

- Pinned checker: `benchmarks/runs/20260924-am350-late-proof/pins/drat-trim`, SHA-256 `f2cb7d45d6acc05b1295bfa0d27b53d75fa48259c2b1e15f15772f064f99849a`.
- Original 11-variable/42-clause CNF SHA-256 `96d343d170aa9f3afd6677a3b9ef2336e29cf698c0d545c62513a300bab41c1c`. Parent and repaired action proofs are byte-identical, SHA-256 `5f1fd827581fc2c2dbf5357fb7870ff961911257f8512d36379fd2e52a77d655`.
- Independent custom strict audit: `FAIL step 28`, 19 additions/10 deletions reached. The same audit with persistent root consequences: `OK`, 33 additions/44 deletions. This reproduces the owner's classification but is not a checker acceptance result.
- Actual pinned checker on **each** saved action proof with `drat-trim <original.cnf> <action.drat> -f -U` reports `c VERIFIED derivation: all lemmas preserve satisfiability`, `0 RAT lemmas in core`, and `s DERIVATION`. Because the formula is SAT and the action ends without an empty clause, `DERIVATION` is the correct result, **not** `s VERIFIED` UNSAT. This checker exits 1 for derivations, so status text—not exit code alone—separates the grade. Both outputs are preserved under `benchmarks/probes/scratch/am359-h3-drat-audit/`.
- Negative control: the one-line `-11 0` addition against the same original CNF is not implied. The same `-f -U` invocation reports `RUP check failed` at proof line 1 and `s NOT VERIFIED` (also exit 1). This guards against mistaking any nonzero exit for an accepted derivation. The negative proof SHA-256 is `b9f9421bf2613d8bfebc8bdeb53b95afa72fe7cc969a4ba3114e4ba4d2130d51`.

`-S` was deliberately **not** used as acceptance evidence: inspection of the pinned checker's `FORWARD_SAT` path shows that mode validates deletion-side SAT behavior, not the additions at issue. The `-f -U` route is the relevant forward all-RUP derivation check.

## Disposition and limits

H3 is a mismatch between a stronger diagnostic and the explicit DRAT checker contract, not a demonstrated solver defect. No speculative unit-publication change is made to the DSR/LRAT or solver state. This closes only H3's bounded DRAT action-proof question; it does **not** prove that a later complete UNSAT certificate on this or another input will verify, and it does not waive any N2 proof/model oracle, fuzz verdict, or retained performance gate. Continue with the active AM349/AM351/AM352/AM355/AM358 Opus owners and debug any actual rejected certificate or invalid model generally.
