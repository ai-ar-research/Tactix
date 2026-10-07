# long64-fixed independent review — DISPOSITION (terminal)

Reviewer: Claude session 49c00a94 (independent of the fix author). Date 2026-10-07.
Subject: explicit `--dsr-checker-profile=long64-fixed`, commit `4c17dee25fb0dd2861f61659e0d634b8ae9ffb25`.

## Verdict

**ACCEPTED** for explicit-profile checker use in a fresh full44 candidate campaign.

One bounded **provenance-record defect** was found. It sits outside checker acceptance and is
handed to root below. It changes no checker verdict, executable, or enforced hash binding.

## Immutable subjects (all hash-verified by the jobs)

| Item | SHA-256 |
|---|---|
| fixed `dsr-trim` (fixed-pins/) | `30207e6f0a6e3b5374976e6ef06528cea21c429383223ba46ae1020b8d7ff564` |
| fixed `dsr-compress` | `8ddab8302509ff6c3ebe1b6534698d64ff072b202b5b5f903dc4898bf73dd531` |
| Trestle/srcheck (unchanged) | `19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945` |
| canonical32 `dsr-trim` | `3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee` |
| unpatched long64 `dsr-trim` (diagnostic) | `94f5fc0c122b7996207fb38f9010180f2d1e6b61b3008a3ef515ab5d971bc614` |
| source.tar | `397557639afc3a58b7a46979a4bb77e09b664b049b7017d71355435ba08632ad` |
| fix.patch | `52592ba37c46b422d77021dd2bf9c46a72f3cc476eddf90239dc114ab253deb8` |
| capacity patch (tools/checkers/patches/…forward-elaboration.patch) | `edd70e504e573fbf9801a1c9f80a05fae04e2bcadd763a1e2a1b2b923df3afc6` |
| qualify.py / directed_v2.py | `7996ea87…987267` / `1931d84a…4b1992` |

## Jobs (cpu-q, 4 CPUs, 32 GiB, no node constraint, no throttle)

| Job | State | Use |
|---|---|---|
| 1755471 | FAILED 1:0 | Sanitizer build failed to link (needed `LDFLAGS`). The u64, f64 and u32 rebuilds still reproduced their pinned hashes. Superseded. |
| 1755475 | CANCELLED (by me, 48 s) | UBSan `shift-base` fired on upstream `MSB64 = (1L<<63)`, which aborted every sanitizer run. Superseded. |
| 1755483 | Slurm state OUT_OF_MEMORY 0:125, elapsed 10:20 | Main qualification. Every stage completed and wrote its JSON plus `summary.json` (the driver printed `end=`). One child process was OOM-killed: see the Trestle `error` row under "Not tested". Harness defect: v1 directed controls never reached the restore path (replaced by 1755534). |
| 1755534 | COMPLETED 0:0 | v2 directed deletion-restoration controls. |

Tool inputs (proof_oracle.py, runtime_task.py, experimental_tierd.py, fuzz.py, selftest.sh) were
hash-bound in `manifest.json`. They are unchanged at the current HEAD, even though the repository
HEAD moved during job 1755483.

## Evidence

1. **Source binding.**
   - `source.tar` is byte-identical to `git archive 0b8cc0e` of `dsr-trim-sr` plus the 64-hex capacity patch.
   - Clean Slurm rebuilds from `source.tar` (GCC 13.1.0) reproduce every pinned executable exactly:
     unpatched long gives 94f5…/3adb3…, patched long gives 30207e…/8ddab8…, and unpatched default gives 3b5c…/6bc416….
   - The fix is the single `IS_DELETED_CLAUSE` line.
2. **Width review.**
   - `-Wconversion/-Woverflow` over all sources under LONGTYPE: the only diagnostic the fix removes is
     `global_data.c:20 -Woverflow`, the defect itself. It adds none, and no `-Woverflow` remains.
   - Every other `SRID_MSB` use either sits in a Boolean context (`if`/`while`/`FATAL_ERR_IF`) or
     normalises through `&&`/`!`, so no other mask is narrowed through `int`.
   - On the restoration path, `restore_wps_for_user_deleted_clauses`, the range array and
     `clauses_lui` are all `srid_t`/`ullong`.
3. **Linked width probe** (real `global_data.o`, synthetic offsets 0 … 2^31±k, 2^32, 2^40, 2^62, LLONG_MAX−16).
   It exercises `is_clause_deleted`, soft delete/undelete and clause size.
   - Fixed64, fixed64 under ASan+UBSan, canonical32 and fixed32 each pass 154 checks (66 for the 32-bit builds).
   - Unpatched64 fails 28 checks, which reproduces the defect.
4. **Rank147.**
   - Unpatched64 rejects at `[line 111] No UP contradiction for RAT clause 731`.
   - canonical32 and fixed64 both verify and produce identical binary LSR `e9d97cd1…`.
     Both compressors yield text LSR `67cfd918…`, which Trestle verifies.
   - Forward outputs are identical across the two builds.
   - 30 seeded mutants (drop-add, hoist/sink-delete, flip, strip-witness, truncate): canonical32 and
     fixed64 agree on all 30 in status, exit code and LSR bytes. 16 verify (Trestle verifies all 16) and 14 are rejected.
     Unpatched64 disagrees with canonical on 16.
5. **Directed controls v2** (1755534): ASCII and binary encodings, backward and forward modes, 26 cases.
   - Restore-path positives (binary RUP, ternary RAT, witnessed SR, derived chain with a three-deletion
     group, delete and re-add, duplicate input): unpatched64 rejects or errors on each, which proves the
     path is reached. canonical32 and fixed64 both verify, and Trestle verifies the LSR.
   - Duplicate-lemma multiplicity also verifies end to end.
   - Retained negatives (use after delete, a later-deleted clause as RAT/SR candidate, re-add after use,
     and satisfiable bogus proofs) are never accepted end to end.
6. **Differential corpus** (1755483). The proofs come from 113 generated and corpus instances
   (random 3-SAT, PHP, Ramsey, parity, 52 regression CNFs), with kissat and tactix as producers.
   - 195 proofs: 20 witnessed, 169 with deletions. Plus 392 mutants, for 587 cases in total.
   - canonical32 and fixed64 agree on every case in status, exit code and LSR bytes, in both modes.
   - Unpatched64 rejects 340 canonically valid proofs and accepts no canonically invalid one.
   - The exact `runtime_task.check_unsat_proof` acceptance code ran 360 times with both pin sets.
     It agreed every time. Witness-free proofs always took drat-trim+cake_lpr and never dsr-trim.
     Every witnessed acceptance ended with srcheck: 59 verified, 27 not.
7. **Proof-oracle selftest.** `tools/fuzz/selftest.sh` printed `SELFTEST OK` under both frozen collections
   (`collections/canonical32` and `collections/fixed64`, with dsr-trim hashes confirmed in the logs).
   This covers the lie-unsat-as-sat and negate-model model negatives, truncate-proof, the DSR/DRAT routing
   controls in `test_fuzz_proof_oracles.py`, and the every-lemma controls.

No checker acceptance error, no accepted retained negative, and no canonical32/fixed64 divergence was observed.

## Defect handed to root (bounded reproducer)

The provenance constant `FIXED_LONG_DSR_PROVENANCE['repository_patch_sha256']` in
`tools/bench/experimental_tierd.py:43` is a **62-hex** string, missing `2b` after `…e2a1b`.
It is not the patch hash, and no code verifies it.

```
python3 -c "import sys,hashlib;sys.path.insert(0,'tools/bench');import experimental_tierd as b;r=b.FIXED_LONG_DSR_PROVENANCE['repository_patch_sha256'];a=hashlib.sha256(open('tools/checkers/patches/dsr-trim-0b8cc0e-forward-elaboration.patch','rb').read()).hexdigest();print(len(r),r==a)"
# -> 62 False   (actual edd70e504e573fbf9801a1c9f80a05fae04e2bcadd763a1e2a1b2b923df3afc6)
```

- The same truncated string appears in the frozen fix-run `manifest.json`, the Rank147 report, and the
  `dsr_checker_provenance` of the launched `20261007-fixed44-b26ef-cooperative-python{,-retry01,-retry02}` manifests.
- The enforced bindings are correct: source.tar, fix.patch, result.json and both pins, plus each manifest's `pin_sha256`.
- The binaries were reproduced from the verified source, so acceptance results are unaffected.
- Required correction (metadata only, owned by root): fix the constant. Preferably also hash-bind the patch
  file in `files` so the field is verified rather than recorded.
- Already-launched manifests should be read with the 64-hex value. This review does not edit them.

## Not tested / stated separately (not conditions on this bounded fix)

- **Beyond 32-bit capacity.** No proof with a lits_db offset ≥ 2^31 or a clause/line count ≥ 2^31 was checked.
  That range is exercised only by the synthetic width probe.
  - The Rank147 deleted value is offset 21450, which is in range.
  - Pre-existing int-typed allocators remain: `clauses_lui_alloc_size`, `lines_RAT_hint_info_alloc_size = formula_size*2`,
    and lit_occ int indices. They overflow near 1.07e9 clauses, independent of the fix.
  - Large-proof wall time and memory for long64-fixed are unmeasured.
- **Pre-existing upstream behaviour, identical in canonical32 and fixed64.** `dsr-trim` backward mode prints
  `s VERIFIED UNSAT` for some invalid proofs:
  - the satisfiable `[1 2][-1 2]` with `-2 0 / 0`, with or without a padding line;
  - 4 flip/drop mutants in the corpus.
  
  Trestle rejects the elaborated LSR in every such case (forward mode also rejects the directed
  ones; forward mode was not checked on the 4 mutants), so the campaign pipeline
  (which always ends in srcheck for witnessed proofs) never accepts them. Trestle remains the authority;
  never treat a dsr-trim verdict alone as acceptance.
- `1L<<63` (`MSB64`) is signed-shift UB in ISO C but defined by GCC. The sanitizer runs excluded only `shift-base`.
- One raw-LSR Trestle run (`reg-039-walk-near-unit-base-case`, tactix base proof) ended in `error`
  under the job's single OOM-kill event. It is UNCHECKED, not rejected.
  That proof's real campaign route, DRAT/Cake, verified it under both pin sets.
- The v1 directed controls (1755483 `directed.json`) are superseded by v2. Their fixtures closed the
  refutation by unit propagation before the deletion.

## Artifacts

`manifest.json` lists every input and its hash, and `SHA256SUMS` covers the run scripts.
`launch.json` holds the attempt history. Per-job results are in `results-<job>/`,
the logs are `slurm-<job>.log`, and the frozen checker collections are in `collections/`.
No production code, pins, submitted campaigns, or git state were modified.
