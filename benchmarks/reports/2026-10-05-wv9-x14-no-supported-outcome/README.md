# WV9 GF(3) X14 causal boundary

This frozen diagnostic uses the decompressed Tier-D rank-187 / WV100 rank-9
input `247e42e46495f1c4b8707e497af8c93e-16.sanitized.cnf`, SHA-256
`dd2e52c12cafa012316052c6c2ac06ea48ccf1c999a5ae57a25c5f208e1cb9bc`.
The source and linked objects were pinned from
`5c51fad1dc12f4b652d81251765009c220c4be5c`. No production source changed.

## Result

The standalone Gaussian planner in CPU-Q job `1750867` completed in 0.12
seconds with 360 outcomes, 19,516 derivations, peak row width 85, peak
provenance 235, 5,160,177 work, 780,488 fill entries and 42,802,736 retained
bytes. The diagnostic completion envelope was deliberately above each of
these measured peaks.

CPU-Q job `1750876` then ran the public X14 offer-only path. Recognition,
bridge and Gaussian limits used the measured exact requirement plus a fixed
margin of one; certificate, proof, revalidation and session caps remained
stock. It returned `NoSupportedOutcome`, `complete=true`, zero candidates and
zero offers. Exact accounting was 21,040 source clauses, 81,360 source
literals, 435,451 capture work, 5,413,377 bridge work and 4,053,219 certificate
work. Runtime was 6.75 seconds and `/usr/bin/time` peak RSS was 308,612 KiB.

The bridge completed one work unit below its diagnostic ceiling, and the
certificate used 4,053,219 of its stock 10,000,000-work allowance. The decline
therefore is not a recognition, bridge, Gaussian or certificate resource cap.
The certificate adapter supports only unit rows and zero-row conflicts; its
`NoSupportedOutcome` result after the complete 360-outcome plan establishes
that all outcomes are derived multi-variable rows. X14 currently has no
publishable Boolean unit or conflict for this target.

This closes the causal preflight only. It does not recover the SAT target and
does not justify increasing production resource limits. A productive repair
would need a separately specified, certified way to consume selected
multi-variable GF(3) consequences during continuing search.

## Frozen artifacts

- `planner-result.json`: exact job `1750867` planner result.
- `profile.json`: exact measured X14 profile used by job `1750876`.
- `offer-result.json`: exact job `1750876` offer result.
- `SHA256SUMS`: hashes of this report's machine-readable evidence and the
  external diagnostic source/binaries retained in the probe directory.
