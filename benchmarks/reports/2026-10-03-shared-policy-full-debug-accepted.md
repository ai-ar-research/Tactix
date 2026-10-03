# Shared-policy full Debug accepted

Full Debug job **1746924** passes on production source
`375b621ba6677600f3e77bf8383af25d256d7b67`. All **212 registered tests**
ran and passed; there are no failed, errored, skipped or disabled cases.
CTest exits zero and SLURM records `COMPLETED`, `0:0`, elapsed `04:14:06`.
This closes the full-Debug requirement, **not whole-goal acceptance**.

## Exact completion and immutable snapshot

The frozen registry and JUnit have exactly matching test-name multisets. The
maintained acceptance parser checks per-case status/failure agreement and
summary counts rather than accepting a green aggregate alone. Its result is
[acceptance.json](2026-10-03-shared-policy-full-debug-accepted/acceptance.json).
The long `demo3_scripted_default` test passes in 15,186.45 seconds; total CTest
wall time is 15,213.98 seconds. Its quiet runtime was not treated as a failure
and the run was not restarted.

The submitted manifest binds **984 binary/source/support files**, including
**795 repository source bindings** identified by PIN metadata. The scheduler
log contains successful preflight and postflight verification markers; a parent
read-only recheck also verifies all 984 digests. The immutable Debug solver
SHA-256 is
`95197f8c9f468bd3b121b1cd66d65457b9d5baade62bffb27f95e82caff593c2`;
PIN metadata SHA-256 is
`72a3a25461ab2a1974b6c16e723e54e87734fdcd14e5bfbddf1676f96cf3c136`.
The closed watcher session is `40103`, terminal exit zero.

The [hashed companion](2026-10-03-shared-policy-full-debug-accepted/validation.json)
preserves the exact registry, JUnit, manifest, submitted job and verification
code, complete CTest/scheduler logs, exit marker, PIN metadata, terminal
accounting and binding receipt. Earlier frozen snapshots remain as recorded. Raw CTest JSON/XML preserve their
original generated trailing spaces; the exact two-file whitespace exception and
byte-equality checks are recorded in
[landing-check.json](2026-10-03-shared-policy-full-debug-accepted/landing-check.json).

## Other acceptance and limitations

The [four selected DefaultPolicy regressions](2026-10-03-shared-policy-regressions-recovered.md)
already pass within the declared bounded-checking protocol, with the original
WV100 rejection preserved and the single registered recovery consumed. Scoped
feedback overhead and deterministic benchmark correspondence retain the limits
stated in those owning reports; Debug does not broaden those claims.

The current Claude fuzz owner remains active through CLI `3818`; public
scheduler metadata shows targeted job `1747244` running. Completed fresh
and targeted job exits do not establish seed counts, effective coverage,
purity or replay. The parent has inspected public metadata only, not private
campaign files. Exact fresh counts, retained-corpus outcomes and all required
R1--R13 targets need the owner's delivered terminal report.

Next: preserve the candidate bindings through the owner's postflight; obtain
and audit its terminal both-policy report, close any demonstrated gaps, then
complete the original requirement-by-requirement audit and final Andrew
TeX/PDF handoff. Reward choices and feedback-informed learning remain deferred.
No solver experiment, source mutation, artifact deletion, unrelated-job
cancellation or push is introduced by this checkpoint.
