# Final-source allocated storage recheck

The completed `du -s -B1 .` scan exits 0 with no stderr and records **259,232,714,752 allocated bytes (259.23 GB)** for the repository. This is below the 700,000,000,000-byte cleanup gate after final-source build preparation began. [Exact output](du.txt) and [stderr](du.stderr) are retained.

The preceding `du -sb .` result of 452,944,463,759 bytes measured apparent size. These two measurements use different accounting and must not be interpreted as a new deletion or storage drop. No deletion occurred during this scan. Immutable active Debug/fuzz pins, retained failure evidence, checkers and regression baselines remain protected.

Continue aggressive cleanup if subsequent allocated usage exceeds 700 GB; build only required targets except the full Debug suite, which requires its complete executable inventory.
