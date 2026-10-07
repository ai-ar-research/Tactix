# Archived Debug snapshot harness NFS recheck

The `938a6ae6b` immutable full Debug campaign (job `1751518`) reported a `fuzz_slurm_snapshot` teardown failure: NFS returned EBUSY while deleting a temporary harness copy. The original failure remains in the campaign log.

Job **1751521** reran the exact unchanged archived `source/tests/test_fuzz_slurm_snapshot.py` in a one-CPU/2-GiB/five-minute SLURM allocation. It completed exit 0 and printed **fuzz SLURM immutable harness snapshot: PASS**. [Submission](snapshot-recheck-submission.json) and [output](snapshot-recheck-1751521.log) preserve provenance. No source, test or checker was changed and no actual fuzz campaign ran; the test mocks submission.

This clears the targeted infrastructure recheck only. The main Debug suite still awaits Lucky and final binding verification; existing documentation-size failures remain unaccepted. Final `76e1f68b8` has its separate Debug archive preparation.
