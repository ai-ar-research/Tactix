# FINAL REPORT CLOSEOUT CLARIFICATION — shared-policy preparation fuzz, pin 375b621b

Owner: Claude (fuzz owner), same session. Written 2026-10-03.

This document only reports on existing evidence. No solver run, job,
experiment or diagnosis was performed for it.

The delivered reports are unchanged, with these sha256 values:

| Report | sha256 |
|---|---|
| `FINAL_REPORT.md` | e648d19f65c7ba255cb8bd4b0e6f89adda373848940acb090d946e3ceacd61d2 |
| `FINAL_REPORT_ADDENDUM.md` | 8a721895dfbb419ecb27dad91b2d8c4ca5a21ba2117cb63d9406a784b9dd64f4 |
| `FINAL_REPORT_CLOSEOUT.md` | 1eb5b3aab3b40b0a27cfa6c145b74c963299a3f99ec531ed45df44f5f0e4ed71 |

Where this clarification and the closeout differ, the clarification states
what the evidence actually supports. It does not redefine acceptance.

## Summary answer

**One original requirement remains unproven.** The 19,084 original
mining-scan completions and the 6 original d2 raw completions never received
an independent proof/model check of their own artifacts. Those artifacts were
not retained, and nothing retained (proof hashes or bytes) ties a lost
original certificate to any independently checked artifact. So:
- **Under the strict original requirement** — per-artifact independent
  disposition, and for C3 explicit artifact correspondence — **those
  requirements remain unproven.** The strict disposition is **NOT ACCEPTED**.
- **The closeout's ACCEPTED is a scoped replay-replacement disposition**,
  defined in §1 and §4. It does not establish that the lost bytes were
  independently checked.

Codex owns the decision on whether the scoped disposition suffices for the
whole objective.

## 1. C3 (and C1): what correspondence evidence actually exists

### Retained original evidence for the d2 raw runs (job 1747244)

Source: `diag/d2/rows.jsonl`, sha256
15110bde9a3a24af39b24e7dadea45546100266b66289c7a46bdaa0c32fbcd3f. It was
frozen into the closeout snapshot as `d2-original-rows.jsonl`, byte-identical
(same hash).

What each configuration's row retains:
- **For the raw off and raw on runs:**
  - exit code;
  - wall time;
  - the s-line;
  - the final `c conflicts`, `c decisions` and stress-fire counts, parsed from
    the solver's stdout.
- **For the on feedback stream:**
  - the maximum conflicts, decisions and stop-fuzz fires over all records;
  - the line count;
  - the record count;
  - the session_end verdict and stop reason.
- **Comparison flags:**
  - `same_s_v_proof`: raw off and raw on had equal s/v lines and equal proof
    bytes, compared in-job before deletion;
  - `diverged_conflicts` and `diverged_fires`, both false.
- **A separate third run** (`on_graded`): verdict UNSAT with no failures. It
  was a different execution, and its proof was **not** compared to the raw
  proofs.

What is **not** retained:
- any hash or byte of a raw proof;
- any raw model or s/v-line text beyond the s-line;
- the raw feedback stream itself.

There is **no retained original proof fingerprint**.

### Owner session observation (not a retained artifact)

While d2 was running I listed its temporary directory. It showed raw proof
file sizes of 11,866,676 bytes (gen-2042000015 n50, off and on) and
11,133,825 bytes (gen-2042000015 n500, off and on). That observation is in this
session's transcript only, not in any file. It covers no size for gen-25 n50,
and file sizes are not content fingerprints.

### Other runs of the same configurations

Earlier, independently graded runs of the identical off/on invocations also
exist:
- **f1** `ts-default-*`: off graded and verified for all four (instance, n)
  pairs.
- **d1** (1747211): off and on graded and verified for gen-25 n50 and n500;
  gen-15 off graded and verified, gen-15 on timed out.

Their rows retain verdict and checker statuses only. They hold no proof hash,
so they cannot be byte-linked to the d2 raw runs.

### What the closeout demonstrated (closeout job 1747299)

**(a) New-twin proof equality: demonstrated.** In each configuration, graded
off and graded on produced byte-identical proofs, and both verified:

| Configuration | Proof size (both runs) |
|---|---:|
| gen-25 n50 | 2,173,327 bytes |
| gen-15 n50 | 11,866,676 bytes |
| gen-15 n500 | 11,133,825 bytes |

**(b) Retained original fingerprint match: demonstrated** for every retained
original field:
- the off and on s-line;
- conflicts, decisions and stress fires;
- the on stream's maximum counters, exact line count, exact record count and
  session_end.

These are coarse execution fingerprints of the trajectory, not of proof
content.

**(c) Determinism: inferred, not proven per original artifact.** Evidence
that the off/on invocation is deterministic on this pin:
- (a) and (b);
- the matching graded runs in f1, d1 and d2's third run;
- the C1 twin results: 19,100 byte-identical A/B twin pairs across stage 1 (19,096) and
  stage 2.

From this I infer that each lost raw proof equals its rerun counterpart. That
is an inference. **Nothing proves that a lost raw proof equals an
independently checked artifact.**

### Strongest supported dispositions

**C3:** each of the 6 original raw completions is replaced by an exact graded
rerun of its invocation that:
- verifies through the canonical drat-trim + CakeLPR + forward-audit route;
- has twin proof equality (off = on);
- matches every retained original fingerprint exactly.

The C3 request asked for "explicit artifact correspondence" and forbade
reliance solely on off/on equality or a separately graded third run. That
**strict correspondence cannot be established, because no original artifact
fingerprint was retained. It remains unresolved.**

**C1:** the request explicitly permitted the full-schedule replay. The
correspondence available there is narrower still:
- retained per seed: the original exit code (10 / 20 / None) and the
  LocalSearchGuidance/Propagate hit counts (`targeted/out-mine/rows.jsonl`,
  sha256 bd29c71420390b22c431343135cfe5e0aa654df6f1e8112d1914430897d77968,
  frozen as `mine-original-rows.jsonl`, same hash);
- not retained: any proof, model or stream.

All 19,084 original completions now have two graded, byte-identical twin
reruns, with verdict and hits equal to the original. The lost original
certificates and models remain **not independently checked**. This is the
permitted replay disposition, with the same inference limit as C3.

## 2. Exact provenance (full SHA-256)

### Solver, sources, checkers

| Item | sha256 / binding |
|---|---|
| Source commit | 375b621ba6677600f3e77bf8383af25d256d7b67 |
| Pin `build/pin-shared-policy-feedback-debug-375b621b/tactix` | 95197f8c9f468bd3b121b1cd66d65457b9d5baade62bffb27f95e82caff593c2 |
| Pin `PIN.json` (173 binaries, 795 sources) | 72a3a25461ab2a1974b6c16e723e54e87734fdcd14e5bfbddf1676f96cf3c136 |
| Checker manifest `SHA256SUMS`: owned pin `build/pin-fuzz-checkers-shared-feedback-375b621b/` = installed copy `tools/fuzz/failures/checkers/895f1989…/` = snapshot `tools/fuzz/core-checkers.sha256` | 895f1989468d5e01a81fa647f9cb9739448b81507a4b78515e4e46a7441c44f8 |
| cake_lpr | ab9dc6c43ad0f55bbaf336c5d898919ddb03d0bdb75e88dff48237fb06ba1f57 |
| drat-trim | f2cb7d45d6acc05b1295bfa0d27b53d75fa48259c2b1e15f15772f064f99849a |
| dsr-compress | 6bc416a0df73f16911a0b16d85aa4bed4d15e4915d309407ff1e80b07d19d676 |
| dsr-trim | 3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee |
| lrat-check (diagnostic only) | 0c9a87d5cfb8851350928303094cb7d36317d47786dda243ebcc6250d6eec6d7 |
| srcheck (Trestle) | 19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945 |
| Snapshot `tools/fuzz/core-checkers` file (content: the installed path above) | e45e30c0e2aae926b59041257d59811bbf681d4be4f0df3c8464304eb42266cc |
| Snapshot VeriPB / cake-pb-cnf | 2e5fa5ef07c655640e6fff0d3f41561eab1502ec2e48766cca60287625935674 / c859a9c926ca8c63dac3317930ea8dbe3732a6fc24faa6e28ed740fadd67dc47 |
| Differential oracle `tools/solvers/bin/kissat` (resolves to `kissat/build/kissat`) | 3aa8e41f976e213d3c4627150e9cd4e5fce0ce9508d67ab469a7696e26a8d49c |
| Maintained `fuzz.py` / `generators.py` / `modelcheck.py` / `proof_oracle.py`, inside the snapshots | 125a71bd6f0c04e98dc27014ac4dd902c5432c5e3e225d951ee4c5bd76f36bbc / 489bfdabebb05f9498dc1dbcd3350e2acddbe48d373fec40bda329986cdc21ba / 023d06f8f4016ced484897a4a6aeadf2fdb45e16dbf929661b1c6e92ec0ea9d4 / 020bfcef6f4382e9b3136442a253bb7eb4fcfa1091be9d692ddd438161a3b8b2 |
| Leg harness aggregate (replay copy `tools/fuzz/failures/harnesses/1fceb82e…/fuzz`) | 1fceb82e94b70bb57ec60d7b6e69b6cb55eecb155d974b0059be649e17cb5fc3 |

### Owner snapshots

Each digest is computed as `find . -type f ! -path ./SNAPSHOT_SHA256 -print0 |
LC_ALL=C sort -z | xargs -0 sha256sum | sha256sum`.

| Snapshot | Digest |
|---|---|
| s1 (smoke) | 710ff4438b3f581d3a8b582fe7e8c01cdf0da8753fefce4bc37ff8806c75db1e |
| f1 (targeted, mining, d1, d2) | f538b1f895ebc92306493c42e429138e74cef9af438774fc03c16e52af89595b |
| c1 (closeout) | 047864a8818335eebd6a3c1779c64069c5e125badb80b64a05d4a24447597a20 |

### Closeout and owner scripts and tools

| File | sha256 |
|---|---|
| `CLOSEOUT_PLAN.md` | 9f4da453552f9bb225445d4660d8d133d37211c4e663c4cd024ceaa1a58fe3a8 |
| `closeout/closeout.py` (= `snap-c1/closeout.py`) | 26cf9879c55a79db7670bfda177e1649910da51e8a8f1fb7aabbdbf2ba258bb8 |
| `closeout/job.sh` (= `snap-c1/job.sh`) | 01314eaca092532953cdc5ae6da998eb2275652a55fcdc735dd4983c795f8707 |
| `closeout/make_snapshot.sh` | 8f7fa191f81be3e64a1655d964c52aedb8982397536303b6464e0dd9b190bc49 |
| `closeout/plan-freeze.sha256` | 5a204dc692cd1945f3e2e0cd333a5889f7a546e5603ff0c9711151e784ec01fe |
| `closeout/postflight.sh` (the terminal C2 postflight) | 5d85bde9720c3e48cd2089ad02193be1ef8ddb766bea74b3a2598e06d38c40f1 |
| `closeout/tally.py` / `closeout/census.py` (analysis only, no solver) | 6492b4fe6a26a7c9a46abdb86dc04bb26acdfc3e8518f9d145c0255578c3f1e5 / b8c868b7c84e7c2f3c3e555fada78a971f2fce0da1fc4a6089070a97a92c00bb |
| `tools/verify_pin.py` (= `snap-*/verify_pin.py`) | 818b26fce7b37ab00d9d9ceaaa8dac491ab5beb6030572758aa386e2986fa7ae |
| `tools/leg_accounting.py` / `tools/addendum_census.py` | a41db0758f4df52624f821a98b01289749a38e2e5e673082b657e9e0959245b0 / 48847823d38346a28d45c0c251301d30d85e89b9e7a5a8cb72ec3818861d412e |
| `fbdrv` (= `snap-f1/fbdrv`) / `fbdrv.cpp` | d6b56a532fee61349b2513dcc838d8fd16f4163132190412973d06accc6d994b / c1dd10489ab3a4d9400e04a49343fe075e520866dd2ff7ced1a353255d7b60ed |
| `fbdrv-ctl-f6f01982` (causal control only) | a3ba600bff48619506b5bab57fa281374702303bffec1162ef4c0db27326760d |
| `cfgdump` / `progcheck` | a2c0e2e9677c3d0f48b382225cc4daec1698116e6c3493c2467f297575ff52de / 9b2de8f9df7b539ffad9e2ee1ff46430b2d926f13e5acb806298d2787f3c87e6 |
| d1 `run.py` / `job.sh` | 6e4401f52e2ca758730d7f442db3c7e5f87bb12942a872fce3243a19f2bbd2dc / 771d898ab1b13618812c420bd73033b9ee8158267af43beec34a1b0a46b4bc58 |
| d2 `run.py` / `job.sh` | a585de8fa2e2f65afccbb215b40f04046d0bce39dac396b1f1f2d3cdaa142021 / a1ba51578e09a13bd1f83fd549832746f7f85b16ddaa948d5d5c5197c74949e8 |
| Snapshot `controls/stopfuzz-env.sh` | 97050985ce03723478306a0790245378fdead3bced4ebda41a06b8c56576a2a6 |
| d2 input `gen-2042000025-mut-n90-m984.cnf` | 3b83ece395afa69dae27dd98202d2981a525f29ea617f1abbb7eb6ba2adaf101 |
| d2 input `gen-2042000015-ksat-k5-n67-m1880.cnf` | 5697a9dbe151e0f8a47a89b04bc75f62befb78d638f5dec70f0d80138643c67d |

The postflight script is hashed here as it exists now, after it ran. It was
made read-only before it ran.

### Exact invocations

**C1 original mining scan** (`snap-f1/tgt_runner.py` `task_mine`) and the **C1
replay** (`snap-c1/closeout.py` `remine`):
- **Index and seed:** for i = 10..20009, seed = 2041000000 + i. These are the
  L2 leg's own seeds.
- **CNF:** `name, nv, cl = generators.generate(seed, i)`, then
  `write_dimacs(CNF, nv, cl)` (snapshot `generators.py` 489bfdab…). The CNF
  file name differs between the original and the replay; the content is
  identical by construction.
- **Command:** `<pin>/tactix --controller-policy=<seed> --feedback=<F> <CNF>
  <PROOF>`. No other flags; native DSR is the default proof format.
- **Budgets:**
  - original: 30 s via `fuzz.run_cmd`, ungraded;
  - replay: A and B each via `fuzz.run_case(CNF, [tactix,
    --controller-policy=<seed>, --feedback=<F>], kissat, 30, False, wd)`;
  - stage 2: identical, with 600 s.

**d2 raw runs** (`diag/d2/run.py`) and the **C3 reruns** (`snap-c1/closeout.py`
`d2re`):
- **Wrapper:** `snap-*/controls/stopfuzz-env.sh N` runs `TACTIX_STOP_FUZZ=N
  exec <pin>/tactix "$@"`.
- **Configurations:** (gen-2042000025-mut-n90-m984, N=50),
  (gen-2042000015-ksat-k5-n67-m1880, N=50) and
  (gen-2042000015-ksat-k5-n67-m1880, N=500).
- **Off command:** `stopfuzz-env.sh N --auto-proof <CNF> <PROOF>`.
- **On command:** `stopfuzz-env.sh N --auto-proof --feedback=<F> <CNF>
  <PROOF>`.
- **Budgets:**
  - d2 raw: `run_cmd`, off 900 s and on 3600 s, ungraded;
  - d2 third run: `run_case` of the on command, 3600 s;
  - C3: `run_case` of off (900 s) and of on (3600 s).
- **Inputs:** the input CNFs are the f1 pool files. They were generated by
  `generate(seed, i)` with seed = 2042000000 + i (i = 25 and i = 15), the
  targeted generator-pool window. They are hashed above.

**In-job gates** (`snap-c1/job.sh`):
- the snapshot digest equals its recorded value, before and after the run;
- `python3 verify_pin.py` before and after;
- `cd <installed checkers> && sha256sum --strict -c
  tools/fuzz/core-checkers.sha256` before and after;
- the maintained selftest for the remine profiles.

**Terminal postflight:** `bash closeout/postflight.sh` (5d85bde9…). The
outcome was `postflight status=0` at 2026-10-03T08:42:32-04:00.

## 3. Timeout attribution (correction)

The closeout's line "it accounts for the 6 non-evidence feedback-variant
timeouts" overstates the evidence. Corrected:
- **Four CLI stop-fuzz feedback-variant rows have diagnosis evidence.** These
  are `ts-default-n{50,500}-gen-2042000025-mut-n90-m984` and
  `ts-default-n{50,500}-gen-2042000015-ksat-k5-n67-m1880`.
  - d1, d2 and C3 show that with longer budgets the feedback-on runs complete,
    with deterministic counters equal to the off run, s/v lines equal and
    proofs equal (proofs in d1-graded, d2-raw and C3-graded forms).
  - That attributes their f1 120 s timeouts to recording overhead.
- **Two TF feedback-variant rows remain undiagnosed:**
  `tf-default-gen-2042000025-mut-n90-m984` and
  `tf-default-gen-2042000176-ksat-k5-n58-m1292`.
  - In each, base was UNSAT and verified at the 30 s budget; the feedback run
    timed out at 150 s; the replay was not run.
  - They had no stop-fuzz, and no diagnosis covered them. The
    gen-2042000025 CLI diagnosis used a different invocation (stop-fuzz), so
    it does not transfer.
  - Their cause is **not established**.
- **All six remain non-evidence and are never passes.**

## 4. Original-requirement disposition (explicit)

| Original requirement | Status |
|---|---|
| Every **new** completed candidate semantic execution fully graded | **MET.** C1 has 40,008 graded runs and the labelled smoke 48. C3 has 6 graded runs. 0 checker timeouts, rejections or errors, and 0 failures. |
| Every completed SAT/UNSAT has an independent model/proof disposition: **original** mining scan (19,084) and d2 raw (6) | **UNPROVEN for the original artifacts**, which were lost and never checked. Replaced by graded replay (§1), within the scope stated there. |
| C3 explicit artifact correspondence of lost raw proofs to a checked artifact | **UNRESOLVED.** No original fingerprint was retained. Only coarse trajectory fingerprints match, and determinism is inferred. |
| C1 deterministic correspondence where retained evidence permits | **MET to the extent permitted:** verdict and hits only. No certificate correspondence is possible. |
| C2 terminal pin / source / checker / snapshot / harness postflight after all jobs | **MET** (postflight status 0). |
| d1/d2 historical in-job pin/checker/snapshot gates | **ABSENT** (historical gap, retained). The terminal postflight does not create them. |
| Fresh 100,000 seeds per policy; R1–R13 / TW evidence | **Unchanged.** Established on the same pin, checkers and f1 snapshot, all verified unchanged at the terminal postflight. |
| The two undiagnosed TF variant timeouts; Debug feedback overhead; unavailable native producers (Condition witness, native SR, LocalSearchGuidance/Propagate effects from mining only, Factor terminal arm unreached) | **Explicit limitations, not relaxed.** |

**Disposition statement.**
- The closeout's **ACCEPTED** holds only as the scoped replay-replacement
  disposition defined in §1.
- Measured against the strict original requirement — independent disposition
  of the original completions' own artifacts, plus C3 artifact
  correspondence — the requirement **remains unproven**, so the strict
  disposition is **NOT ACCEPTED**.
- No solver, proof, model or checker failure has been observed anywhere in
  this campaign.

Whole-goal acceptance, including whether the scoped replay disposition
suffices, is Codex's decision.
