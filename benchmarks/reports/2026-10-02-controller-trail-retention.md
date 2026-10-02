# Controller ordinary partial restarts: focused validation

This is the second shared-policy preparation increment, following `75698803`.
It implements ordinary trail retention in the random controller; it does not
accept the complete migration or replace new full Debug, Claude-owned fuzz,
default correspondence, overhead or four-set regression evidence.

The [controller interface](../../docs/controller-interface.md) owns the profile
and lifecycle contract. Both policies now use the typed native execution path
introduced by the first increment. Default scheduling and source are unchanged
by this increment. The controller uses the active brancher's next variable and
the existing native ranked-prefix reuse threshold; no new sampler RNG draw is
introduced. The root selector's conditional uniform sampling law remains the
same, while partial continuation changes when the next root boundary is reached.

The default retention profile is `ranked-prefix`; `full` remains an explicit
profile for callers needing every due restart to reach root. Positive keep-level
continues search without reinstalling its configuration or rotating its phase
clock. The due trigger is consumed and a fresh minimum conflict interval starts;
reluctant sequence/history and adaptive averages remain intact. Full restart
returns to the root action loop. Strategy reconfiguration and root inprocessing
remain unavailable during retained-prefix search. The profile is included in
transcript configuration, Restart service events include actual keep-level,
and coverage distinguishes partial/full restarts and continuation intervals.

Eight focused Debug tests passed in 3.14 seconds: restart_reduce, scheduling,
strategy_config, solve_reset, controller_trail_retention, policy_action_execution,
random_controller_policy and restart_controller. The retention test checks
actual surviving/removed assignments, both active branchers, restart counters,
configuration/sequence/rotation retention, clock/history retention and rejection
of root actions/configuration changes before a full restart. It solves fixed
five-pigeon/four-hole UNSAT fixtures for 16 seeds under each retention profile;
ranked-prefix coverage is required to be nonzero, and full mode must report no
partial restarts. Native reuse counters must agree with controller coverage.

After adding retained-prefix transcript checks, the updated retention test
passed separately. It records an actual partial-restart solve, replays using
the transcript seed despite a different constructor seed, compares full/partial
counts, and rejects replay under the wrong retention profile. Temporary trace
files are removed by the test. This is fixed-fixture validation, not a campaign
of fresh random seeds or independent proof/model fuzz acceptance.

The [validation record](2026-10-02-controller-trail-retention/validation.json)
binds source and log hashes, selected test outputs and documentation checks.
The development build has proof shadow enabled. Its first attempted target build
preceded CMake discovery of the new test; reconfiguration corrected that. A test
fixture initially omitted required clause flags; the compiled corrected fixture
passed. Subsecond filesystem-clock warnings appeared during linking; the final
targets compiled/linked and ran, and these mutable development binaries are not
submission pins. Final immutable validation builds remain required.

No real-instance solver campaign, checker rerouting, scalar reward computation,
learning, rollback, forced resumption or fallback is part of this increment.
Accepted source/binary/checkpoint authorities remain frozen. Remaining work is
the full shared startup/search/support/inprocessing migration, feedback and
offline interfaces, preserved-default correspondence and all goal gates.
