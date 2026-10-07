# Graph-labeling Window cursor adapter

Status: focused Debug and sanitizer qualification passed; Rank991 recovered
with an independently checked model. Default enablement remains off.

The retained graph-labeling session now owns a resumable `WindowCursor`.
Lucky26/27 and DefaultPolicy preserve the exact Window frontier, random stream,
candidate and logical-work count across bounded continuations. The
source-derived profile keeps the Window total cap at 5,000,000,000 work and
uses its existing 100,000,000 Construction allowance as each DefaultPolicy
continuation dose.

## Immutable candidate

- Base commit: `49582550c89708ef650d40ac61c20164eba4a24c`
- Adapter patch SHA-256:
  `f6441fc2f269a73a02e12286c65d9b883ed8415d9ce4521bdb4ab266127a22b1`
- Release job: 1754453, terminal `COMPLETED 0:0`
- Release binary SHA-256:
  `5f08a89cfe8ed71cea7d260047ec100f2a032fb791cc74d0ef2b36274803c412`

The earlier r1 candidate was superseded before target execution because it
allocated cursor storage during authentication while Construction scratch was
still live. The final candidate allocates the cursor lazily at Window entry,
charges legacy initialization work before validation/allocation, and admits
the mutually exclusive Construction or cursor-plus-candidate peak. Exact
workspace minus one declines atomically; exact workspace succeeds. Setup work
minus one makes no cursor progress; the measured setup cost succeeds.

## Focused qualification

SLURM jobs 1754401/1754407 built independent Debug and ASan/UBSan trees from
the immutable candidate. Both legs are terminal `COMPLETED 0:0` and each
passed all four selected tests with zero skips:

- `graph_labeling_window_cursor`
- `graph_labeling_model`
- `graph_labeling_actions`
- `strategy_config`

The matrix covers workspace and work admission boundaries, tiny slices versus
one large slice, interruption versus total-cap exhaustion, Default's bounded
continuation, Lucky26/27 native versus serialized replay identity, and no
logical publication from partial search. A final source-test-only follow-up,
SLURM 1754512/1754514, directly compared legacy and cursor
work/nodes/candidate at the total-cap boundary. Its Debug and ASan/UBSan legs
are both terminal `COMPLETED 0:0`; each passed the selected test with zero
skips. The final test patch is base `96c89925432039ef8e68cdde227eefd43543cbf4`
plus SHA-256
`31a991903b1e6c4c72ca089b52c39eeb1a041b849c2360ac0dfe24551e28c778`.

## Rank991 result

SLURM job 1754499 ran the immutable Release binary with normal scheduler
placement on node015, four cores and 32 GiB. The exact command used
`--default-graph-labeling=on --auto-proof`; graph labeling remains disabled by
default in production.

- Verdict: SAT, exit 10
- Solver time: 28.825 seconds; wall time: 28.954 seconds
- Search: 8,935 conflicts and 44,009 decisions
- Graph: 57 calls, one recognition, 117 vertices, separation 50, 24 seeds
- Retained work: 22,626,615 Window nodes, 1,434 repairs, 4,800,000 anneal
  attempts, 5,063,668,149 total work, one budget stop, zero interruptions
- Result: one graph SAT completion; 2,905,373 output proof bytes
- Original input xz SHA-256:
  `a12f681de4000a95988891fe1951c6494a3eb9d605765d54cab042269eb0b050`
- Independent checker SHA-256:
  `5fde6bbc7be305f6b6166ef75dc4788e2cf40180317ab1ec9a38a3d6071676cf`
- `checkmodel`: exit 0, all 178,213 clauses satisfied with 56,511 variables
  assigned

The previous monolithic retained-graph run used seven graph calls and solved in
13.940 seconds with 552 conflicts and 7,743 decisions. The cursor candidate's
57 calls are the expected bounded resumptions, and its structural graph
counters match: recognition, shape, seeds, Window nodes, repairs and anneal
attempts. The intervening CDCL work increases runtime and search counts.
Nodes and concurrent workload differed, so this is correctness and persistence
evidence rather than a precise speed comparison.

Job 1754485 was a pending-only submission with dynamic exclusions for active
project traffic. Its estimated start moved to 2026-10-09, so it was canceled
without execution. Job 1754499 used unconstrained scheduler placement in line
with the normal resource contract.

## Remaining limits

Only Window search is cursor-resumable in this increment. Repair and anneal
retain their bounded total allowances but remain monolithic within their
individual calls. Lucky21 retains its existing one-shot behavior. This target
result does not satisfy same-candidate regression or fuzz acceptance and does
not justify enabling graph labeling by default.
