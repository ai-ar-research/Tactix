# Expected-image recognition metering repair

Review of native equality repair `938a6ae6b` found that shared prefix capture allocated its quadratic expected AMO map before charging recognition work. Construction now charges each expected value and each clause before allocation, using the existing one-plus-literal clause accounting, and polls interruption through the same meter. Exhausted and Interrupted remain distinct from malformed Invalid. No proof semantics or shared controller settlement changed.

Fixed fixtures cover exhaustion during ALO/AMO construction, a 128-value-domain image bounded by 260 work, deterministic interruption on the second construction charge, unchanged capture output/kernel/journal, malformed inputs, and successful same-request recapture with a fresh allowance.

Focused [Debug](debug-ctest.log) passes **13/13** in 41.39 seconds. [ASan/UBSan](asan-ctest.log), with proof shadow and leak detection, passes **12/12** in 264.45 seconds, including independent equality/X20 and root-fixed prefix proof checks. The additional public-prefix sanitizer target is still building and will be recorded separately. No performance claim follows from these checks.

Immutable equality candidate `938a6ae6b` remains in Claude-owned targeted job `1751516` and native-controller broad job `1751515`; its smoke calibration checks all 36 reproducer runs. Those jobs do not cover this later metering repair. Resume the same owner session on this committed source and validate affected recognition-budget/interruption/retry and native-producer coverage, retaining unchanged evidence only under the owner disposition. Full Debug and the four regression gates remain pending; the earlier Debug archive is immutable and still building.

Last completed storage scan: 452,944,463,759 bytes, below the 700-GB gate. No generated artifacts were deleted during this increment; all pins remain unchanged.
