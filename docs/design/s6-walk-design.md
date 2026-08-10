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
break-value updates, bounded local flips, and phase export. The retained
warm-up/rotation behavior is a policy choice with an explicit configuration
surface. Historical pre-implementation status, the proposed separate Track W,
and contradictory “removed” language are retired.

## 6. Dosing record

Measured warm-up and limit variants did not justify accumulating additional
permanent arms. Current knobs exist only where they retain a distinct diagnostic
or scheduling use; neutral or superseded arms are removed under the project's
arm-frugality policy. Current defaults, counters, and measurement evidence are
owned by `docs/implementation-notes.md` and dated reports.

Validation remains formula/proof identity, deterministic Walk counters and
phase output, configuration wiring, and the inventory boundary that classifies
Walk as an H-only operation.
