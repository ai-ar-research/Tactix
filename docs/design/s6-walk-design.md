# S6 Walk — frozen compact design record

**Status:** implemented and scheduled ON. Walk is an H-only policy operation,
not a proof rule.

## 1. Contract

Walk searches for a complete assignment used to refresh policy phase state. It
must not mutate the clause formula, proof state, reconstruction stack, or public
rule surface. DefaultPolicy schedules it; the kernel merely provides neutral
formula/search services.

## 4. Durable reference choices and deviations

The implementation follows the reference shape for assignment initialization,
break-value updates, bounded local flips, and phase export. Before every walk,
the policy now runs Kissat's default root decision/propagation warm-up and saves
its assignment-time phases. The rotation remains `B+W`: on `tr-n280-s052`, a
fresh pure-`W` rerun regressed 12.6 s to 15.7 s and exposed a walker-output
phase mismatch immediately after the slot. Historical pre-implementation
status, the proposed separate Track W, and contradictory “removed” language
are retired.

## 6. Dosing record

The former `TACTIX_WALK_WARMUP` arm only warmed the walker's private assignment;
it was not the reference mechanism and has been removed. Current knobs exist
only where they retain a distinct diagnostic or scheduling use; neutral or
superseded arms are removed under the project's arm-frugality policy. Current
defaults, counters, and measurement evidence are owned by
`docs/implementation-notes.md` and dated reports.

Validation remains formula/proof identity, deterministic Walk counters and
phase output, configuration wiring, and the inventory boundary that classifies
Walk as an H-only operation.
