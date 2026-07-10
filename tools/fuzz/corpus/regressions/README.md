# Regression corpus

Minimal failing cases from fuzzing, archived forever (plan §8.2). Entries are
directories `NNN-<slug>/` containing at least `min.cnf` (or `case.cnf`),
`meta.json` (seed, failure kind, diagnosis), and where applicable the rule
trace. Every fuzz run replays this corpus first (`fuzz.py corpus`); entries
are never deleted. Promote a shrunk failure bundle here once the bug is
understood, and reference the entry in the fixing commit's message.
