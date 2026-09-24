# AM356: current-pin recheck of two late-LRAT handoffs

This is bounded diagnostic evidence, not combined N2 or fuzz acceptance. No
solver, proof adapter, checker, or rule changed. The AM349 Opus owner remains
responsible for its terminal campaign verdict.

The AM349 H4 handoff's original 13-variable, 54-clause CNF has a repeated
literal in input clause 6, `8 10 3 11 8 -7`. Its immutable AM349 Release
driver (SHA-256 `8ca0e72253f578c0895891ed709cd6a8627597f1fb9ed59f0e148b6818cfe8c6`)
was run on `cpu-dense-hwulab-q` in job **1717340**. It returned UNSAT, but
pinned CakeLPR `ab9dc6c4...` reported `Checking failed at line: 9`; its later
line 10 cites raw input ID 6. This is a real rejected certificate, but the
first rejection is earlier than the duplicate-literal hint named by the
handoff. The old source commit `b7b758db` predates the general C-API late
input replay in AM350.

A small public C-API driver linked against the current AM355 Debug object
build (CLI SHA-256 `c4f95a0fc07283dd73b0fa2c9bc4d935632dc67f84b8e6d55f429c8919e740be`;
driver SHA-256 `1791d7a8238e81ec5bc5b43e24bc71c862d6fdafdd4eecbc4f7b7f0983fcbae2`)
imports the *same original CNF*, attaches native LRAT either before the first
clause or after the last clause but before reasoning, and applies the public
canonical `Congruence` action. The four independent SLURM jobs below ran with
one CPU, 8 GiB and a ten-minute limit, without a node constraint. Each
returned UNSAT/20 and printed CakeLPR's exact `s VERIFIED UNSAT` verdict:

| Bounded handoff / timing | Job | LRAT SHA-256 |
|---|---:|---|
| H4 duplicate input / early | 1717342 | `4561edf5ad0917b0320c8d7994ebe75461db91201d974f26ad6e884115dfb109` |
| H4 duplicate input / late | 1717343 | `6e8ebd3470516608df8995559736e9df53b40b9d32189de2b22c0b63f2611096` |
| H1 unary cancellation / early | 1717346 | `55ac8ab6ee2265ff960a8939e6a9bfd49378a4574e8a055b87b35958b152cdfa` |
| H1 unary cancellation / late | 1717347 | `91ca811d0ae007617eff9c4270edb2dd28fcc50a26aa552de8fdceabaceb9a93` |

For H4, the current proof begins by deriving the distinct-literal version
`8 10 3 11 -7` from original ID 6. The late path derives a second, redundant
but valid copy when reconciling the eager session with the proof replay. All
four proofs were checked against their **unchanged original CNFs**, not a
deduplicated checker input. The public driver, proof files, SLURM logs and
old negative control are preserved locally under
`benchmarks/probes/scratch/am356-h4.KFk3Ke/`; the immutable handoff CNFs and
old driver remain in AM349's bounded handoff. `sacct` reports all five jobs
COMPLETED/0:0. The checker success line, rather than job exit alone, is the
acceptance condition.

This closes these *two canonical current-pin reproductions* only. It does not
establish all late-attachment semantic instances, the AM349/AM351/AM352/AM355
independent fuzz verdicts, complete proof/model coverage, N1--N4, or any tier
gate. In particular H4's separately handed-off semantic `idx142` remains to
be checked on a current pin. No Tier D job was submitted.
