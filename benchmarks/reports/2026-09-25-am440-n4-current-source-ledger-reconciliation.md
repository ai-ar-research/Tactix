# AM440: three high-severity N4 ledger rows were stale

Date: 2026-09-25. Current-source/code/test reconciliation only. No solver,
policy, rule, proof, checker, benchmark pin, or acceptance gate changed. The
September 7 isolated reports are frozen historical evidence, not present-tense
production status.

| N4 row | Shipped code evidence | Current-source focused evidence | Remaining gate |
|---|---|---|---|
| `SP-CONG-PREFIX-1` | `src/rules/congruence/congruence.cpp` scans live clauses through the last irredundant logical position, including interleaved redundant records. `git blame` places this in `7a668e76`. | `tests/test_congruence.cpp::test_forward_includes_only_redundant_physical_prefix`; current-source Debug job 1723391 test 52 `congruence` passed. | Affecting fuzz, exact later trajectory and checked cumulative runtime. |
| `SP-REDUCE-ROOT-GUIDANCE-1` | `default_prepare_reduction` counts roots created by `reduce_prepare` and consumes exactly the appended suffix into `H`; `DefaultPolicy::solve` calls it before victim selection. Both are in `7a668e76`. | `tests/test_reduce_root_guidance.cpp` compares policy and bare preparation, checks heap activity and original SAT model; job 1723391 test 156 passed. | Fuzz, reference runtime attribution and checked cumulative gates. The historical isolated 5900 arm's full-set slowdown is not current-source acceptance. |
| `SP-VIVIFY-RETRY-1` | Learned-large irredundant Vivify marks surviving old variables for BVE retry; the shared replacement handles removed literals, while instantiated cases retain their separate path. The loop is in `7a668e76`. | `tests/test_vivify.cpp` checks retry bits for irredundant/redundant and instantiation cases; job 1723391 test 41 passed, plus test 178 `vivify_binary_retry`. | Affected fuzz, full-trajectory comparison and checked cumulative gates. |

Before this audit, `docs/scheduling-parity-ledger.md` described each as **OPEN
in production** despite the implementation. `docs/implementation-notes.md`
also described Congruence as skipping redundant prefix clauses. Those living
claims now say **implemented in production; acceptance open**. The code/test
checks show behavior is reachable, not that N4 is fully satisfied. Full Debug
job 1723391 had passed 186/187 at this snapshot and was still running its
final scripted demo; neither that partial suite nor old traces support a
whole-policy N4 verdict. Paired AES checker jobs 1723443_0/1 were also active.
H2/N2, the general PB rule interface, winner gates, final learning readiness,
and the requested post-goal Tier-D run remain separate and open.

The ledger validation also exposed a tooling classification defect:
`check-config-wiring.py` failed on `Kernel::sweep_incomplete` as an unclassified
configuration field. Current source shows only Sweep's lifecycle state machine
sets/clears it (`sweep.cpp:1798,2000`); tests may seed it for a retained-pass
fixture, while policy never configures it. It now belongs to `NOT_CONFIG`,
with that reason recorded beside the entry. The checker moves from 97 fields,
one failure to 96 actual configuration fields, zero failures, without adding
a fake policy writer or changing Sweep behavior.
