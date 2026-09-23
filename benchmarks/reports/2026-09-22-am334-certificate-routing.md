# AM334: acceptance routed by certificate content

Checkpoint 22 September 2026 UTC. Base commit `75487222`. Approved by the user as a plan revision
after confirming that no solver change is involved. Frozen checkers: dsr-trim `3b5c7737`, Trestle
`19bf79f4`, dsr-compress `a44102ce` (AM324 pins) and the baseline's drat-trim / cake_lpr (byte-identical
to `tools/checkers/bin`).

## The problem

Five certificates never verified inside the 57,600 s checker window on any pin since AM324
(`oisc-subrv-sll-nested-8`, `tseitin_grid_n250_m250`, `tseitin_d3_n100000`, `tseitin_grid_n400_m400`,
`ncc_none_21015...`; AM324--AM333 gate directories). Each held 12 CPUs for 16 hours per campaign until
cancelled by ID. All five are witness-free: no substitution-redundancy row was applied, so their DSR
content is DRAT content and dsr-trim is merely a slower DRAT checker on them.

## The rule

DSR is a superset of DRAT: an addition carries a witness exactly when its first literal (the pivot)
reappears later in the line. Acceptance now routes by content, in the one place that defines the
pipeline (`tools/bench/proof_oracle.py`):

- a certificate with any witnessed line takes the Satsuma+Kissat DSR pipeline
  (`dsr-trim`, `dsr-compress -d`, Trestle), unchanged, with Trestle as the authority;
- a witness-free certificate takes the DRAT owner's pipeline as the baselines run it
  (`drat-trim -L`, then `cake_lpr` as the authority under the reference's checker amendment 2,
  `CML_HEAP_SIZE=24576` MiB: CakeML's 4,096 MiB default heap exhausts on large certificates), after
  `dsr_proof_as_drat` rewrites the binary line tags 1/2 (dsr-trim's encoding) to `a`/`d`; the literal
  varints are identical, ASCII is copied.

The solver, its rules, DefaultPolicy and the proof writer are untouched; every run still emits native
DSR before anything is known about the run. Solving in `--drat` mode is not an alternative: DefaultPolicy
then declines the structured root and every SR application, so the run itself changes.

The witness scan must be exact: drat-trim treats a repeated pivot as a duplicate literal and deletes it
with a warning (`php_sudoku_p15_h14`, 91 witnessed lines), so a witnessed certificate must never reach
drat-trim. `certificate_route` decides before any checker runs; `dsr_proof_as_drat` re-checks every
addition during the rewrite and refuses on a witness.

## Implementation

`proof_oracle.py`: `_dsr_lines`, `dsr_proof_has_witness`, `dsr_proof_as_drat`, `drat_stages`,
`certificate_route`, `check_routed`, CLI `check` (next to the unchanged `check-dsr`, which the proof
suites and the fuzz harness keep using with the every-lemma audit). `runtime_task.py`: the `dsr`
certificate branch routes (`pipeline = witness-free-dsr-as-drat:drat-trim-cake_lpr`) and the pin
contract requires `pins/drat-trim` and `pins/cake_lpr`; the gate-staging template copies them from the
frozen baseline pins. DEVELOPMENT_PLAN R1 and §5 and `docs/implementation-notes.md` carry the rule.
Tests: `tests/test_fuzz_proof_oracles.py` (witness scan on ASCII and binary, exact tag rewrite, refusal
on a witness, routed acceptance through the real checkers) and `tools/bench/test_runtime_task.py` (pin
contract); `micro_proofs` and `sweep_proofs` pass on a fresh build with the frozen overlay (job
`1714790`).

## Evidence

`benchmarks/runs/20260922-oisc-drat-routing-probe`: `oisc` is witness-free (70.2M additions, 41.1M
deletions); its ASCII form verifies with drat-trim in 7,003 s and its `--drat` twin in 6,990 s, inside
Kissat's own 4,271--7,814 s DRAT check of the same instance; on binary the tag rewrite is exact
(`sgen1-unsat-121-100`: drat-trim 1.2 s, cake_lpr 0.08 s, `s VERIFIED UNSAT`).

`benchmarks/runs/20260922-am334-certificate-routing` (job `1714785`): the five certificates regenerated
on the committed AM333 pin and checked through `proof_oracle.py check` with the frozen checkers.

First routed pass (job `1714785`, before the heap parameter): `oisc` route=drat, drat-trim verified in
7,253 s, cake_lpr then failed without output under the default heap, the exact failure shape the
baseline's amendment records. Diagnosis (job `1715196`, 64 GiB allocation): drat-trim 6,948 s (RSS 3.7 GB,
LRAT 1.64 GB); cake_lpr under the default heap exits 1 in 19 s with `CakeML heap space exhausted`;
under `CML_HEAP_SIZE=40000` it prints `s VERIFIED UNSAT` in 55 s (RSS 41.7 GB: CakeML reserves the whole
heap); lrat-check, a diagnostic and not an authority, agrees in 18 s. The route now carries the
reference's 24,576 MiB heap, which the baseline's own gates run inside 32 GiB.
`tseitin_grid_n250_m250` as tag-rewritten binary DRAT did not finish
drat-trim within 30,000 s (job `1714724`): the three Tseitin parity refutations (0.87--2.6 GB) are a
proof-size problem that no route fixes; their remedy is a parity-native certificate, out of scope here.

| Certificate | Solve | Route | Check | Result |
|---|---|---|---|---|
| `oisc-subrv-sll-nested-8` (745 MB, 70.2M lines) | 236 s | DRAT | 7,182 s (drat-trim ~6,950 s, cake_lpr ~55 s) | **VERIFIED** (was: 57,600 s timeout; Kissat's own DRAT check 4,271--7,814 s) |
| `ncc_none_21015...` | | DRAT | in its 16 h window (job `1715197_1`) | pending |
| `tseitin_grid_n250_m250` (872 MB) | 12 s | DRAT | drat-trim > 30,000 s | not verified: proof size |
| `tseitin_d3_n100000` (1.74 GB) | 21 s | DRAT | in its 16 h window | expected not verified: proof size |
| `tseitin_grid_n400_m400` (2.63 GB) | 33 s | DRAT | in its 16 h window | expected not verified: proof size |

## Not in scope

Technique-native certificates (a counting-invariant proof for `FmlaEquivChain_4_8_8`, VeriPB) remain a
separate decision. The `checker_timeout_overrides` cap on these five rows stays in the staging template
as a safety net until the routed gate campaign confirms them verified.
