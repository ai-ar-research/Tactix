# Public guarded-cover pilot startup failure

Job1752050 completed0 but both diagnostic arms stopped before native publication, driverexit3, because the harness unconditionally called public Propagate at an already-propagated input boundary. Propagate correctly requires pending work. Both original proof prefixes independently check; native publication/use and target recovery are absent.

The original source-bound package remains immutable. Successor must query public applicability before propagation and retain exact root-state validation. The original grader labelled incomplete startup as common-startup-mismatch; successor should distinguish startup failure. No solver-core change is warranted by this evidence.

Source ef0e00e9d,653 verified bindings and seven fixed checker controls are recorded in the frozen manifest. This failed diagnostic does not qualify mechanism integration or target performance.
