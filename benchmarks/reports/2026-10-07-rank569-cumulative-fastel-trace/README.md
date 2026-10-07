# Rank569 cumulative FastEliminate trace disposition

This diagnostic tests the cumulative source after completing SC2024
FastEliminate's noncollapsed and collapsed feasibility self-strengthening.  It
does not establish a timing benefit or close Rank569.

SLURM job `1754568` completed on node017 from commit
`37226faa78d052fc71c2c3fdd4a02f8ca0108c61`, tree
`66dee0690a5469c393b3baffcc3d448066f2f052`, and source archive SHA-256
`c11109ff8f2358406e48b1f12ba584cfd810bb1c5d48c61257c304f2ce0fd2ea`.
The event-trace binary SHA-256 is
`24639080ce24e2e1b37541860a955299c706f8d985c50c7a80a6faa6bb370b20`;
the wrapper SHA-256 is
`24b4f8cd97da9b025603858c5a3d150a2abb3bde0e12bc68ed4809249c8e5d41`.
Artifacts are under
`benchmarks/runs/20261007-rank569-cumulative-fastel-trace`.

Against the pinned Kissat 4.0.4 reference with the SC2024-matched
`fastel=1` setting, all 7,596 selected rows through conflict 50 match exactly:
223 decisions, 49 analysis starts, 49 learned clauses, and 7,275 implications,
including conflict, decision, level, and payload fields.  The retained search
state therefore has no selected-event divergence over this prefix.

The complete startup image still differs.  Kissat ends preprocessing with
3,989 irredundant clauses and 380 binaries; cumulative Tactix has 3,969 and
378.  The earlier noncollapsed-only Tactix pin had 3,977 and 378.  Thus the
collapsed parity path performs its expected eight additional source deletions
and exposes a pre-existing 20-clause excess-deletion gap; it does not close the
image-count difference.  Those extra native clauses are inert over the checked
50-conflict prefix.  Without a causal solving benefit, this count gap does not
justify another production change.  The full Rank569 candidate and cumulative
regression gates remain the decision authorities.
