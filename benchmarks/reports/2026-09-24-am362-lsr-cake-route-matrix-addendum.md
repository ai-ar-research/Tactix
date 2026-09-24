# AM362: DSR/LSR/Cake route matrix and ncc hash correction

Date: 2026-09-24. This addendum supplements the frozen [AM361 ncc audit](2026-09-24-am361-lucky-and-ncc-terminal-audit.md). It changes no solver, checker, production route, or N1--N4 verdict. Fixed microtests ran on current Debug `build/am360-debug/tactix` (SHA-256 `c48e7fd798d68472ebb4cf8b013826a160a1bfc3e86c0ab977c485040c8eebb5`); local artifacts are retained in `benchmarks/probes/scratch/am362-lsr-cake-matrix/`.

## Provenance correction

The AM361 report's displayed ncc proof SHA-256 omits one `c2` near the end. The authoritative AM343 `results/002.json` field `certificate_sha256` and the frozen AM361 diagnostic script agree on **`4620a6e8d0647bc381da13286235c8ae5b0e2d753e3fcb12488611c2c2c9f84d`**. The AM343 result JSON hashes to `6912103bb1255bf1b664ba125cb1b4e7270b819281b144216f8a0734ae12399e`; the queued script hashes to `bb1187072e17d6a59356ddbe238d392b7064199dfd3d67108b101616b86b6f5f`. Neither frozen artifact was edited. Job `1717619` was still PENDING (Priority) at this addendum; no result is inferred.

## Bounded compatibility matrix

The matrix uses pinned `dsr-trim` (`3b5c7737...`), `dsr-compress` (`a44102ce...`) and CakeLPR (`ab9dc6c4...`), byte-identical to the frozen AM361 diagnostic pins. Every reported acceptance is against the original CNF and requires the literal status `s VERIFIED UNSAT`, not only a zero exit code.

| Fixed fixture | DSR class | Elaboration and independent check |
|---|---|---|
| Sweep regression 031, seed 296485266 | ASCII, witness-free | `dsr-trim` verified; emitted LSR accepted by CakeLPR. |
| Sweep regression 033, seed 296326708 | ASCII, witness-free, proof `f0cf8f2e...` | `dsr-trim` verified; emitted LSR accepted by CakeLPR. |
| Same regression 033 and seed | Binary, witness-free, proof `4971af16...` | `dsr-trim` verified binary LSR, `dsr-compress -d` decoded it, and CakeLPR accepted the decoded LSR. This exercises the exact format chain proposed for ncc. |
| Two-variable, four-clause contradiction `enum2.cnf` | ASCII, **witnessed SR** | `dsr-trim` verified; CakeLPR accepted its LSR. This is not witness-free route evidence. |
| Sweep regression 032, seed 296305338 | ASCII, witnessed SR | Classified only; deliberately excluded from the witness-free route test. |

The witnessed `enum2.cnf` proof exercised SubstitutionRedundancy in the current solver; its successful LSR check does not turn it into DRAT. The already-retained AM361 invalid empty-claim control was rejected by CakeLPR with `Checking failed at line: 0` despite process exit 0, reinforcing the exact-status requirement.

These are small compatibility controls, **not a general performance or correctness proof** for witness-free DSR. The queued ncc row is a 396 MB certificate; AM343 `drat-trim` and AM324 native DSR/Trestle each exhausted the checker window on that identical proof. The production route and 100/100 WV100 gate remain unchanged. If job `1717619` independently verifies the original-CNF ncc proof inside its allowance, propose a general content-based checker-route change under the maintained plan and validate it across witnessed, witness-free, negative, layout and timeout cases before using it for any gate.
