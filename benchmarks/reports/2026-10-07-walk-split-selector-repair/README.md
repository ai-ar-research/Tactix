# Split Walk public-selector repair

The random controller formerly included seed identity zero among full-CNF Walk choices. That identity is illegal for this opt-in profile and could end an otherwise legal run as UNKNOWN. Only legacy Walk now includes the ordinary-phase zero choice. Full-CNF Walk chooses an exact observed candidate; an empty snapshot preserves the configured identity for an explicit stale decline.

The fixed-seed public-policy regression reproduces legacy zero selection, full-profile admission and export, exact walk3 serialization and replay, stale decline with zero preparation/search/export, continuing SAT solving and independent checking of all original clauses. Debug passes1/1 (0.43s); ASan/UBSan with leak detection and proof shadow passes1/1 (2.01s), outside sandbox ptrace restrictions. Other focused split-Walk checks remain the preceding b56 nine-test checkpoint. Fresh Claude-owned broad and targeted fuzz remains required; no performance claim.
