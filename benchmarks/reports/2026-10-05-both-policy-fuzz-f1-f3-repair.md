# Both-policy fuzz F1/F3 repair

The Claude-owned both-policy campaign against candidate
`1de96264a6c7b54c0e635433ce22859638746fd4` found two minimized product
defects. This checkpoint repairs both defects; it does not claim a passing
replacement fuzz campaign.

## F1: repeated Direct-PHP capture

The 51-clause minimized RandomController case invoked
`DirectPhpRefutation` after inprocessing had left duplicate physical watch
pairs for one logical binary clause. The complete snapshot scan treated the
second pair as malformed and returned fatal `InvalidRequest`.

Capture now charges and scans the same population, coalesces identical binary
keys, and retains the first deterministic authenticated occurrence. A focused
unit test constructs a duplicate binary and requires the canonical action to
decline as `NotRecognized`, rather than reject the request. The exact minimized
controller seed then completed SAT with two Direct-PHP applications and no
`InvalidRequest` or controller error.

## F3: checker-retained X9 helper units

The 12-clause minimized X9 wide-row case deleted a proof-only helper unit and
later fixed the same helper to the opposite polarity. Exact-deletion replay
accepted this sequence, but the canonical `dsr-trim -> dsr-compress -> srcheck`
route retains unit deletions and rejected the certificate.

Wide proof preparation now reuses the unique polarity of any earlier derived
unit on that proof-only identity. Conflicting historical polarities decline
before publication. The provenance journal records the installed polarity and
matching reconstruction value. Focused coverage additionally requires that no
helper receives both retained unit polarities in one transaction.

## Validation

CPU-Q job `1750809` cloned base
`23a95a4b2a31bc5fad2590318553a11923f969ef`, applied patch SHA-256
`c03a76349a18d72315e7d150ebfbbe69900f22544f6f013ad2e809c7f1d14b55`,
and built the Debug preset with ASan+UBSan. All six focused tests passed:
`affine_basis_live`, `affine_matrix_transaction`,
`affine_matrix_transaction_proof`, `parity_lifecycle`,
`parity_wide_reason_proof`, and `direct_php_public`.

The F1 reproducer completed SAT without a controller error. The F3 reproducer
completed UNSAT, emitted DSR SHA-256
`028f41d46b850e06f79c9fabd85a60137867738c321e5e414e558b35d6a90d14`,
and passed the independent proof oracle, including Trestle. The validated
Debug solver SHA-256 was
`cb638bc320bf03c8aac4513016f9f767e8b053f1835dc9b3daf17cb5927e3168`.

Jobs `1750787` and `1750801` had already passed the same six tests. Their
wrappers stopped respectively on the solver's normal UNSAT exit code and a
missing oracle work directory; neither exposed another product failure.

Acceptance remains open. A fresh immutable candidate containing these repairs
must run both DefaultPolicy and RandomControllerPolicy fuzz; the superseded
candidate's remaining census cannot establish acceptance for this source.
