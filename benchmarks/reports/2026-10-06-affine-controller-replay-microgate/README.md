# Affine controller replay and failed-predecessor microgate

This test-only follow-up strengthens verification of solver source `4096d41b3`.
No solver source, public API or scheduling changes are introduced.

The pure matrix fixture first retains a one-assignment predecessor and a
two-assignment conflict. An equal-length nonmatching request then pays the
predecessor-comparison charge before exhausting its budget or being interrupted
at fresh conditioning. Rows, trail, work, event, diagnostics and the hidden
checkpoint remain unchanged; the preserved event authenticates and the original
predecessor still restores.

The controller fixture independently executes serialized initialization and
fresh public requests against two kernel states. It records and replays actual
decision, propagation, restart, resume and affine-service results across
extension, reason publication, predecessor restore and empty-trail rebuild.
It compares proof bytes and provenance, verifies restore/rebuild counters and
unchanged publication identities, then checks the reconstructed model against
both original parity CNFs. This is a fixed controller transcript, not a claim
of universal policy replay coverage. The SAT fixture does not submit its
nonterminal proof stream to an external checker; the separate native-proof
tests exercise canonical closure checking.

The new atomicity test passes in [Debug](debug.log) and
[ASan/UBSan with leak detection](asan.log). After fixture corrections, the
controller-search and native-proof checks pass 2/2 in
[Debug](debug-final.log) and [sanitizers](asan-final.log). The sanitizer build
also enables proof-population shadow. These three distinct affected checks
supplement the preceding cumulative 10/10 checkpoint.

Initial failures are retained. The standalone program incorrectly used
reserved StepId zero; assigning its sole step ID 1 fixed deserialization.
[The sanitizer stack trace](asan-stacktrace.log) then identified an instrumented
libstdc++ string input-iterator assertion in the fixture's proof-file reader;
an initialized byte-buffer reader replaced that test-only constructor.
[Intermediate Debug](debug-repaired.log) and
[intermediate sanitizer](asan-repaired.log) results preserve that distinction.
No solver change was needed for either correction.

Claude-owned cumulative fuzz, full Debug and the four standing regression
gates remain pending. These fixed tests establish neither broad acceptance nor
a performance improvement.
