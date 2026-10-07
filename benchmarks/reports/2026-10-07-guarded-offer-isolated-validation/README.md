# Guarded-cover producer isolated validation

The 17 explicit overlays in source-bindings-final.json were checked byte-for-byte
against the worktree and the isolated source view on base2bce79528. This view
excludes all in-flight persistent XOR, Proof, Substitute and DefaultPolicy edits.
Seven focused Debug tests and the same seven ASan/UBSan/LSan tests pass, with
proof shadow enabled and leak detection active. The proof/model fixture checks
both signed orientations independently and rejects its corrupted proof control.

The producer authenticates current clauses, nominates bounded tuple guidance,
uses exact existing V2 publication and preserves fixed-allowance replay. It
makes no global owner-exhaustion or total-memory-bound claim. New CLI flags are
opt-in; C14 remains explicit and Default scheduling remains disabled.

Two initial fixture errors (reserved StepId zero and work below the supplied
preference count) were corrected without weakening production admission. The
initial bindings and available diagnostic logs are retained. Broader Debug,
Claude-owned fuzz, regressions and target recovery are not established for this
new source. The isolated tests do not validate unfinished XOR code.
