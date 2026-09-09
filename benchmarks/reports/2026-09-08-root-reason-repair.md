# Root-reason retention repair: causal closure through 300K

## Decision and outcome

Retain isolated6049 as the next general implementation baseline. It removes a
spurious root-reason retention lock; it does not tune Reduce's cutoff, ranking,
scheduler, or any instance-dependent policy. The new Rank32 diagnostic6060
matches all55 recorded reductions and all69,828,932 decision rows through
300,000 conflicts against frozen native traces. This closes the previously
observed split at271085 and the retention/Vivify/BVE ancestry behind it.

The runtime goal is **not established**. Preceding5962 measured7603.117s against
the fixed6810.066s Kissat denominator (1.116453x). New full6061/job1676727 is
running; no current aggregate speedup is claimed at this checkpoint.
B28+C5 has66 expected solver verdicts: B305.613/410.569s=0.744365x production,
C655.438/705.710s=0.928764x.64 certificates were checked at the frozen first
complete solver snapshot; the other checks continue asynchronously.

## Complete semantic change

Native Kissat flushes root assignments before locking reasons for reduction.
CaDiCaL protects reasons of active, non-root variables. Tactix retains root
identities in its trail, so marking every trail reason incorrectly protects
root-derived redundant clauses.6042 had already proved that this changes the
candidate pool and first victim set atReduce51/conf240062, eventually deleting
the useful learned clause that native retains for Vivify.

6049's private `lock_reasons` skips assignments with true level zero. Pending
learned clauses are independently locked before the trail walk; non-root
reasons remain locked. Both enumeration and apply use the helper, preserving
stale-payload revalidation and all public legal instances. M1 already names
LRAT root units before their source clauses can be deleted, including root
implications retained below an elevated current solver level. Existing garbage
collection forwards live reasons and retires deleted root reasons to
`UNIT_REASON`. DRAT correctness is supported by independently checked complete
refutations, not by claiming it names every LRAT unit. No new primitive,
strategy value, API, or proof vocabulary was introduced.

Source is `.claude/worktrees/runtime-reduce-root-lock-6049`, successor of5962.
Its manifest binds the two changed files, cumulative and incremental patches,
four immutable builds and validation. Release and BC hashes are identical:
`8d0f17118dd505faeaa558728bbc36b0451ce2974381e7bf77dd71b45b3d9ed2`.
No production source was promoted.

## Correctness and test consideration

Every affected boundary received new tests:

- Actual learned-clause/M1/Reduce fixtures cover16 root cases: root or elevated
  solver state, sparse or compact collection, SAT or UNSAT, DRAT or LRAT.
  The exact same final fixture against5962 finds zero root candidates and
  deletes zero;6049 finds one and deletes one. Root assignments and elevated
  decisions survive; SAT models are checked against original clauses.
- Actual non-root and pending-learned cases remain protected, including raw
  stale references and public stale instances. Candidate enumeration preserves
  arena, trail, statistics and proof identity; all public count instances remain
  available. Kernel invariants hold.
- An actual native caller, compiled with the reference's NDEBUG ABI, checks
  root, elevated-after-root and non-root reason admission in three cases.
- Full suites pass Release98/98 twice, trace97/97 and journal98/98. The complete
  proof seal binds62 independently checked current-source DRAT/LRAT proofs:
  eight new root-reason witnesses,46 inherited coupled-rule witnesses and
  eight BVE inactive-pop witnesses.

The submitted solver pins were not rebuilt when a stale diagnostic annotation
was noticed. Separate6054 changes only the Reduce census's reason-level filter
and adds an actual root/pending/non-root annotation test; its trace suite passes
98/98.6060 uses this corrected observer, not6049's original trace pin. The
observer change has no solver-algorithm effect.

One aggregation helper6055 failed after all46 inherited checks passed because
it expected a different output filename. Preserve that failed helper and its
actual46-proof artifact. Successor6057 validates the existing hashes and adds
the remaining witnesses into `complete-proof-validation.json` (62). This was
a tooling filename error, not a rejected proof, solver crash, or failing unit
oracle. No failure was hidden by rerunning a frozen output.

## Causal validation and performance scope

6060/job1676661 is terminal, exit0, with scratch cleanup recorded. Its complete
candidate trace hashes and immutable native0–100K and100K–300K streams were
validated, including exact overlapping rows counted only once. All69,828,932
decision rows match through300K. At all55 reductions in the recorded0–270978
window, clause multiplicities, glue, usage, reason flags, eligibility, ranks and
victim flags match. The old reason mismatch and survivor chain are gone.
This is bounded causal evidence, not full-run trajectory identity or a final
checked proof; the diagnostic partial certificate was hashed and removed.

6059's B28+C5 solver gate passed before submitting6061's full40 Tactix-only
array. Both use12 logical CPUs and48000MiB per task, one single-threaded solver,
1200s solver-only limits where applicable,16000s checker allowance and5h
allocation. The diagnostic uses25min allocation. Arrays have no throttle or
node constraint. Fixed Kissat timing and native diagnostic caches were reused;
no native measurement was rerun. New runs have independent frozen runners and
pins; the2332-task cpu-q5784 campaign remains untouched at4CPU/32768MiB.

## Checkpoint and next action

Use `snapshot-root-lock-full-6065.py` once all40 expected solver records exist;
do not wait for proof checking. Compare the complete aggregate to fixedKissat
and the work/certificate changes to5962, then prioritize the largest remaining
worse-side component boundary. If Rank32 remains a leading loss, extend the
repaired trace beyond300K; reuse cached native evidence where available rather
than refreshing its timing. Preserve the general repair unless stronger
evidence disproves its correctness or reference fidelity.

Current-source fuzz remains unverified because the prior external-transfer
retry was denied. No retry, bypass or inspection of Claude-owned fuzz artifacts
was made. This blocks promotion, not further isolated development. No manual
deletion or push was performed. Normal campaign checker timeouts and the four
previously classified checker-status cases remain invalid/unscored under the
unchanged runner. The fresh6067 audit owns current counts and hygiene.

Primary evidence:

- `benchmarks/runs/20260908-6049-reduce-root-lock/manifest.json`
- `benchmarks/runs/20260908-6049-reduce-root-lock/complete-proof-validation.json`
- `benchmarks/runs/20260908-6049-reduce-root-lock/final-controls/summary.json`
- `benchmarks/runs/20260908-6054-root-lock-trace-controls/observer.patch`
- `benchmarks/runs/20260908-6059-root-lock-BC-gate/solver-checkpoint.json`
- `benchmarks/runs/20260908-6060-root-lock-rank32-prefix/summary.json`
- `benchmarks/runs/20260908-6060-root-lock-rank32-prefix/reduce-validation.json`
- `benchmarks/runs/20260908-6061-root-lock-ranks1-40/launch.json`
- `benchmarks/reports/2026-09-08-root-reason-repair-validation.json`
