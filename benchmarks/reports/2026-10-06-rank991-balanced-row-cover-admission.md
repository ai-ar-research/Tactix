# Rank 991 balanced-row-cover admission negative

CPU-Q job `1751021` tested the disabled, explicit `BalancedRowCover` wrapper on
Tier-D rank 991, `abw-K-dwt__234.mtx-w50`, with the `8,000,000` proof-addition
profile.  The immutable candidate was commit
`dc47aebddb2c50442731ed5eec64515a5db1540d`, based on
`581e817f48958c551f5328c6a553542d8105d827`; its release binary was
`e733c6b7fe89a149437d6c447bde359a10a36697ffb9ffd07a6bb0192b799910`.
The compressed input hash was
`a12f681de4000a95988891fe1951c6494a3eb9d605765d54cab042269eb0b050`
and the decompressed checker-input hash was
`30ff5b99bc373ab5486789be475e7e7a984dd72a58e437304a3344f4004e58eb`.

The 300-second run ended `UNKNOWN` with timeout return code 124.  It made
300,732 decisions, 30,752 conflicts and 70,817,916 propagations.  The action
was called 2,646 times, recognized zero structures and published nothing:
zero preflight or emitted additions, helpers, reverse implications and row
covers.  All 2,646 calls declined after a cumulative 9,688,066,406 recognition
work units.  The first call declined after 1,979,804 work units and 52.0 ms.
Together the calls consumed 164,958 ms, 55.0% of the run.  The probe therefore
did not reach or validate the saved 6,858,501-addition certificate estimate and
produced no SAT model or checkable UNSAT result.

The profile's `once` repeat applies within one controller sequence; it did not
prevent the same no-structure action from being selected again in later
restart sequences.  Future diagnostic admission actions need durable
no-structure/session suppression before search resumes.  Repeated recognition
must not stand in for that lifecycle state.

This is an admission negative.  The wrapper remains isolated and is not
integrated; its caps are not broadened.  The small exact fixture separately
verified the underlying DSR construction through backward and forward
`dsr-trim` plus Trestle, but that soundness check supplies no causal target
evidence.

The authoritative run is
[`benchmarks/runs/20261006-rank991-balanced-row-cover-admission/`](../runs/20261006-rank991-balanced-row-cover-admission/).
Its principal artifact hashes are:

- `result/result.json`: `3df94b50917e2c57a371acd71c87e978a5dee9262db41fe963639fd3172ccbfb`
- `result/controller.trace`: `a047b678691c64777c87ebf532a9e277ec8310fbecc88aa6e09b1a55e8e98249`
- `result/feedback.jsonl.gz`: `9a48f6ec08bf1fa67e5c34189bc63c812d134cb6169a3e534186d2d68ac15420`
- `result/provenance.jsonl`: `628c76546c984822497c52065b3aa9eb474fffb317963baba463bb32a2ffa74e`
- `result/proof.dsr`: `feb913c5adcacb3d7b26c5221d3d6b2bab2ea38be40e76ba6ecfa18b8f5e5d07`
- `result/solver.stdout`: `54899f40672e67a381eb681640009d766876022cf7228d081e8528a92c9277ac`
- source archive: `9affbe72573fb592cbe2179c2c41251fd00707c3bac499aa6b457aae9492b405`
- action profile: `33f417178c92b7cd2e53fd3ae128131432ff890e36e3032e7ba416ee43368901`

The 5,525,155-byte DSR file is the interrupted `UNKNOWN` search prefix and has
no checker claim.  Provenance contains only the 118-byte session record because
no structural transaction was admitted.
