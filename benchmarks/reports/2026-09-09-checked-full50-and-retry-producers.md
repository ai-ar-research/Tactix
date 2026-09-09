# Checked Rank 1–50 comparison and Rank 32 retry producers

Fresh accepted-production/current-Kissat campaign6121 (`1676920`) is complete:
100/100 certificates checked, zero residue,50 solves per arm (25SAT,25UNSAT),
no solve losses or invalid certificates. Solver totals9979.146/7160.088 seconds
give **1.393718345x**. The [final table](2026-09-09-fresh-rank50-final-6227/comparison.md)
retains every rank; adjacent TSV includes peak RSS and classifications, and
summary binds records, pins and bounded accounting. Snapshot6226 inherited an
interim label despite100 final records;6227 is the explicit final successor.

Both arms used12 logical CPUs,48000MiB, one bound solver thread,1200 solver-only
seconds,16000 checker seconds and5h allocations. All20 matched Rank41–50 results
in6108 were already checked under this same allocation shape;cpu-q4CPU times
are not mixed into the12CPU baseline. This is one paired wave, not repeated
causal timing evidence for any candidate repair.

Latest isolated6169 remains8403.998/7239.010=1.160931951x the frozen50 reference,
48/50 checked at this checkpoint (tasks21/42 pending),all50 exact6107 work/proofs.
BC6171 is66/66 checked. Different campaign windows cannot establish a causal
candidate gain. Memory remains open: fresh accepted Rank21 peak RSS is
4725892/1436392KiB. The private FlatLists redesign remains unapproved.

## Missing Rank 32 retry producer

[6217 summary](../runs/20260909-6217-rank32-retry-members/summary.json) verifies
complete6208 per-arm trace and partial-proof replay with exactlyfive selected
rows appended. Allfive omitted members are free/nonretired in Tactix; stale
assigned-level fields on free variables are not current decision levels.
Seven selected-state/retry-parser negative controls pass.

For337674, the last shared marks are271079/67954244. Native alone records two
`kissat_delete_binary` marks at319386/73705568,first0→1 then1→1,probing=true,value=0.
Tactix has no marks there. General Tactix binary deletion primitives already
mark both endpoints, so a blanket primitive repair is not justified.

Canonical Substitute's direct watch rewrite is the next hypothesis: its fast
DRAT tautology/unit branches delete proof clauses without the ordinary deletion
primitive. Native queues these retired binaries and deletes them after unit
propagation. This does not yet identify the actual observed clauses/caller.
Retain SP-SLOT-5 event-count watermarks; no tie-breaking or threshold tuning.

## Follow-up and validation scope

6224/job1677189,submitted2026-09-09T02:24:10, reuses frozen6206 diagnostic pins.
It adds selected binary snapshots at319386 and lifecycle tracing for four signed
337674/468072 pairs suggested by shared deleted clauses. Require every6217
parent trace and partial-proof hash to replay before interpreting results.
These endpoints are hypotheses, not exhaustive coverage.

Source review found the frozen `slot_boundary` selector misses native Probe
macro names. Native snapshots may be absent or only forward boundaries. Native
lifecycle records remain independent evidence; never claim paired snapshot
coverage. Validator6228 explicitly records this and has eight rejection tests
for framing, counts, identities and truncation while retaining duplicate copies.
The initial fixture's tabbed payload was corrected from source inspection to
the actual seven-field, pipe-delimited payload before result validation.

6224 completed before landing: its summary verifies every6217 parent trace and
partial-proof hash. Native lifecycle explicitly names `substitute` then `delete`
for `(337674,468072)` and `(-468072,-337674)` at319386. Tactix's60 selected
snapshots remove precisely those pairs across the first Substitute boundary;
its lifecycle contains their earlier Strengthen additions but no deletions.
Native snapshots are absent, as predicted by the selector review. Combined with
6217's missing retry marks, this identifies canonical Substitute's bookkeeping
path; it does not yet establish repaired heap/trajectory or runtime closure.

No solver source changed. Any repair needs an actual failing fixture for the
responsible deletion path, both reference lineages and full required gates.
Rank50 separately needs actual deadlines/all admission inputs at823562, not just
the statically different watermark expressions. Production and all ongoing
campaign pins/runners remain unchanged. Checkers run asynchronously; Claude-owned
changed-source fuzz remains open. No denied transfer/push retry, promotion,
broad cleanup or new architecture was attempted. Docs remain within budgets.
