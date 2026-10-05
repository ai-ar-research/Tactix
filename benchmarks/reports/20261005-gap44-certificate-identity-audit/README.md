# Exact-certificate acceptance audit for the 14 Tier-D proof-only misses

This audit searched every `benchmarks/runs/**/results/*.json` record, including ignored run artifacts, for the exact SHA-256 of each certificate produced by the fixed `d170b4a5` Tier-D comparison. A prior acceptance counts only when certificate bytes, instance, original decompressed CNF hash, and canonical checker-CNF hash all agree. The checker pipeline, checker pins, elapsed time, and applicable deadline are then bound separately.

The result is **one historical fixed-target recovery out of 14: rank 744**. The other thirteen certificates appear only in their original failed records. No historical result accepts any of the fourteen within the newer 1,800-second regression cap.

Rank 744 has two independent verified records for the same 401,269,091-byte certificate. The tighter one used the exact same `dsr-trim`, `dsr-compress`, and `cake_lpr` binary hashes available in the target campaign and verified the exact original/checker CNF hashes in 9,615.897 seconds. This is below the fixed target's 14,400-second checker deadline, so it covers the fixed 44-row target. It does not pass today's 1,800-second regression cap. The source record is `benchmarks/runs/20260927-am507-wv100-dsr-trim-first/results/089.json`; the same certificate was independently accepted again in `benchmarks/runs/20260927-am519-no-mab-wv100/results/089.json` in 10,271.805 seconds. `audit.json` binds the latter record and manifest hashes, stage times, and checker pins.

The audit does not combine a baseline proof with a Tactix result. It transfers acceptance only across byte-identical Tactix certificates on byte-identical checker inputs. Current regeneration and 30-minute checking remain separate evidence.
