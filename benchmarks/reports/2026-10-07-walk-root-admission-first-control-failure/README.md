# Imported-root Walk pilot: checker-control failure

All three tasks of1751820 failed before producing solver or Walk results. The empty-positive native DSR control used a one-clause formula; the pinned dsr-trim rejects it with "Number of HT buckets must be positive." Its clause hash table initializes with formula_size/2, which is zero here. This is a checker-fixture failure and provides no evidence about Walk performance or soundness.

Preserve the original submitted package and failure artifacts. A separate successor uses two distinct non-unit SAT clauses for native DSR controls, independent model checking, and a separate one-unit formula for exact direct-LSR controls. Checkers and solver remain unchanged. Require all positive and negative controls before admitting any pilot result. The experiment concerns public imported-root admission under matched nominal work, not reproduction of the historical default-policy Walk effort trajectory.
