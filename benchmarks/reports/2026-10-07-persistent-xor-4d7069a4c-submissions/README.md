# Persistent XOR committed candidate submissions

Local commit **4d7069a4c** is pinned in `build/pin-persistent-xor-4d7069a4c`; all 390 src/include/CMake inputs match the focused-validated compiled snapshot byte-for-byte. Release binary SHA256: `64da8d3e0702c3d4a8da338bac90d627dcaacd992f60653073638741a897078d`. Fifteen focused Debug and eight ASan/UBSan/leak checks pass; full candidate acceptance remains pending.

Fresh B28+C5, Rank1–50, WV100 and WV2–89 full-width arrays are submitted as 1752440, 1752441, 1752442 and 1752443. Their exact pinned arguments are `--auto-proof --default-affine-search=on`. Standing saved row/scoring/proof-deadline contracts are unchanged.

Rank 569 job 1752488 follows successful independent pre-launch validator 1752458. Its exact arguments are `--dsr --default-affine-search=on`, with 5000-second solve, 32-GiB memory and 14400-second checking limits. The original-instance target remains substantially below verified Kissat 3359.975 seconds with compact independent proof; no result or closure is inferred from submission.

Full Debug campaign `benchmarks/runs/20261007-persistent-xor-4d7069a4c-full-debug` has frozen its source and is building before submission. Four existing documentation-size failures remain unwaived. Claude owner session 4109784f-d61a-4bd4-8f29-8f842447ddc7 received the committed pin and cumulative effective fuzz authorization; its current qualification CLI is still running. Existing earlier broad campaigns remain preserved.

Maintained job watchers: B28 session28769, Rank59143, WV10085001, WV242170 and rank56976909. Full Debug prepare60354 and Claude owner12175 are live handles. Independent completion and scoring remain required; verified portfolio count stays4/44.
