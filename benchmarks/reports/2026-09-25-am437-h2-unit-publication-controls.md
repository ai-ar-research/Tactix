# AM437: one-reason H2 publication/deletion controls

Date: 2026-09-25. Checker-only experiment on AM436's 16-clause original CNF
and 314-byte native DSR. No solver, proof emitter, checker binary, canonical
DSR option, or public action changed. The ASCII baseline is an exact
line-for-line re-encoding of the AM436 binary proof and reproduces its
frozen no-option rejection in both directions. Exact proof hashes, checker
logs, partial LSRs, commands, and pinned input/checker hashes are in
`benchmarks/probes/am437-h2-unit-publication/` (`probe.py`, `result.json`,
`artifacts.tar.gz`).

The test targeted the only checker-retained implied-unit reason in AM436:
input clause 15 `-18 19`, whose deletion follows explicit unit `17`.
Nine variants tested: baseline; omitting that deletion; publishing RUP unit
`19` alone or units `18,19` after `17` or immediately before the deletion;
publishing `19` while omitting the deletion; and repeating the deletion
after publishing either set of units. Every variant still rejects with the
frozen no-option checker. In particular:

| Variant | Backward | Forward |
| --- | --- | --- |
| ASCII baseline or omitted deletion | Watch-pointer failure at addition line 21 | No UP contradiction for RAT clause 15 at line 12 |
| Add `19` before deletion | Watch-pointer failure moves to addition line 22 | Reduced clause 18 claims contradiction at line 13 |
| Add `18,19` before deletion | Same backward failure | Reduced clause 19 claims contradiction at line 13 |
| Add `19` but omit deletion | Same backward failure | No UP contradiction for RAT clause 15 at line 13 |
| Publish `19` then repeat deletion | No matching clause at backward line 1 | No matching clause at forward line 3 |

The repeated-deletion result indicates that publishing `19` can make the
first deletion effective in this particular checker state; the second
deletion then has no matching clause. It is an inference from the paired
checker outcomes, not a claim about all deletion orders. The later SR steps
still fail because the explicit unit itself remains checker-visible and
creates a different reduction obligation. Omitting the original deletion
leaves the same baseline failure. Thus a single root-unit publication,
complete local closure, omission, or repeated deletion is **not** a general
no-option repair. This agrees with AM390's broader original-H2 controls.

The local `dsr-trim-sr` source explains why: no-option checking protects an
implied-unit reason in both forward deletion and backward watch-population
reconstruction; true-unit deletions are also ignored by default. The frozen
AM369 binary and preserved logs, not this locally modified source checkout,
are the result authority. The `--delete-implied-units` diagnostic contract
continues to verify AM436 in both directions through Trestle (AM436), but
AM404/407 show that option is not a monotone relaxation on other real
proofs. A choice of canonical checker contract remains material.

Next: under no-option checking, design a general proof representation or
checker-visible state model that can validate and emit each SR action
against all retained reasons; this may require an alternative derivation
when the requested witness conflicts with that population. Do not mask a
public action, special-case this CNF, or claim N1/N2. If the user permits a
fully audited option contract instead, all producers, negative controls,
Trestle checks, fuzz and cumulative gates must be revalidated. Full Debug,
AM434 AES extended checking, N3/N4, online-learning refactors and Tier D
remain separate and open.
