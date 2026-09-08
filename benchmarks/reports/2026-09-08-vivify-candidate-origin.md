# Vivify clause-order drift: a native-only learned candidate

## Outcome

The Rank 1–40 objective remains unmet. Retained implementation5962 measures
7603.117/6810.066 = **1.116453x fixed Kissat**. This increment changes only
diagnostics, not solver behavior, scheduling, runtime results or promotion.

New6020/job1676418 is terminal and fully replays parent6008: every
ordinary300K trace, BVE journal, selected phase history and partial certificate
matches its own arm exactly. Both terminal records confirm scratch deletion.
The candidate observer records68164 rows in each arm.

## Decision-changing evidence

The selected seven-literal clause was already localized to the second Vivify
call at conflict270978. New candidate boundaries identify its only unmatched
within-candidate pose mutation:

- Native tier2, second occurrence, ordinal2249, reference1266110.
- Candidate literals: `53,51,-47,-93630,-4166,-4349,4172`.
- Selected incoming pose: `-246596,-47,49,-5409,5415,148944,238853`.
- Selected outgoing pose: `-246596,5415,-47,-5409,49,148944,238853`.
- Native spends1239 probing ticks on this candidate. The candidate survives.
- The same learned candidate is native's first unmatched candidate at tier2
  ordinal3666 in the first call. Neither captured Tactix schedule contains it.
- The previously frozen5937 formula snapshot independently lists exactly this
  learned clause as native-only at the subsequent first-BVE frontier.

This changes the next action from an assumed watch/budget defect to tracing
the producer or retention history of a specific learned clause. Its absence
from captured schedules alone does not prove absence from Tactix's database at
every earlier point. Restoring a native-only clause is not yet a justified fix:
Tactix may have soundly strengthened or deleted it, or learned a different
clause earlier. The selected operation could expose an upstream difference.

The complete first tier1 candidate identity sequence agrees (3769 candidates);
its first per-candidate tick difference occurs at1964 (304 versus306).
The second tier1 sequence also agrees (3723). There are small effort and stop
differences in other scopes; those are not by themselves a budget defect.
`candidate-analysis.json` retains all eight per-scope comparisons and all
within-candidate selected-pose mutations, without aligning past a differing
candidate by raw row index.

## Diagnostic implementation and tests

New6016 sources add a read-only selected-clause cache around candidate begin,
end and budget-stop callbacks. The cache revalidates a clause reference, scans
again after deletion/replacement, and resets at scope entry. The observer
records raw scope IDs, ordinal, candidate identity, ticks/limit and physical
selected-clause pose. It caches the first live exact copy; phase history proves
one copy at sampled boundaries, not uniqueness at all transient states.

The full Tactix trace suite passes99/99. Actual Tactix and native fixtures cover
pose changes, deletion, replacement, conflict filtering and scope reset. Five
malformed native selectors exit2. Native fixtures preserve the Release ABI.
The parser rejects eight malformed rows and tests all four portable mappings:
Tactix scopes2/3/4/5 mean native tiers0/1/2/3. Raw IDs remain in the evidence.
This normalization was corrected during source review before interpreting the
completed experiment; submitted solver sources/pins were not changed.

Preparation initially referenced the wrong field in6008's summary and stopped
before creating6020 or running a solver. The helper now checks `full_replay`.
No solver crash, rejected proof/model, or soundness incident occurred.

## Follow-up

New6024/job1676453 is also terminal and fully replays6020, including its original
candidate-pose stream and partial certificate. It reuses immutable6016 pins.
Only the phase-history selector changes to the causal learned clause, and
Reduce is added to the sampled macro boundaries. All emitted selected boundaries
have explicit absent/present frames. This run is not a timing refresh.

Native's first observed presence is bracketed by **Reduce end240062** (absent,
decision64659574) and **Reduce begin247206** (present, decision65290913,
reference1351108). It remains present at all subsequent sampled native frames.
Tactix has no copy at any sampled frame. Its ordinary slot-boundary journal
does not emit Reduce frames, so the native-only Reduce samples do not establish
a matched Tactix deletion window. The last common absent boundary is second
Factor end226862; first common presence difference is Congruence begin270978.
The 196 Tactix-only and110 native-only sampled boundaries remain explicit.

Next add a narrow standalone learned-clause stage observer on new clones,
using the existing learning callbacks over conflicts240062–247206. Capture
actual intermediate/final learned literals and conflict/decision IDs in both
arms; require exact ordinary trace/partial-proof replay. Identify the first
producer operation before proposing a general repair on5962. Do not attribute
this inter-boundary appearance to either Reduce call, or claim Tactix never
created the clause transiently. No restoring extra clauses merely for identity.

Implementation work must start from5962 after identifying a general defect,
with a new-test consideration at each change. B28+C5, full Rank1–40 and
current-source fuzz remain promotion gates. Proof checking continues
asynchronously. Fuzz ownership and the denied transfer remain unchanged;
no fuzz-owner jobs or artifacts were inspected. Existing cpu-q5784 runners,
manifests, resources and pins were not modified. No manual deletion or push.

## Authorities

- `benchmarks/runs/20260908-6016-vivify-pose-controls/summary.json`
- `benchmarks/runs/20260908-6020-vivify-pose/manifest.json`
- `benchmarks/runs/20260908-6020-vivify-pose/summary.json`
- `benchmarks/runs/20260908-6020-vivify-pose/candidate-analysis.json`
- `benchmarks/runs/20260908-5937-first-bve-frontier/summary.json`
- `benchmarks/runs/20260908-6024-vivify-lemma-history/manifest.json`
- `benchmarks/runs/20260908-6024-vivify-lemma-history/summary.json`
- `benchmarks/runs/20260908-6024-vivify-lemma-history/presence-analysis.json`
