# WV9 X19 direct-clause proof compactness boundary

## Decision

Do not raise X19's public `1,000,000` proof-literal cap and do not publish the
screened action.  A deterministic 16-candidate screen found no admissible
direct forbidden-tuple consequence.  The direct authenticated Gaussian-DAG
planner is now limited by proof representation rather than planner work.

The next design should represent GF(3) reasoning as a bounded Boolean extension
circuit: one-hot partial-sum wires and constant-size mod-3 Scale/Add gates tied
to exact authenticated source tuple clauses.  The certificate must prove the
target output from those source relations with ordinary DSR/RAT additions,
retain exact replay/projection identities, and publish the introduced semantic
variables to CDCL.  Merely publishing intermediate case-split clauses in
separate actions could reduce per-action size, but it would only divide the
same proof volume and is not the primary compact representation.

## Fixed input

- Saved WV9/Tier-D187 input:
  `benchmarks/cache/tierD24-26/247e42e46495f1c4b8707e497af8c93e-16.sanitized.cnf.xz`
- Compressed SHA-256:
  `9500dbeaceb88b9a16e7ff5ca17684d3984dbb631a72726fba3c4b514ce55057`
- Decompressed SHA-256:
  `dd2e52c12cafa012316052c6c2ac06ea48ccf1c999a5ae57a25c5f208e1cb9bc`

## Exact-hash memo gate

CPU-Q job `1751014` completed `0:0` in `00:02:12`.  It used diagnostic commit
`959ed33e01b9cb83b35b55bc5aacbd957ecef246`, source archive SHA-256
`59964a653e979018cbbc5f30b1d9105b689dd0882f8fc25642ced54a79422c3d`,
diagnostic SHA-256
`352a581ffbf3b34551c4f4e85c81135d11e34905beb1dac35d3e0ff28023c691`,
and submission-script SHA-256
`07760842e6269cc6fc681b582ca9d60884de0bf570b30c55f0f9bd0131a47059`.

The append-only deterministic open-address index removed the 50M-work
boundary.  The first candidate stopped at `EventLiterals` after `15,929,918`
work units, `62,282` events, and `999,998` emitted literals.  It had `54,500`
memo entries (`8,949,456` accounted bytes), 15 rehashes, and 144,424 exact-key
collisions.  The result line SHA-256 is
`767a540cbc36575625ac7712861c961f93fe8b169e6d0f793fbf3720b101181d`;
stderr was empty (`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`).

## Bounded candidate cost screen

CPU-Q job `1751017` completed `0:0` in `00:03:54`.  It used diagnostic commit
`288c021b1bc6e50924651896537297cf9b70319f`, source archive SHA-256
`6fce9d0cb3e6e57bdc89b4bbe97b5ce94029756155c50221f8d064706b860239`,
screen SHA-256
`417523b8c21c47f320e00a85ea3edda1f8243c63e6eb8896eb5fa6624dfc7969`,
and submission-script SHA-256
`f40974e325cec8597cec09ea75b5088ce737fa584c71ea808479315716a16aad`.
The result SHA-256 is
`a4a38a6761e2a4e16a49d073ca0aba01a147f1ca5e9b6a16fbb99db0b9e7397c`;
stderr was empty.

The screen selected, without formula-specific identities, the first two
admissible tuples for each of deterministic authenticated outcomes 0 through
7.  It reused the immutable captured theory/bridge but created a fresh proof
memo for every candidate, matching independent public actions.  Count-only
mode still performed every authenticated derivation, exact local RUP or
weakening check, memo operation, work charge, width check, and interruption
check.  It omitted event-body retention solely to measure proof cost beyond the
unchanged public literal cap.

All 16 candidates stopped at `MemoBytes`, before producing a complete plan.
Fifteen reached the same common boundary: `86,277` events, `1,481,568`
literals, `79,391` or `79,392` memo entries, and about `13,899,9xx` accounted
memo bytes after about `24.25M` work.  The remaining candidate was worse:
`99,543` events, `1,578,917` literals, `80,558` memo entries, and `25.40M`
work.  No exact completed cost exists inside the retained bounds; the smallest
observed lower bound is therefore `86,277` events and `1,481,568` literals,
already 48.2% beyond public admission.  The repeated stop at the same
derivation/tuple boundary shows that the candidates share the same expanding
case-split frontier.

No solver mutation, target solve, or checker run followed either gate.  The
production-only hash-index commit
`deeed63e0a8b2f880dcce249a1ede680f30f2da1` remains isolated and is not a
promotion recommendation while the direct-clause representation is
inadmissible.
