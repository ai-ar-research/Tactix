Original pair 1751752 remains frozen FAILED. No original file was edited and no solver was rerun.

The pinned executable is lakefile.lean's Experiments.SR.Main, not the older Experiments.ProofChecking runner. Its ASCII and patched binary loops both explicitly accept nonrefuting valid streams. The failure is ASCII trailing whitespace at EOF: Main.lean:44 tests the unskipped iterator; readNat then consumes the newline and returns ID0 at EOF; the pseudo addition reaches parseLSRAdditionLine and fails "Line ended early" (SRParser.lean:148). The original final newline is sufficient to trigger this after every otherwise valid prefix. It is not a rejecting proof lemma.

Exact isolation, for both arms, with the pinned srcheck:
- First full LSR lemma with newline: exit0/sINVALID.
- Same first lemma without newline: exit0/sVALID.
- Entire retained stream with only trailing whitespace removed: exit0/sVALID.
- Entire original stream losslessly compressed, binary `c` reader: exit0/sVALID.
- Truncated binary stream: exit0/sINVALID.

Commands, bounded checker-only logs and hashes are in stages.json. No checker source was patched. The successor route explicitly uses dsr-trim -f -c, preserving every original proof lemma; its decoded complete token stream equals the original full forward.lsr. Independent binary Trestle succeeds and truncated negative controls fail. Exact original input/proof/transcript/checker/source hashes bind bindings.json. The successful complete checker-only successor is successor-results/successor-grade.json, not a relabeling of the old jobs.

Both arms stop UNKNOWN before their first restart. Common request/result, exact owners/definitions and root snapshots compare equal. Off: 1579 decisions, 1025 conflicts, 41341 propagations, zero direct uses. On: 1622 decisions, 1027 conflicts, 41673 propagations, two immediate non-jumped binary-reason uses and zero direct binary conflicts. This is narrow mechanism evidence, with zero verdict credit and no speed/solve claim.

Attribution caveat: the original driver recorded kernel_binary_id=1 for each installed binary because DSR import discards the binary ID archive (parse.cpp:628–635); get() returns the presence sentinel when tracking is disabled (kernel.hpp:158–160). Those values are not unique proof IDs. The exact absent-before pairs and immediate assigned reasons identify the two direct uses. Independent full LSR binds their certified binary clauses: line1188 clause8999 [-1421,-1422], line1193 clause9003 [-1421,-1423], line1198 clause9007 [-1422,-1423]. Consumed pairs are the latter two at level1. Root assignments are unchanged immediately after treatment. Do not claim unchanged unique runtime proof-ID identity or transitive resolution attribution.

Prepared unchanged-driver replay pair (not submitted):

    sbatch benchmarks/probes/scratch/r401-x20-prefix-checker-diagnostic-20261007/replay-pair.sbatch

Each SLURM task requires exact hash bindings, invokes the unchanged driver with original transcript replay, requires byte-identical proof and provenance, checks its entire proof through explicit forward binary elaboration, checks roundtrip tokens, independently checks binary LSR and rejects a truncated control. Tasks have identical 4 CPUs/32 GiB/35-minute limits; driver ceiling30s, whole checker ceiling1800s, per-file128MiB. Per-arm result.json is emitted only after all gates pass. First-restart/service boundary replay qualification remains pending. Both original UNKNOWNs mean the replay harness requires UNKNOWN and gives zero verdict credit; any unexpected result fails closed.
