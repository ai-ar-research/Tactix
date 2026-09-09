# Audit 6494 reporting erratum

Audit 6494's sentence “No new PDF overflow” is incorrect. After adding the
complete shared-blocker result, a preliminary PDF compile reported a 2.69647 pt
overfull line. That log was preserved at
`report-to-andrew/20260909-1020-activation-memory/report-overflow.log`
(SHA256 `6b74f03d32abcef2b2ec836aa7c8e2ae42fa8117c4cdf41fad11e6cf9a344cf3`).
The paragraph was reflowed and the PDF compiled twice without overfull lines
before the successful audit finished. Audit 6494 binds the final correct TeX
and PDF hashes, so its validation and deliverable are unaffected. Only its
failure-history prose was stale in the already-running audit process.

The original audit is preserved unchanged. Supplement 6497 verifies its hashes,
the preserved preliminary log, the final clean log and PDF, reruns the tooling
and documentation tests, and includes this correction in the owned landing.
No solver, proof or model failed, no measurement was excluded, and no benchmark
or successful snapshot was rerun for this correction.
