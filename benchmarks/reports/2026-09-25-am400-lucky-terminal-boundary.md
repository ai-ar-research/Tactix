# AM400: consume terminal Lucky results at both DefaultPolicy boundaries

Status: repair checkpoint, not combined N1/N2/N3/N4 acceptance. Source base
`32296504`; this report accompanies the policy fix, `lucky_terminal` regression,
and calculus/implementation-ledger synchronization. The fixed Debug diagnostic
binary copied to `benchmarks/runs/20260925-am400-lucky-terminal/pin/tactix`
has SHA-256 `42662d31adb0c244b7eecc1170ec68e42b1a89bd9ff4fa6e6ad44e2619c4c1be`.
The run-local script and stdout/stderr preserve every command and result.

## Failure and causal repair

AM397's terminal Opus handoff reports unfiltered rule journaling made a
20-variable UNSAT PHP instance return SAT with an original clause falsified;
the parent and AM397 candidate behaved identically. A second PHP input
aborted at a Debug root assertion. Its bounded traces end in Lucky's
`refuted` result, followed by a journaled Propagate attempt declined by the
`consistent` guard. The handoff is
`benchmarks/runs/20260925-am397-journal-atexit-claude-fuzz/handoffs/HANDOFF.md`.

`lucky_apply` legitimately returns `Unsat` after root-closure or failed-literal
refutation. Both `DefaultPolicy::solve` calls to `default_lucky` had checked
only `Sat`, so execution continued with `k.inconsistent` set. In diagnostic
builds the journal's registry guard then declined a later Propagate that the
unjournaled service would have used to notice the refutation. AM400 consumes
`Unsat` immediately at *both* Lucky call boundaries and returns 20 through
the normal policy finish path. The repair is by result contract, not by PHP
shape, proof format, journal setting, or a weakened guard.

The new fixed `test_lucky_terminal` constructs PHP(3,2), disables unrelated
structural shortcuts, and asserts an early Lucky UNSAT leaves cold state
deferred and guidance unattached. The old fallthrough would materialize that
state. It passes Debug with and without the journal and Release. The Debug
journal's last transaction is `Lucky early ... refuted`; no later rule is
offered. The existing larger `lucky` CTest was deliberately interrupted in
the local shell after more than ten minutes; it is not counted as a pass.

## Original-CNF proof evidence

Fixed SLURM array `1720723` ran the Debug journal build with native `--drat`
on the exact two AM397 handoff CNFs, with journaling on and off. All four
solver processes returned 20. For each formula the two proof files have
identical SHA-256 (PHP20 `6e3177c0...`, PHP21 `738a8935...`). The PHP20
journal now ends at `Lucky late ... refuted`; PHP21 ends at `Lucky early ...
refuted`. The first validation attempt incorrectly sent native DRAT to the
DSR content router and all four *checker* tasks failed as `invalid-dsr`;
no solver result was treated as verified on that attempt. Corrected pinned
DRAT→LRAT→CakeLPR checker array `1720744` completed 4/4 with exact success
statuses (`drat-trim` exit 0 / `s VERIFIED`; `cake_lpr` exit 0 /
`s VERIFIED UNSAT`) against each original CNF. Artifacts and scripts are in
`benchmarks/runs/20260925-am400-lucky-terminal/`.

The earlier frozen full Debug suites for AM394, AM396 and AM397 all ended
185/185 passed (SLURM jobs `1720222`, `1720277`, `1720299`). They predate
AM400 and do not substitute for a current-source suite. AM396's terminal
Opus report passes its affected Factor-detachment scope but hands off the
parent-identical explicit-API invariant H1; AM397 passes the atexit-lifetime
repair scope but fails overall on this now-repaired journal soundness class
and a related Debug root assertion. Fresh affected fuzz on the combined
AM399/400 source and full current Debug CTest remain mandatory. Neither
N1/N2 nor any performance gate is promoted, and Tier D is not submitted.
