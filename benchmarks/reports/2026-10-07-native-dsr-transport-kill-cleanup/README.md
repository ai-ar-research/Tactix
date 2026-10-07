# Native transport child-kill cleanup

Claude owner 22266f12 rejected the first integration for one storage-hygiene defect: killing native conversion could leave a named partial DRAT staging file. Proof soundness/routing controls passed; this repair does not change checker acceptance.

The oracle now owns a unique staging directory beside each regular destination and supplies its exact output path to the helper. The helper still validates before writing and atomically replaces the destination only after successful conversion. Python removes the directory only after subprocess.run has reaped the child, including SIGXFSZ, SIGKILL and whole-pipeline deadline exits. Calls never share a cleanup namespace. Anonymous /proc descriptor destinations retain the previous inode-preserving path.

Standalone conversion writes its partial output to O_TMPFILE. It needs only a brief link/rename operation after successful completion. A standalone process killed during that final publication window cannot clean its own completed-file staging name; managed oracle calls own and clean their entire publication window. Standalone O_TMPFILE failure is an I/O error; managed conversion supports ordinary sibling staging even on filesystems without O_TMPFILE. The helper and oracle must be pinned together for the new optional OWNED-STAGING protocol.

Frozen Slurm array 1754249 passed 8 transport suites plus 92 protocol tests in both Release and ASan/UBSan, zero skips. A new 128 MiB bounded test observes actual output writes before issuing SIGKILL or the pipeline deadline signal; separate RLIMIT_FSIZE controls deliver SIGXFSZ. Every case requires the original destination unchanged, no named staging leftovers and a reaped child. Grammar, anonymous runtime and independent drat-trim/CakeLPR positive/negative controls remain green.

Source and helper identities, timing and gate counts are in focused-gate.json. Claude affected-leg requalification remains pending. The fixed helper converts 33,554,340 bytes in 0.173s versus Python14.438s with identical bytes. No benchmark closure or final-candidate acceptance is claimed.
