# Rank 1129 proof-route diagnostic

## Outcome

This is a frozen negative result.  Rank 1129 remains open under both the
fixed-target and 1,800-second regression acceptance rules.  Passing the binary
LSR emitted by `dsr-trim` directly to Trestle avoids the 6.074-second
`dsr-compress -d` stage and the 844,384,806-byte text expansion, but Trestle
still times out.  There is no checker-route change and no target recovery.

The exact retained Tactix certificate is 230,735,672 bytes with SHA-256
`5331268715736a7895ff07e2bfe6c5951af42e2959e04935387f5404b3f260d7`.
Both diagnostics used the exact checker CNF with SHA-256
`7e5fa9f68988f676a75934bbb05776746e09317f3d07205c67ca529283373e59`.
This is the same certificate and checker spelling audited against the frozen
target result; the solver was not rerun.

## Checker boundary

| SLURM job | Route after `dsr-trim` | Trim | Retained core | Expansion | Trestle | Total |
|---|---|---:|---:|---:|---:|---:|
| `1750736` | binary LSR -> text LSR -> `srcheck` | 323.919 s, verified | 3,447,844 additions; 380,994,051 bytes | 6.074 s; 844,384,806 bytes | timeout after 1,430.159 s | 1,800.158 s |
| `1750754` | binary LSR -> `srcheck ... c` | 337.955 s, verified | 3,447,844 additions; 380,994,051 bytes | omitted | timeout after 1,421.598 s | 1,800.154 s |

In both runs `dsr-trim` parsed 17,940,417 proof lines and 3,448,380 additions,
derived the empty clause after retained addition 3,447,844, and independently
reported `VERIFIED UNSAT`.  Trestle parsed the 42,048-variable,
11,135,080-clause formula and then exhausted the remaining deadline without a
verdict.  The 13.0-second difference between the two trim timings is node/run
variation; the direct route's deterministic saving is the removed 6.074-second
decode.  That saving is immaterial beside the semantic check.  The causal
boundary is therefore the 3.45-million-addition retained core, rather than
binary-to-text conversion.

Promoting a direct-binary route would change checker plumbing without changing
this row's disposition.  The existing checker route remains authoritative.
Further work on this certificate should reduce the retained semantic core or
replace the proof mechanism, then pass the required independent checker.

## Compact-proof mechanism lead

The frozen LymphoSAT comparison for the same exact instance solved UNSAT in
68.338 seconds and emitted a 1,006,567,937-byte PBP certificate.  Its required
independent pipeline verified in 1,340.631 seconds: VeriPB took 1,183.965
seconds and CakePB-CNF took 156.666 seconds.  Peak RSS was 8,272,476 KiB and
25,516,148 KiB respectively.

The frozen LymphoSAT bundle has a dedicated `clique-coloring` slot named
`clique-color-pigeonhole-veripb`.  Its documented UNSAT construction recovers
clique-slot and vertex-color one-hot blocks, introduces proof variables saying
that a clique slot has a color, derives occupancy and pairwise color exclusion,
and emits a short pigeonhole refutation.  The retained comparison row records
the PBP certificate and checker timings but does not retain the selected
internal slot in its result schema.  The exact row plus the frozen bundle
therefore establish a clique-coloring/PBP mechanism lead, not a transferable
certificate attribution.

The smallest constructive follow-up is a source-bound structural preflight for
this family and a compact Tactix startup PBP producer, followed by independent
VeriPB and CakePB-CNF validation on the exact target.  It must not be counted as
recovered until the Tactix-produced certificate passes those checks.  This
diagnostic does not change Tactix's current certificate semantics.

## Frozen evidence

[`evidence.json`](evidence.json) records the exact identities, job results,
stage counts and historical PBP reference.  Its source result hashes bind the
otherwise mutable run directories.  The raw diagnostic JSON SHA-256 values are
`a6c2143456af350644e4d1103abf6e447d5ac29074fccf0450d3d7c7815e0fca`
for job `1750736` and
`817eb976854b748c44d86c7de0659f03bb48984f49c49a51d24920a89a22bdbc`
for job `1750754`.
