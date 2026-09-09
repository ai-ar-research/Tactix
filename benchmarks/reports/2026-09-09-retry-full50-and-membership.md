# Full Rank 1–50 repair checkpoint and remaining omissions

## Matched scope and runtime outcome

Ranks41–50 already have matched12CPU/48000MiB measurements from6108, all20
Tactix/Kissat outcomes checked and clean. Their fixed reference adds428.944s
to the original40's6810.066s without changing those original measurements.
The independent4CPU/32GiB cpu-q experiment is not mixed into this baseline.
Fresh accepted-production/current-Kissat6121 is separately100/100 checked;
final6227 owns that comparison. No reference timing rerun is needed here.

Candidate6231 full6239 has all50 expected solves, no solve loss, and
9030.164/7239.010s = **1.247430795x** fixed Kissat.6255 freezes38 checked;
the later6258 full comparison has41 checked. Pending certificates remain
unverified and run in the background; no promotion or final gate closure.
Original40 is1.149750x and added10 is2.798239x. The added10 contributes771.344s
of the1791.154s aggregate gap. All50 rows, memory, validation and parent work
are in [6258](2026-09-09-retry-full50-6258/summary.json) and its adjacent TSV.

This is not a successful runtime repair. The parent6169 measured1.160932x
in a different window, so their total ratio difference is not a causal timing
estimate. Nevertheless, nine ranks have changed certificate bytes and allnine
have more conflicts:24/27/31/32/33/35/37/39/40. For example, Rank31 grows from
4,777,112 to7,913,744 conflicts, and Rank33 from4,868,213 to7,408,273. This is
real additional search work, not merely a unit-cost hypothesis. All41 other
certificate hashes match the parent. Keep the repaired mechanism isolated;
do not replace the accepted/default source or claim a gain.

Rank50 remains the largest runtime discrepancy,426.284/49.047s. Its certificate
and11,797,027 conflicts are unchanged from6169. The earlier6211 trace identifies
native-only Eliminate admission at823562; capturing actual admission counters
and deadlines remains the next expanded-set investigation. Rank49 is
162.428/22.555s. Memory remains an independent open gate, including Rank21
3,034,308 versus1,436,392KiB; the private payload redesign is not approved.

## Rank32: one restored member, four still missing

6254 validates6248's complete pre-stop decision, boundary, macro and retry
streams as exact byte prefixes of the same6231 binary's6237 observation.
The inherited300K prefix has69,828,932 decisions/hash10d4b4ca139060f5.
The intentionally stopped partial proof is deleted and hash-bound; its different
stop length prevents a full proof-identity claim. Nineteen negative schema/
prefix fixtures pass (8 membership,7 selected/producer,4 exact-prefix).

Incoming BVE at386968 now contains30,835 Tactix members versus30,839 native.
337674 is restored. There are no Tactix-only members, and every shared score
and positive/negative occurrence count matches. Four native-only members remain:
390671,399915,409823,444046. There are8,486 displaced shared heap positions;
membership is not yet equal, so this does not establish a tie-handling defect.
The causal authority is6248/summary.json; all original files remain frozen.

New6256/job1677309, submitted03:09:54 September9, traces399915's retry producers
with existing6231/native6206 pins. Two independent tasks,12CPU/48000MiB,
1200s solver-only cap,25min allocation, no throttle/node constraint. It stops
at the same initial schedule, leaving other campaigns untouched. Validate
candidate full6248 replay including partial-proof identity; native pre-stop
6208 prefixes and full members plus selected-state tail; both300K hashes;
then strict399915 producer framing before interpreting source history.

## Gates and limitations

6236 B28+C5 has66 expected outcomes,64 checked at6255, ratios0.726888/0.929090
versus production. Prior6169full6181 has49/50 checked at6255. Checking is
asynchronous; no failure observed at this checkpoint. Three independent paired
timing waves remain required for repair timing attribution. Changed-source
Claude-owned fuzz is still open; no denied network transfer or push retry.
No solver source, guard, strategy, public API or frozen campaign was edited in
this increment. Production remainsc2a95a6. No benchmark artifacts were deleted.
Tools/bench39 tests and tools/slurm13 tests pass. The goal remains unmet.
