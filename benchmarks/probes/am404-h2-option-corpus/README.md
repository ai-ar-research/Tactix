# AM404 checker-only H2 option corpus

This is a frozen diagnostic, not a production proof-route change. `run.sbatch`
was submitted as SLURM job `1721075` on `cpu-dense-hwulab-q` (2 CPUs, 8 GiB,
30-minute limit), and completed 0:0 in three seconds. It executes **no solver**.
The script uses the AM369 pinned `dsr-trim`, `dsr-compress` and Trestle
`srcheck`; `dsr-trim-option.sh` adds only `--delete-implied-units`. The oracle
canonicalizes the original input's checker spelling, checks the reference
backward path, then runs forward `-f` every-lemma checking and independently
verifies the LSR with Trestle in each mode.

Inputs are the exact H2 public handoff, the two AM401 fixed H1 proof/CNF pairs,
the terminal AM397 observer DSR pair, and local positive/negative option
controls copied semantically from `tests/micro_proofs.sh` / AM390. Every input
and checker pin is SHA-256 recorded in `out/1721075/`. The run's
`results.tsv` and per-case `check.log` files are the verdict authority;
accepted cases also retain generated LSR and canonical checker CNF. The exact
68-entry job output tree is also preserved in tracked `job-1721075.tar.gz`
(SHA-256 `d797326832b9371f4035e52f20e438af65251107084331593b42abc55d71e5d7`),
so generated logs remain byte-identical rather than being whitespace-edited.
The eight archived external input/proof files are preserved separately in
`inputs-1721075.tar.gz` (SHA-256
`e000c5243e8bede112eafa7523a702afeab2d7e67a01196d81bad9be5ffdd0ac`);
its relative paths match those used by the frozen job script. Checker binaries
remain pinned by hash in the AM369 local run, not duplicated in this bundle.

Expected and observed status matched for all seven pairs of modes. Under the
option, H2 and the deletion-dependent valid control verify, while a proof
without the deletion and an invalid witnessed unit reach Trestle and are
rejected as `s INVALID`. This is a bounded diagnostic; see the AM404 technical
report for interpretation and the unchanged N2 gate.
