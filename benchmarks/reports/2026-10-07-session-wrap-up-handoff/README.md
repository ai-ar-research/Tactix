# Session handoff: certified symmetry units and pending qualification

The user requested stopping at the next natural checkpoint. Development is
paused after local commit `d576abaeb365acab5bebde05bc3025a9f61f4717`,
**not after acceptance**. The goal of at least 12 independently verified fixed44
closures on one coherent candidate remains unmet. Earlier accepted coverage is
4/44; the intermediate b26 campaign has two checked rows, not a new accepted
portfolio. Read this handoff before the retained historical body of PROGRESS.

## First actions on resumption

1. Inspect `queue-at-handoff.txt`, current Slurm states and terminal result files;
   record submitted jobs without waiting for them. Read the [unit-admission checkpoint](../2026-10-07-symmetry-singleton-unit-admission/README.md).
   Its code removes singleton-unit withholding from both FixAndCut and
   large-domain generator fixing. Validated root-forcing binaries also bypass
   nonforcing-cut admission. Existing soundness guards, public IDs and replay
   values remain. This is an **unvalidated development checkpoint**.
2. Verify and submit the prepared r4 focused array below. Its two positive unit
   tests ran in r3 before an invalid synthetic truncation control failed. That
   control was removed; the existing exact nonforcing-batch controls remain.
   Neither standalone proof checks nor sanitizers completed. Do not infer a
   passing gate from those earlier partial runs.
3. Independently reproduce and repair Claude's deterministic persistent-affine
   root-maintenance API failure after Sweep. The owner-provided
   [minimal handoff](../../runs/20261007-firstopen-original-seed-fuzz-owner/handoff/am-rel-23/README.md)
   contains the CNF, scenario, driver, exact request sequence and Slurm repro.
   A well-formed request returns `InvalidRequest`; no wrong model/proof/verdict
   was observed. It reproduces on b26 and the older noL-free source. Keep this
   repair separate, then qualify one coherent source including both changes.
4. After focused correctness, freeze fresh Release/full Debug pins. Package all
   maintained ignored dependencies using the previous clean-builder procedure,
   verify source binding, and use **tactix only, without native transport**.
   Resume the existing Claude fuzz owner with the exact new binding and launch
   fresh B28+C5, Rank1–50, WV100 and WV2–89 qualification. Effective reach and
   independent proof/model validation remain mandatory.
5. Resume [Rank899's bounded trajectory task](../2026-10-07-rank899-unit-admission-handoff/README.md)
   after the focused proof gate: independently certify concrete `+40310`, verify
   structured-root trail growth, existing FastEl suppression, and exact residual
   ordering/activation queue before the same-CPU 300-second causal comparison.
   Do not add a benchmark-specific policy switch or revive unit withholding.

Prepared focused authority (not submitted at handoff):

```sh
sha256sum benchmarks/runs/20261007-symmetry-singleton-unit-admission-r4/pins/source.tar.gz
# 1249dc7f7b5bd8c3a401d944fa0d74a9aaed080c64e46f8fee685c5a70ae1eee
sha256sum benchmarks/runs/20261007-symmetry-singleton-unit-admission-r4/run.sbatch
# 1a1a42367b0b8c928b9f1c2329a8950e0939c47663b3492c72140dd82a787e9e
sbatch --parsable benchmarks/runs/20261007-symmetry-singleton-unit-admission-r4/run.sbatch
```

This archive precedes final status-only documentation; compare production/test
bindings to the committed checkpoint before building a cumulative candidate.
Never mutate submitted archives or pins. All solver runs and substantial builds
use Slurm; the development host has four cores.

## Work preserved in flight

The [clean-build handoff](../2026-10-07-symmetry-units-clean-build-handoff/README.md)
owns cancellation evidence: dependent Release1755536 and fullDebug1755537 were
canceled before start after focused1755527 failed. They produced no binaries.
Do not resume those permanently unsatisfied dependencies or count them as builds.

Intermediate b26 remains useful reference evidence only. Its
[fresh full Debug](../2026-10-07-b26ef-clean-debug-terminal/README.md)
passes **353/353**, zero skips; that result does not qualify the new unit change.
Four reference regression arrays remain active at the snapshot:

| Suite | Job | Final records / expected | Independently checked | Other final records |
|---|---|---|---|---|
| B28+C5 | 1755134 | 32/33 | 32 | none |
| Rank1–50 | 1755135 | 43/50 | 41 | two proof timeouts |
| WV100 | 1755136 | 98/100 | 98 | none |
| WV2–89 | 1755160 | 84/89 | 80 | four UNKNOWNs |

These are partial counts, not accepted gates. `regression-snapshot.json` binds
the exact result files and cutoff. Read terminal records and use the owning
regression grader before any gate claim; apply existing censor/timeout policy.
Do not rerun the saved d170 baseline or transfer b26 acceptance to changed code.

Seven b26 full44 tasks were left running:1755331 tasks0,1,3,18,29;
1755422 task6;1755462 task4. Exact queue and merged coverage are in the clean-build
handoff. Rank147 checks UNSAT0.168s; Rank401 checks SAT42.273s. All other missing,
busy or canceled rows remain incomplete. The35 pending retry02 tasks were
canceled as superseded. **No more b26 retries.** Check current scheduler state
before touching anything; unrelated job1751450 is outside this workstream.

## Claude owners and acceptance boundaries

- Current fuzz owner: `385a3588-c5ba-4eef-b07a-a5a237bd1182`, campaign
  `benchmarks/runs/20261007-firstopen-original-seed-fuzz-owner`.
  `SUPERSEDED.md` rejects b26 qualification; broad1755222/23/24 never started.
  Calibration found the affine API failure above. The owner was instructed to
  publish `HANDOFF.md`, preserve harness changes and stop without new jobs.
  Resume this same session with a new exact candidate; do not create a duplicate.
- Previous cumulative owner: `4d8f46d8-e163-4e13-9611-131f30e26ff0`, campaign
  `benchmarks/runs/20261007-owner-policy-cumulative-fuzz/FINAL_REPORT.md`.
  Terminal **SUPERSEDED/INCOMPLETE**:44,000 seeded broad cases with zero reported
  failures, but required targeted reach was not completed. This is not current
  candidate acceptance. Preserve its missing-reach and harness findings.
- Independent long64-fixed reviewer:
  `49c00a94-873e-4626-abb4-41fd26df74f8`, campaign
  `benchmarks/runs/20261007-long64-fixed-independent-review`.
  The terminal `DISPOSITION.md` arrived during wrap-up: **ACCEPTED** for
  explicit-profile use. Canonical32/fixed64 agree on587 corpus/mutant cases;
  directed1755534 passes. Qualification1755483 reports an OOM-killed raw-LSR
  Trestle child, disclosed as unchecked, not a rejection; the actual DRAT/Cake
  route verifies that case. No beyond-32-bit-capacity proof was tested. Read
  the full owner report and retain Trestle as final authority: some invalid
  inputs pass dsr-trim alone but are rejected end to end. A metadata repair is
  still required: `FIXED_LONG_DSR_PROVENANCE['repository_patch_sha256']` in
  `tools/bench/experimental_tierd.py` omits two hex digits. Correct it to
  `edd70e504e573fbf9801a1c9f80a05fae04e2bcadd763a1e2a1b2b923df3afc6`,
  preferably hash-bind the patch file, and issue a supplemental record for
  frozen manifests rather than editing them. Enforced binary/source bindings
  were correct; the owner reports no acceptance impact. This repair is left
  for the next session as requested; no new development was started.
- Native helper remains excluded: independent owner5c14af7b rejects r9 signal
  restoration. Use pure-Python transport and canonical32 for regression/fuzz.
  Explicit long64-fixed now has the bounded independent acceptance above;
  exact per-instance verification and all solver gates remain required.

To resume a Claude owner, inspect `claude agents --json` first. If still working,
stop that exact owner before `claude --bg --resume UUID 'instructions'`; otherwise
the CLI makes a copy. Do not touch unrelated historical owners.

## Decisions and evidence that must survive the handoff

- Never suppress a valid fully certified symmetry-breaking unit merely because
  it is a singleton or changes a benchmark trajectory. Discovery budgets and
  proof/root soundness checks still apply. Ordinary nonforcing cuts retain their
  separately documented admission policy.
- Theory reasoning and publication stay at initial/full-restart roots, never
  search or partial restart. Preserve authenticated equation provenance across
  simplification, substitutions and elimination; unsuccessful analysis is neutral.
- X17 and noL-specific local search stay retired. Reject overfitted LymphoSAT
  integrations even if named gaps remain open. General semantic applicability,
  varied causal evidence and total cost are required, not just soundness.
- Rank899's trace matches actual Kissat's first50 conflicts exactly:12,723
  decisions,50 analyses,50 learned clauses. It is selected search-prefix evidence,
  not preprocessing equivalence or a runtime claim. Historical checked66.579s
  involved retired MAB and is not a reason to reintroduce it.
- Rank569 remains open. Its24 recognized XORs are disjoint; raising Gaussian
  budgets does not create combinations. The frontier-clause experiment spent
  359M ticks/25MB successful proofs without measured solving benefit and is
  retired. The3627.980s full run still misses the compact/sub3359.975s target.
  The fixed64 deleted-clause bug plausibly explains its checker failure, but
  its deleted4.116GB proof prevents proof-specific confirmation.
- The initial overfit audit's Lucky10 no-positive-reach claim was corrected by
  `e3eb95c84`:21 real rows recognized,11 checked terminal SATs,10 search-free
  parity declines. Retain the opportunity while reviewing generality and cost.

## Storage, repository and report

Quota was508GB at wrap-up, below the700GB mandatory cleanup trigger. Earlier
audited obsolete Debug cleanup reclaimed44.57GB allocated. Preserve live pins,
retained proofs/repros, baselines and frozen authority; do not delete by age alone.
The unit owner separately records its small superseded-archive cleanup.

All implementation and handoff commits remain local. Earlier push authorization
did not extend to later commits; automatic approval review previously rejected
the push beyond that scope. No push was retried during wrap-up. Keep history.
Andrew's two-pass compiled two-page TeX/PDF are under
`report-to-andrew/20261007T224148Z-session-handoff/`, linked from PROGRESS.
Documentation checks pass (size,74 section references, proof-rule sync,
citations,418-option coverage,45-family inventory, default configuration,
rule-policy boundary and progress audit). Size checks warn about limited
reserve; PROGRESS is348/350 lines. No checks were weakened. Documentation history was not compacted. This is an intentional
paused checkpoint with known validation and API work, not a clean accepted build.
