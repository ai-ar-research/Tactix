# Exact declared serializer: negative controls

The diagnostic-only exact-order prototype does not justify promotion. Both Rank16 and Rank23 report complete metadata capture, yet regress against the same-source legacy handoff on matched sequential CPU pairs:

| Instance | Legacy control | Exact declared serializer |
| --- | --- | --- |
| Rank16 | SAT128.791s, independent model accepted | SAT520.735s, independent model accepted |
| Rank23 | SAT150.791s, independent model accepted | UNKNOWN at600.020s |

Complete captures contained8952 and9044creation records respectively. This isolates a substantial trajectory cost from the proposed ordering change; it does not establish which ordering component causes the regression. The prototype defines its own serializer, not actual Satsuma output, so it cannot decide Satsuma+Kissat parity.

The initial Rank401 pair on node013 aborted with SIGABRT at104.024s and68.284s, without footer or captured errno. A subsequent audit found the filesystem full due443GB of abandoned telemetry, now removed under a separately recorded cleanup. These are failed environmental-suspect runs, never solver timing conclusions. The replacement shared-scratch pair1754534 was canceled by the coordinator because the negative controls and new user priority made it unnecessary. Partial campaign evidence remains preserved.

The next diagnostic uses the archived Satsuma binary's actual `--out-file` residual and the bundled Kissat4.0.4 source/binary, retaining real external-ID import/activation/decision order. No production order metadata or policy changes were made.
