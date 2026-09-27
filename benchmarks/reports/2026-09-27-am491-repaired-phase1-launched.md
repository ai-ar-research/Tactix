# AM491: repaired-source Phase 1 automatic-proof arrays launched

Candidate source `0ed8b564` is unchanged from the AM485 X7 status repair. Two independent plain Release builds and the complete checker/source-pin validation are recorded in [AM487](2026-09-27-am487-repaired-phase1-prepared.md). Immediately before submission, `fixed_runtime.verify_campaign` and `runtime_task.validate_runtime_pin_contract` passed again for both frozen manifests.

| suite | SLURM array | rows | manifest SHA-256 | solver aggregate target |
| --- | --- | ---: | --- | ---: |
| Rank 1–50 | `1731234_0-49` | 50 | `5459b235e5051f74bff51dedd47af481b0aefc7195f70cb84a3896fae6ab12fc` | < 8,115.735 s |
| B28+C5 | `1731267_0-32` | 33 | `37a23e60c31d9912c67e68e46614829230f26609a905005ecc44f673c1f58e39` | < 952.263 s |

Both arrays were submitted on 2026-09-27 at 18:50 UTC through `tools/bench/fixed_runtime.py submit`, full width, on `cpu-dense-hwulab-q`, with 12 CPUs and 32 GiB per case. Each task has a 1,200-second solver limit, a separately bounded independent checker stage, and plan §5 automatic PBP/DSR/DRAT routing. SAT models must validate against the original CNF; UNSAT certificates must complete the pinned independent route. No reference solver was rerun. `launch.json` in each ignored run directory records the job and manifest identity. These are Phase 1 jobs, not the 4-CPU/5,000-second Tier D campaign.

The launch follows the terminal AM486 affected fuzz and the bounded [AM490](2026-09-27-am490-postsat-twin-divergence.md) audit. AM486 remains **NOT ACCEPTED under its frozen oracle** because 12 post-SAT, non-X7 action twins differ across proof formats; every X7-specific condition, reached model, and graded certificate passed. AM490 attributes 11 cases to different legal SAT-producing policy paths and observes a pre-SAT hidden state difference in the twelfth. No source repair or checker change was indicated by that handoff. Running this separate performance gate does not promote N1/N2 or waive the need for a newly frozen terminal-scope fuzz oracle.

The arrays were still running/queued when this launch note was written. Score them only after every task is terminal, using `fixed_runtime.py score` and the frozen aggregate and per-case limits; inspect any failed proof/model or timeout before drawing a Phase 1 conclusion. Repaired-source full Debug job `1731003` is also pending. No Tier D job has been submitted.
