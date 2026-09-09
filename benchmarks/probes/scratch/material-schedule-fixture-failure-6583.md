# Initial budget fixture failure

All three 6579 builds completed; their initial full ctests failed only the new
`material_schedule` fixture at its budget-tail assertion. The original fixture
is preserved in `material-schedule-initial-fixture-6583.cpp`; original full-suite
logs remain under the 6579 build campaign. No manifest or benchmark submission
had been produced, and no solver-runtime or certificate failure occurred.

A focused rebuild with printed counters reproduced:
`band=0 filter=0 checked=2 stops=0 visits=2` (exit 134 at the same assertion).
The first empty-stream candidate costs one; the native strict `spent > budget`
cutoff correctly admits the second at equality. The fixture incorrectly assumed
an inclusive cutoff. Add two irrelevant binary visits to the first candidate
to exercise a real tail, retaining the strict implementation unchanged.

Resume only the failed build/test stage into new `*-2.log` files; preserve
successful configure/build logs and the original failure. Do not rerun the
completed source preparation or create a new benchmark to hide this result.

After adding the binary visits, all four band/filter combinations reported
`checked=1 stops=1 visits=3`. The next new assertion exposed a second fixture
mistake: `kernel_removed_var` publishes shared advice, not the independent BVE
removal event. Assert that it does not invite, then call the actual existing
`kernel_mark_bve_retry` producer for the positive SP-SLOT-5 control. Neither
failure required a change to the scheduling implementation.

The fixed proof driver also initially assumed that a constant one-step budget
must finish in 100 rounds. Diagnostic6583 shows three strengthenings by round4,
then an overlapping one-candidate prefix through round100, unchanged material
count3. This is not a proof rejection or loss of pending flags. The corrected
control forces one budget cut, then resumes with a completing allowance.

Its next public-instance completion assertion was also too strong: explicit
C2 instances cover large candidates, not the fused round's binary prepass.
The public prefix must remain selectable with empty flags; after exhausting
it, use the actual unfiltered canonical binary service to finish the tiny
refutation. Keep all failed logs/binaries and already checked successful cases;
never rerun those successful cases. The first checked case's proof hash was
not persisted before its later sibling failed; its exact checker acceptance,
fixture source/binary and effects are retained, and this limitation is explicit.
