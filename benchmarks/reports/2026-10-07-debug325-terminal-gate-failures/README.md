# Archived Debug terminal validation failures

Job1752246 is FAILED,325 JUnit cases, five failed checks: config_wiring,
effort_specs,feedback_statistics,comment_hygiene,doc_size. Runtime tests pass,
but fullDebug gate does not. Config wiring lacks three Walk fields and updated
candidate bindings; transitivity effort classification is repaired in c584.
Statistics inventory needs regeneration. Four documentation budgets stillfail.

Some Git-root-based documentation tools resolve the outer mutable checkout
rather than archived source (comment_hygiene sees new Default fixture). This
scope discrepancy prevents treating documentation checks as immutable-source
acceptance and must be repaired in maintained tools before the next snapshot.
Do not edit this frozen campaign or waive its failures. No cumulative acceptance.
