Resume independent native-DSR transport qualification from your frozen b063/r6
FAIL. Preserve every earlier report and reproducer. The user authorizes
continued Claude-owned fuzz/oracle work. Do not edit production or commit.
Run heavy work on Slurm, full width, without arbitrary node constraints.

The developer successor replaces the rejected whole-lifecycle
`pthread_sigmask`/`sigpending`/`sigwait` design with main-thread handled-signal
deferral. Before acquiring staging it temporarily replaces callable SIGALRM
and SIGINT handlers with a recorder. It then kills and reaps the helper,
removes the staging namespace, restores every original handler, and invokes
recorded handlers only at that controlled boundary. Worker-thread behavior
uses ordinary try/finally and never invokes a main-thread handler. It also
maps helper SIGXFSZ exits to `OSError(EFBIG)` so the fixed44 runtime classifies
the conversion as `scratch_file_limit`, never proof rejection.

The r9 successor specifically closes r8's between-restore boundary: while
both recorders are active it stops `ITIMER_REAL`, restores SIGALRM first and
SIGINT last with bounded state checks, rearms the remaining timer, and only
then replays recorded handlers. Independently stress repeated SIGALRM/SIGINT
during handler restoration, including exceptions after each state change,
timer-state behavior, partial installation rollback and permanent restore
failure. Do not accept a repair that merely moves an asynchronous bytecode
window or leaves a recording handler installed after an exception.

Freeze your own copies from developer commit
`838325505df20334e393576689b977958ca15fbf` and r9 pin manifest SHA-256
`f7a83f215eb12158a85b3cf1b1ebd98602ab9659290c3493efdea731c26c5097`;
do not import live files. Release/ASan helper
and checker identities are unchanged from r6. Developer job 1754901 is only
developer evidence, never independent acceptance.

First run the exact r6 reproducers:

- `sigwait-hang-process-directed-signal` with an extra thread;
- `sigmask-leak-single-thread`, including a fresh later deadline.

Then retain the earlier lifecycle bar: exact r5 prologue/acquisition-boundary
residue, binary-probe deadline and r3 interrupted-cleanup reproducers;
handled SIGALRM/SIGINT at acquisition, Popen, communication, cleanup and
handler restoration; child and process-group kill/reap; original-exception
precedence; permanent cleanup failure visibility; unchanged destination on
incomplete conversion; concurrent namespace isolation; NFS kill/deadline;
and explicit native reach. SIGKILL remains outside the catchable contract.
Investigate ResourceWarning rather than dismissing it from clean reruns.

Run the native transport harness selftest with known-bad proofs/models and
real checker reach. The frozen dependency modules include the committed
cooperative scratch and early-release lifecycle. Exercise their affected real
SAT-model, witness-free proof and witnessed-proof last-use boundaries, plus
file-cap classification, or explicitly leave a scope unaccepted. Preserve
all complementary AM407 option/no-option controls; conversion success is not
proof acceptance. This request does not require broad solver fuzz.

Publish IN_FLIGHT.md, terminal DISPOSITION.md and status.json with exact
hashes, jobs, required/achieved coverage, limitations and acceptance. Fail
fast on one actionable product failure and hand root one bounded reproducer.
