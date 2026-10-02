# Shared controller completion and feedback checkpoint

The shared-policy/feedback goal remains incomplete. This increment extracts
common pure observation and completion machinery, makes root selection
replaceable, wires random-controller feedback, and adds offline recording,
inspection and replay. DefaultPolicy's native implementation is unchanged;
its feedback integration and the whole-goal acceptance campaigns remain next.

## Implementation

`controller_observation.hpp` preserves the previous controller snapshot shape
and extracts its kernel query. `FeedbackSession` owns optional after-action
delivery and interval timing; the disabled path performs no feedback snapshot,
timer read or record allocation. Records own their request/allowance values,
native outcome, before/after snapshots, actual work/overshoot and stop reason.
The receiver gets no mutable kernel or selector. Search service callbacks finish
native application and heuristic publication before delivery; M3 remains atomic.

RestartController records requested root rules, ResumeSearch, propagation,
analysis/backjump, decisions and restarts. Successful ResumeSearch starts an
interval after root work; restart closes it, and retained-prefix continuation
starts a linked interval without reinstalling strategies. Terminal/interrupted
sessions close once, before heuristic detachment. Rejected, stale, zero-work,
exhausted and interrupted requests retain their explicit results. Root selection
is replaceable through ControllerSelection; the default remains the original
seeded random chooser and unchanged sampling law.

FeedbackTrace uses an independent JSON-lines v1 stream. Deterministic snapshots
include 433 scalar native measurements and two arrays (rule calls and used
glue); native wall timers are excluded from replay. Elapsed records are checked
structurally, without comparing their values. Stable action identities,
complete semantic requests/allowances and raw search enum values preserve
rejected requests as evidence. Provenance, deterministic divergence, malformed
timing, missing/trailing records and session lifecycle errors are detected out
of band and do not change native execution. The inspection tool summarizes
progress, statuses, durations and work/overshoot by native currency; it does not
calculate a reward or combine currencies. The legacy controller transcript is
unchanged. The living contract belongs to
[controller-interface.md](../../docs/controller-interface.md).

## Verified evidence and limits

[validation.json](2026-10-02-shared-controller-feedback/validation.json) owns
source/binary digests, command output, case artifacts and independent-oracle
results. The build/compare scripts are retained alongside it.

- Normal Debug and journal/event-trace Debug each pass 10 focused checks. They
  cover pure repeated reads, disabled instrumentation, selector replacement,
  stale/invalid/zero-work/interrupted results, actual BVE effort and overshoot,
  terminal delivery, full/partial interval boundaries, replay and corrupted
  evidence. Six paired seeded SAT/UNSAT runs compare all deterministic kernel
  snapshot fields with feedback off/on. Sixteen pigeonhole seeds require actual
  partial and full restarts and replay their feedback.
- A preserved `352932d5` random-controller translation unit, with its two
  original headers, is linked against unchanged native machinery. In each build,
  all 16 complete-run signatures and 56 retained artifacts are byte-identical
  across preserved/current/feedback-enabled executions: requests, deterministic
  transcripts, CNFs, proofs and reconstructed models. Each build independently
  checks eight UNSAT proofs and eight original-CNF SAT models and validates all
  16 completed feedback streams. These are the same fixed cases across builds,
  not 32 distinct benchmark instances.
- The 13 documentation/configuration/boundary gates pass. Six pre-existing
  documentation reserve warnings remain; no compaction or weakened check was
  used. The statistics inventory is a separate required structural check.
- A concurrent normal check failed to reopen the default signature fixture's
  PID-named temporary proof. Tool processes can share `/tmp` while using the
  same namespace PID. Both signature fixtures now use unique temporary
  directories; terminal concurrent normal/diagnostic checks pass and clean up
  those directories. This repaired test-file ownership, not solver behavior.

The preserved-source comparison is deliberately limited to these fixed
controller fixtures. It does not establish broad default action/state
correspondence, complete action/configuration reach or runtime parity. No new
full Debug, fresh 100,000-seed fuzz leg, four-set regression campaign or overhead
measurement has run. Ordinary DefaultPolicy source/header digests and the
accepted foundation debug/release pins and baseline results are unchanged.

## Next action

Wire this common session around DefaultPolicy's constituent atomic
applications and support lifetimes, preserving native request/parameter
descriptors, preparation/publication order and search-interval boundaries.
Complete frontend integration, remaining operation-map reconciliation and broad
preserved-default correspondence. Then finish fresh full Debug, both
Claude-owned 100,000-seed legs and retained corpus/coverage acceptance, all four
bounded default regression gates, both-policy instrumentation purity and
measured overhead, and the final Andrew handoff. O6's scalar reward decisions
remain open. No learning, fallback, rollback, forced resumption, global loop
budget, push or unrelated experiment was introduced.
