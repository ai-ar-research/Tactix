# AM451 experimental native-DSR proof transaction

This is an isolated candidate from branch `h2-proof-transaction`, parent
`228e9c2f`. The source patch and Release C-API replay binary are hash-pinned
in `run.sbatch`. The job replays the exact public AM423 H2 action program and
checks its original-CNF native DSR with frozen no-option dsr-trim in both
directions, then Trestle on each successful LSR. It is diagnostic only; no
production source or checker route has changed.
