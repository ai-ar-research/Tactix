# First-BVE attempts and earlier heap topology

## Decision

Continue from independent **5900**, not rejected composition5920. The latest
measured Rank1–40 score remains **7573.054 / 6810.066 = 1.112038x** the fixed
Kissat reference; the ≤1.01x objective is unmet. Its B28/C5 screens remain
0.744014x/0.929844x production. This increment adds causal evidence, not a new
implementation or measured speedup. Production and every old campaign remain
unchanged. Proof checking continues asynchronously; current-source fuzz remains
blocked by the previously denied external-transfer approval, with no retry or
fuzz-artifact inspection.

## Complete first-BVE evidence

5937/job1675374 is terminal. Its new formula, binary and candidate observers
replay every ordinary 300K-conflict trace hash and partial-proof byte per arm.
The frozen `summary.json`, `bve-frontier.json` and new
`attempt-classification.json` own the evidence.

At the first Strengthen exit, conflict271079:

- All **461336 irredundant clauses** match in literal-set identity and arena
  order, with matching root values. **816 literal poses** and 18096 metadata
  records differ. This is not identical physical input.
- The only large-clause multiset differences are **two learned clauses per
  arm**. All **124857 binary copies** match, including multiplicities.
- The earlier second-forward entry already differs by 172 large clauses per
  arm; its equal counts did not establish formula identity.

The initial raw candidate comparison stopped after137 rows because native
emits `postflush` after every attempt while Tactix omits it on an unchanged
decline. Analyzer5943 groups complete begin/end/optional-postflush attempts,
validates the omitted-root-change condition across the entire capture, and
preserves rather than discards the discrepant attempts.

There are168860 Tactix attempts and168819 native attempts across both rounds.
The first complete-attempt difference is index22616: Tactix processes already
retired89339 again and increments its elimination count. Native pops the same
physical ghost but rejects it before the candidate callback. There are nine
inactive Tactix attempts in total. This exposes an unclosed policy eligibility
gap; it is not evidence of a rejected proof or its contribution to runtime.

In a separately labelled projection that excludes those inactive attempts,
the first120872 active attempts agree on identity, outcome, value, absolute
resolution ticks and limit. The next active choices are225614 versus225641,
both declining at43990691 ticks under65731490. The projection does not excuse
the inactive attempts or prove that all preceding formula effects agree.

## Selected heap observer

5949/job1675537 is terminal and **fully replays all ordinary trace hashes,
partial proof bytes and the complete5937 BVE-candidate journals**. Tactix uses
the unchanged5900 pin; the native clone changes only read-only heap callbacks.
The source patch, inputs, pins and controls are frozen in the new manifest.
`heap-validation.json` owns the replay and selected-state comparisons.

All four selected subjects match at both initial heap snapshots. In the first
round, subject225614 matches its first seven observations. At the common pop
of122629, it moves from the common tail position8413 to **4465 in Tactix versus
4472 in Kissat**. Both show the exact score `-0x1.299c942e0440ep+208`, physical
occurrences11/16, next root165965 and remaining heap size8413. This is a
same-callback topology witness earlier than the active-candidate split. It
does not show the first differing heap mutation or justify changing the tie
comparator, which has the same strict child-selection comparisons in both
implementations.

For89339, native's extra `update` row is an intermediate state before pushing
an absent variable; its following `push` matches Tactix's `update-reinsert`.
Both physically reinsert and later pop the ghost. Native has no remaining
physical occurrences when retired, whereas Tactix retains two negative-side
entries. Do not confuse this stage asymmetry with the same-pop topology
witness, or assume the nine extra attempts explain that later topology.

The incoming clauses containing the four selected subjects have matching
literal poses. The global816 pose differences therefore remain a hypothesis
about intervening updates, not a direct local witness on those four subjects.

## Controls and incidents

- Analyzer5943 adds complete-attempt grouping, missing-stage and effort checks,
  including nine negative fixtures. Heap parser5951 adds seven negative cases.
- Native observer5948 passes actual heap construction/update/pop, absent-member,
  external-remapping and per-round-reset controls, plus seven bad selectors.
- The first native microfixture omitted schedule reservation and crashed before
  reaching the observer. ASan located `kissat_enlarge_heap` writing null `pos`;
  reserving capacity exactly as the native caller does fixes the fixture, and
  the corrected ASan replay passes. This was not a solver benchmark failure.
- Final caller review found that5944 incorrectly rejected valid construction
  callbacks before `initial`. Only new job1675510/campaign5945 was cancelled;
  both tasks are terminal CANCELLED and both records confirm scratch cleanup.
  Its runner/pins/results remain immutable. The expanded actual-call-sequence
  fixture reproduces exit2 on5944 and passes5948. Corrected5949 then provides
  the full real-instance replay evidence above.
- Three fixed kernel-level BVE unit-producing fixtures5952 pass invariants and
  produce exactly1/3/5 retired variables and elimination counts. They do **not**
  reproduce the duplicate-retirement case; no repair is claimed from them.

Every change received a unit-test coverage decision. No Tactix rule, policy,
guard, public instance surface, budget, parameter, proof vocabulary or default
changed; the new tests cover analysis tooling and observer lifecycle. No new
fuzz campaign is required for unchanged solver code. Native compilation retains
the pre-existing unrelated `factor_fresh` format-truncation warning.

## Next action

Capture full BVE heap state immediately before the common pop122629 in the
first round at271079, including both child subtrees on its sift path, and bind
it to the initial schedule. Locate the earlier differing update/push and its
clause order before selecting a general repair. Separately reproduce the
inactive-pop eligibility discrepancy with a small actual policy fixture; do
not change public BVE availability to fix a scheduler-only omission. Keep
5900 as the base and require B28+C5 and checked Rank1–40 evidence for any new
implementation candidate. Existing baseline and campaigns are unchanged.

Primary artifacts: `benchmarks/runs/20260908-5937-first-bve-frontier/`,
`benchmarks/runs/20260908-5949-bve-heap-lifecycle/`,
`benchmarks/runs/20260908-5948-bve-heap-lifecycle-controls/`, and
`benchmarks/runs/20260908-5952-bve-retired-reentry-controls/`.
