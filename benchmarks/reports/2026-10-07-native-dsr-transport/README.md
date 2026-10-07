# Native DSR transport focused gate

The maintained bounded C++ helper replaces Python byte loops only when an explicitly copied helper/hash pair sits beside the frozen proof_oracle module. It accepts the same binary tags/32-bit varints and repeated-pivot witness grammar, preserves literal bytes, and rewrites only tags. Witnessed or malformed input never opens the destination. Format rejection, I/O/process failure and witness detection have separate outcomes. Helper absence preserves Python behavior; partial/mismatched pins fail closed. No PATH lookup, environment-selected runtime helper or automatic compilation is used.

Future fixed/controller, experimental target/winner, portfolio and fuzz prepares accept --dsr-transport FILE and include the binary plus SHA256 sidecar in their immutable manifests. The CMake target is dsr-transport. Existing pins are unchanged. Portfolio preparation now freezes its shared oracle module too.

For named outputs conversion publishes an atomic sibling temporary. AnonymousScratch outputs retain their unlinked inode and /proc parent-fd path after a complete validation pass; no named heavy proof is introduced for that path. All converter failures stop checking. subprocess cancellation kills/reaps the helper under the runner's existing whole-pipeline deadline.

Frozen SLURM1753984 passed7 native suites with131 directed grammar cases plus92 existing oracle/runtime/pin/portfolio tests, with no skips, in both Release and ASan/UBSan. Real anonymous-runtime positive and negative proofs reached drat-trim and (for the valid proof) CakeLPR. Tests cover malformed/truncated/overflow and noncanonical varints, mixed tags and chunk boundaries, exact bytes, witness refusal, unchanged invalid destinations, I/O distinctions, helper-hash mismatch, fallback, anonymous lifetime and deadline cleanup. Detailed hashes and results are in focused-gate.json and the run logs.

On33,554,340 bytes, integrated Release conversion took0.156s versus Python13.719s and produced the same SHA256. This is transport throughput, not solver runtime or a checked instance closure. The standalone prototype's131-case evidence is separate; the maintained integration is qualified by this pin.

Preliminary r1 found an overly strict test expectation: the existing runtime marks a rejected checker result with empty LRAT as error. The test now requires the actual independent checker rejection plus pipeline nonacceptance without changing acceptance logic. r2 passed all native suites but omitted historical real-checker dependencies for5 older protocol subcases; r3 packages those dependencies and passes the full99 tests per mode.

Claude owns the separate harness selftest under benchmarks/runs/20261007-native-transport-claude-oracle-owner. Its report remains pending at this focused checkpoint. No full fuzz or regression acceptance is claimed here. Solver semantics are unchanged.
