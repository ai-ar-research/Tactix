# AM432: H2 proof-state boundary and Tier-D automatic-route audit

Date: 2026-09-25. Source/protocol audit only: no solver, checker, canonical
contract, or benchmark result was changed. H2 and N2 remain open; the Tier-D
comparison remains post-goal.

## H2: the no-option checker retains a different formula

AM423's current-source proof is byte-identical to its historical parent.
Frozen no-option `dsr-trim` rejects it backward and forward, while the same
binary with `--delete-implied-units` verifies through frozen Trestle in both
directions. The exact replay and hashes are in
`benchmarks/reports/2026-09-25-am423-current-h2-proof-contract.md`.

The locally modified checker source is explanatory, **not** a substitute for
that frozen-binary result. Its `delete_parsed_clause` ignores a deletion when
the matched clause is the current global-UP reason and the implied-unit option
is unset (`dsr-trim-sr/src/dsr-trim.c:1577-1592`). The backward reconstruction
likewise leaves an implied-unit reason watched instead of retiring it
(`:3815-3848`). Tactix's `Proof::del` and diagnostic population shadow retire
the literal-named copy unconditionally (`src/proof/proof.cpp:485-515`), after
`kernel_retire_root_reason` materializes a root unit for the kernel
(`src/kernel/kernel.hpp:2060-2107`). Thus the shadow is a written-line
population check, not a model of the no-option checker's global-UP reason
population. A direct-SR witness valid for the kernel's reduced formula can be
invalid for the checker-retained stronger formula.

A general no-option repair must account for that checker-visible population at
each witnessed addition or produce an independent valid derivation against it.
AM390 already ruled out bulk unit publication and a fixed deletion reorder as
general solutions. Merely suppressing the public rule action or special-casing
H2 would not satisfy N1. The alternative `--delete-implied-units` contract is
not a monotone checker relaxation (AM404/407 controls); it needs an explicit
contract decision and broad original-CNF positive/negative certificate audit
before promotion. Until then, the historical no-option route remains canonical.

## Tier D: resource parity is known; proof-mode parity is not yet wired

The immutable first-winner manifest SHA-256 is
`b865472054bb6661d4b1472a3a8cbbbf53947004f61cdf35711da7ad3146a25b`.
It owns 1,166 unique SC24--26 rows, `cpu-dense-hwulab-q`, four CPUs per task,
32 GiB, a 5,000-second solve limit, and a 45,000-second checker limit. Its
historical 23-task throttle is documented as a certificate-scratch bound, but
plan §8.1 now requires an unthrottled full-width array with anonymous local
scratch; the old throttle is not part of the per-task resource limit.
`benchmarks/tiers/D.list` is not this frozen union.

`tools/bench/tactix_tierd.py:146-149` currently writes a wrapper that forces
`--drat` and pins only the DRAT checkers. Automatic Tactix output can instead
be a startup PBP or native binary DSR, so that wrapper is a compatibility arm,
not an unqualified measurement of the default solver. The existing
`tools/bench/runtime_task.py:229-239,284-388` supports `certificate=auto` and
content-routes PBP to VeriPB/CakePB, witnessed DSR to frozen no-option
`dsr-trim`/Trestle, and witness-free DSR to DRAT/CakeLPR. It uses validated
clause-preserving checker spelling and original-CNF certificate handling.
The older dedicated launcher must be adapted to that pinned route or replaced
by an equally validated full-union runtime-task campaign before submission.
The AM369 checker pins, not the currently mutable checker-bin symlinks, are
the DSR authority. No Tier-D job was submitted and no performance claim is
made here.
