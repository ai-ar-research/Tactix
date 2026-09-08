# Vivify maximum selection and asynchronous correctness checkpoints

7 September 2026. Production remains `c2a95a6`; the Rank 1–40 objective of
at most 1.01x Kissat is not achieved. The latest complete Rephase solver view
is 1.202033658x, provisional while certificates finish; checked production
is 1.2787518x. Prior evidence is owned by
[the preceding checkpoint](2026-09-07-runtime-next-boundaries.md).

## Decision and implementation

Candidate [5438](../runs/20260907-5438-vivify-max/manifest.json) descends from
phase-entry 5430, not the independent metadata candidate 5426. In the pure
irredundant, non-flushing, reusable-key Vivify schedule, replace sorting each
literal snapshot with linear maximum-count selection. The downstream order
uses only that maximum count, clause size and arena position, plus existing
priorities. These private irredundant snapshots do not alter clause pose.
Equal-count maximum identity is immaterial. Mixed/redundant lexicographic
ordering, prefix flushing and the existing large-irredundant no-sort regime
are unchanged. No public rule, guard, proof step, allowance or cadence changes.

Kissat's `vivify.c:init_countref` with `COUNTREF_COUNTS == 1` similarly selects
the maximum directly. CaDiCaL's global lexicographic ordering remains relevant
to the unchanged mixed/prefix paths. This is an engineering optimization,
not a new calculus operation or production scheduling disposition.

Clean profile 5425 attributes 1.66% self samples to `vivify_schedule` and
0.98% to candidate introsort. Those samples motivate investigation but do not
measure the particular literal-sort branch, nor establish its eventual gain.
Reject a neutral/noisy result as evidence of useful runtime improvement.

## Fixed validation and immutable identity

The new public-scheduler fixture exercises 144 cases: six scopes, reusable
keys on/off, flushing on/off, three priority patterns, and two clause
populations across the existing no-sort condition. It checks repeatability,
nonmutation, unique candidates and maximum-count ordering where applicable.
The actual immutable 5430 objects and new implementation produce the same
complete order-and-pose digest, `a223a06d79fb69dd`.

Independent Release builds 5438 and 5439 pass 79/79 tests in 7.40 and 3.77
seconds respectively. Both binary SHA-256 values are
`2e316cb494cc10e24a917f8e8eaba7855d742168063961980da40363ad3a9d74`.
The source authority retains incremental/cumulative patches, source hashes,
build logs and old/new fixture evidence. Production fixed tests pass 77/77
in 5.22 seconds. No production source was changed or candidate promoted.

## Submitted gates

- [5440 timing](../runs/20260907-5440-vivify-max-rank35-runtime/manifest.json),
  job `1670118`: three candidate/predecessor pairs against exact phase-entry
  5430. Require identical complete non-timing statistics and proof bytes in
  every pair. Report median paired time and full range; an absolute headline
  additionally requires both arm ranges within 2% of their medians.
- [5441 B28+C5](../runs/20260907-5441-vivify-max-BC-gate/manifest.json),
  job `1670119`: unchanged 66-task cumulative regression gate against production.

Both arrays use immutable independent pins, full width, one solver per task,
12 logical CPUs and 48000 MiB, with no node constraint or throttle. They are
queued at this checkpoint. No measured speedup is claimed for 5438. Existing
full-view, profile and diagnostic campaigns continue; their live states and
hashes are recorded in the accompanying validation snapshot.

## User-authorized background fuzzing

The user requested periodic fuzzing, then explicitly requested background
execution while development continues. Plan §8.2 now records that workflow:
run checkpoints after meaningful implementation batches, retain Claude's
exclusive campaign/oracle ownership, and do not infer a passing gate while
pending. A reported correctness failure pauses dependent work and starts
the bounded Claude repair workflow. External failure repair still requires
waiting for its owner; that separate contract was not relaxed.

Parent-owned prompt `benchmarks/probes/scratch/claude-periodic-fuzz-5442.txt`
assigns immutable source 5438, baseline regressions and 10,000 seeded cases,
risk-based debug/sanitizer/model/DRAT/LRAT/differential/random-policy coverage,
targeted reachability, immutable snapshots and the existing fuzz resource
contract. Background Claude process `2920923`, parent tool session `30094`,
owns submission, inspection, interpretation, shrinking and cleanup. Its
launcher exposes only the terminal response. No fuzz job or artifact was
inspected by the primary agent, and no fuzz result is claimed here.

The generic progress checker scans all jobs and build pins, including
Claude-owned fuzz resources. Its citation-only check is safe; the broad
queue/build scan is deferred until the owner finishes. Instead, the checkpoint
audits explicit non-fuzz runtime IDs and immutable candidate identities.
This is a disclosed validation scope limitation, not a passing full check.

## Next action and hygiene

Judge 5440 after all three pairs; revisit its proof/regression/fuzz outcomes
without blocking independent investigation. Compare phase-entry's full view
and run diagnostics 5436/5437 when complete. Resolve unique binary clauses
versus physical multiplicity at the earlier Rank-35 Sweep boundary before
modifying that mechanism. Do not conflate trajectory differences with cost.

All living documents remain within required budgets. No material was deleted;
active checker scratch, submitted sources and user allocation `1668242` are
preserved. Separate future solver/checker jobs and pushing remain unapproved;
neither was attempted. Andrew's report is local under
`report-to-andrew/20260907-2144-vivify-max/`.
