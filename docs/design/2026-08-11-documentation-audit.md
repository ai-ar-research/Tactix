# Tactix documentation audit — PASS 1 findings only

> **FROZEN AUDIT SNAPSHOT — 2026-08-11.** Findings only; it owns no live fact.
> Item 1 of its priority order (split section 13 into
> `docs/scheduling-parity-ledger.md`) and the public Sweep payload correction in
> §1.A.7 are DONE. The remaining contradictions, the fail-open checker contracts
> in §4, and the PROGRESS/§17 compaction in §2 are open and are the reason this
> snapshot is kept. Present-tense claims describe the tree at `ea281a6`.

Date: 2026-08-11  
Repository: `/home/hwu/SAT-solving`  
Audited commit: `ea281a6a04c6d03520b78ee3618b9f6e82baaa6c`

## Scope, exclusions, and method

This is a findings-only audit. No existing repository file was changed. This
report is the only file created.

The working tree contained exactly the two user-declared modifications:

- `CLAUDE.md`: one added skill-routing line at current line 39.
- `DEVELOPMENT_PLAN.md`: the added early-process-exit incident paragraph at
  current lines 288-300.

Those additions are excluded from every defect and proposal below. All line
numbers refer to the current working-tree files, so lines after those additions
include their offsets.

I read all requested living documents, all 19 tracked `docs/design/*.md` files,
all 14 tracked `tools/docs/` checkers/utilities, and all six requested skills. I
inspected source, tests, checker registration, Git history, and checker output
only to test particular claims. `[VERIFIED]` means checked against source, a test,
Git, or executable checker output. `[INFERRED]` marks a structural/editorial
conclusion.

Baseline facts:

- `[VERIFIED]` `docs/implementation-notes.md` is 800 lines in both HEAD and the
  working tree. `tools/docs/check-doc-size.py:9-20,38-41` accepts exactly 800 and
  has no reserve threshold.
- `[VERIFIED]` the documentation checker suite is currently green, apart from
  `check-progress.py` warning that `squeue` was unavailable. The citation checker
  resolved 64 citations and skipped 151, all because of ambiguous basenames. The
  option checker accounted for 418 reference options. The inventory checker
  reported 28/28 families. The effort checker reported 16/16 scheduled rules.
- `[VERIFIED]` the last eight commits touching implementation notes all appended
  or reconciled live facts; this is active state, not a one-time accidental
  overshoot.

## Executive finding

The 800-line limit is exposing a structural ownership defect, not merely excess
prose. Section 13 is a distinct scheduling-parity register occupying exactly 400
of the file's 800 lines (`docs/implementation-notes.md:324-723`). It has its own
gate, provenance, data model, machine parser, status lifecycle, and audience. The
rest of the file is the engineering document promised by the ownership table:
guards, deviations, invariants, and durable implementation decisions
(`docs/README.md:8-16`).

The preferred remedy is a lossless split: move §13 intact to a new living
`docs/scheduling-parity-ledger.md`, add that owner to `docs/README.md`, leave a
short link at §13's old location, and retarget the option-coverage checker. That
reduces implementation notes from 800 to approximately 407 lines, a net saving
of 393, without deleting one guard, deviation, obligation, decision, measurement,
reopening condition, or N4 gate fact.

Raising the existing cap would preserve the ownership problem and guarantee
another saturation. Do not raise it.

## 1. Contradictions and stale facts

### A. Live documents and source/checker contracts

1. **`touch_redundant` is simultaneously documented as shipping ON and OFF.**
   `[VERIFIED]` The deviation ledger says the likely-kept redundant band ships ON
   (`docs/implementation-notes.md:77`), while the parity row says it ships OFF
   (`docs/implementation-notes.md:430`). The code default is ON
   (`src/kernel/kernel.hpp:536`), the environment variable only overrides it
   (`src/main.cpp:260-263`), the wiring checker itself says ON
   (`tools/docs/check-config-wiring.py:92-94`), and
   `tests/test_subsume.cpp:311-314` asserts the ON default. The OFF sentence in
   §13 is stale.

2. **`vivify_flush` is ON in code and living notes but OFF in checker prose.**
   `[VERIFIED]` Implementation notes explicitly say it defaults ON
   (`docs/implementation-notes.md:189-192`), matching
   `src/kernel/kernel.hpp:590` and the override at `src/main.cpp:258-259`.
   `tools/docs/check-config-wiring.py:99-100` says “ships OFF, =1 enables.” The
   checker validates writer classification, not that explanatory string, so it
   remains green while publishing the wrong default.

3. **The Factor/BVE bound coupling is implemented, but three live records still
   say it is absent.** `[VERIFIED]` The current ledger says it is ported, passes
   the phase-entry snapshot to injected matchers, and is closed
   (`docs/implementation-notes.md:414`). Source does exactly that at
   `src/policies/default_policy.cpp:2150-2164`, and the irregular-ladder test
   verifies 0→3→5→13 propagation into Factor
   (`tests/test_slot_pose.cpp:93-157`). In conflict:

   - §13.1 still uses “`factor_bound` ... nothing writes it” as a present-tense
     exemplar (`docs/implementation-notes.md:382-389`).
   - The §13.3 heading and body say the bound “stays uncoupled,” “nothing writes
     it,” and the port is merely owed (`docs/implementation-notes.md:565-576`).
   - `check-config-wiring.py` classifies `factor_bound` as API-only and says the
     ramp is never connected (`tools/docs/check-config-wiring.py:135-142`).

   This is exactly the repository's known “field exists but nothing writes it”
   failure shape, now inverted: the writer exists but its documentary alarms did
   not retire.

4. **The §13.1 “six themes” table states closed historical defects as current.**
   `[VERIFIED]` It says private meters are still global
   (`docs/implementation-notes.md:386`), but the closure row says the global
   consumer is gone (`docs/implementation-notes.md:410`) and current meters read
   private counters (`src/policies/default_policy.cpp:560-579`). It says schedule
   order is still `strcmp`-emergent (`docs/implementation-notes.md:389`), while
   the closure row says authored order landed (`docs/implementation-notes.md:412`)
   and source contains the authored preprocessing order
   (`src/policies/default_policy.cpp:1355-1369`). It also says the cross-keyed
   Factor ramp was not ported (`docs/implementation-notes.md:388`), contradicting
   item 3. These themes remain useful historical diagnoses, but the live document
   needs them labelled historical or moved with the frozen provenance.

5. **Reason-side bump depth has two live specifications.** `[VERIFIED]` The
   durable decision says one reason level (`docs/implementation-notes.md:264`).
   The parity ledger says focused depth 1 and stable depth 2 were ported
   (`docs/implementation-notes.md:637`), and the wiring table describes a
   stable/focused depth (`tools/docs/check-config-wiring.py:56-57`). Current
   strategy config is a constant depth 1
   (`src/policies/default_strategy_config.hpp:71`); policy copies that value for
   analysis and resets it to 1 (`src/policies/default_policy.cpp:1514-1521`). The
   §13 row and checker description are stale.

6. **Phase 3 is both complete and not complete under the implementation notes'
   own exit definition.** `[VERIFIED]` PROGRESS marks phases 0–3 and the phase-3
   rule-completeness gate complete (`PROGRESS.md:144-149`). Implementation notes
   define that exit as including “N1-exposed” (`docs/implementation-notes.md:757-766`)
   but the same table marks M2, V6, and E2 N1 verification pending
   (`docs/implementation-notes.md:774,784,786`); PROGRESS also calls all three
   pending (`PROGRESS.md:10-15,255-257`). The stable plan's narrower phase-3 text
   requires implementation/reach or a no-implement disposition
   (`DEVELOPMENT_PLAN.md:223-226`) and does not say full N1 verification. One
   definition must win: either remove “N1-exposed” from the historical phase-3
   exit, or reopen the phase-3 status. This is a gate-semantics issue, not prose
   polish.

7. **The public API header falsely says Sweep has only a canonical instance.**
   `[VERIFIED]` `include/tactix.h:15-27` lists Sweep with Propagate and SwitchMode
   as canonical-only. Current Sweep enumerates every ranked seed and applies one
   seed (`src/rules/sweep/sweep.cpp:1698-1739`), and implementation notes say
   `instances()`/`sweep_try()` are complete (`docs/implementation-notes.md:193-194`).
   This is load-bearing N1 API documentation and demonstrates the missing owner
   for instance payload encodings discussed below.

8. **The source comment for `reorder.mode` reverses its numeric semantics.**
   `[VERIFIED]` `src/policies/default_strategy_config.hpp:130-133` says 1 means
   both modes and 2 stable-only. The live engineering decision says 1
   stable-only and 2 both (`docs/implementation-notes.md:273-276`). The actual
   predicate skips focused mode only when `mode < 2`, so the document is correct
   and the source comment is wrong (`src/policies/default_policy.cpp:416-423`).
   This is not a living-doc defect, but it is a documentary contradiction that a
   future config audit can easily propagate.

9. **`check-effort-specs.py` has stale, unchecked governor explanations.**
   `[VERIFIED]` Its table says Probe is 20‰, HyperBinaryRes is 20‰, Vivify still
   has `vivifythresh`, Factor still has `factorthresh`, and Sweep uses only
   `sweep_ticks_sub` (`tools/docs/check-effort-specs.py:68,71,74,76,80`). Actual
   Probe/HBR defaults are 8‰ (`src/policies/default_strategy_config.hpp:162-175`),
   the threshold mechanism was deleted (`docs/implementation-notes.md:527-532`),
   and Sweep's currency is the sum of sub-solver and representation ticks
   (`src/rules/sweep/sweep.hpp:58-63`). The script checks the parsed config and
   meter functions, not its rationale strings, so all 16 rows pass.

10. **§13.4's title and scope no longer describe its contents.** `[VERIFIED]`
    “Open gaps — MED / LOW” (`docs/implementation-notes.md:606-608`) contains
    mostly CLOSED/FIXED rows, a high-severity held candidate at line 616, and
    multiple later high findings at lines 621, 624-626, and 630-643. Rename it to
    a neutral disposition register after the split; do not infer openness from
    the heading.

11. **The may-not-gate set has an asserted owner but no owned record.**
    `[VERIFIED]` `docs/lessons.md:3-6,39-41` says PROGRESS owns the may-not-gate
    set. No occurrence of “may-not-gate” or equivalent named set exists in
    PROGRESS, the plan, or implementation notes. Either restore a current set in
    PROGRESS or remove the ownership claim. This is both a stale assertion and a
    missing-ownership finding.

### B. Frozen designs: stale by design, but important traps

These are not defects merely because their status is old: `docs/README.md:15,34-37`
explicitly says frozen designs do not own present truth. They are listed because
their present-tense claims are strong enough to mislead a reader or a lexical
checker, and because the option checker deliberately treats two of them as a
current corpus.

1. `[VERIFIED]` `docs/design/e2-factorsize-design.md:10-12,32-38` describes E2 as
   currently binary-only although its header says the generalized rule landed
   (`:3-4`) and current calculus/implementation use remainder grids. Its density
   gate recommendation (`:135-138`) is the opposite of the live invariant that
   the deleted gate must not return (`docs/implementation-notes.md:173-177`).

2. `[VERIFIED]` `docs/design/s2a-factor-matcher-design.md:86-90,191-199,287-293`
   repeatedly claims swappability proves N1. Its audit banner withdraws that
   claim and points to the live issue (`:10-14`). Keep the snapshot until the N1
   issue closes, exactly as the banner requires.

3. `[VERIFIED]` `docs/design/variable-retirement-design-v2.md:87-93` says Decide
   scans `k.nvars` and N1 is verified. Its audit banner says guided enumeration
   now walks H and may omit retired variables (`:3-7`), matching the actual
   focused path at `src/rules/decide/decide.cpp:84-89`. Preserve it until the
   obligation closes.

4. `[VERIFIED]` The first N4 audit and diagnosis intentionally retain superseded
   facts such as Factor demotion default-OFF and Factor-bound non-wiring
   (`docs/design/2026-08-04-n4-scheduling-parity-audit.md:291`;
   `docs/design/2026-08-04-scheduling-sophistication-diagnosis.md:48-66`). Their
   frozen banners are clear, but `check-option-coverage.py:288-304` counts any
   literal option occurrence in both files as current coverage. A stale sentence
   can therefore satisfy a live completeness check.

No other required frozen design produced a contradiction that is not already
contained by an explicit frozen/historical/companion banner.

## 2. Duplication and verbosity

Savings below are conservative net line estimates after leaving a short link or
current headline. Frozen evidence is not silently rewritten; any frozen-design
compaction needs explicit approval and an inbound-reference scan.

| Duplicate fact(s) | Current copies | Authoritative destination | Proposed links/merge | Net saving |
|---|---|---|---|---:|
| Documentation routing/ownership | `CLAUDE.md:3-7`; `PROGRESS.md:3-6`; `docs/implementation-notes.md:3-10`; `docs/lessons.md:3-6` | `docs/README.md:6-19` | Retain each file's one-sentence purpose and link the ownership table; keep the calculus/guard same-commit obligation in its owning skill/doc | ~11 lines |
| Full N1–N4 wording | `CLAUDE.md:11-16`; `DEVELOPMENT_PLAN.md:24-43` | `DEVELOPMENT_PLAN.md` for full requirements; CLAUDE remains the brief contract/index | Reduce CLAUDE to the four names plus a link; keep all exact semantics in plan §1.1 | ~4 lines |
| Reference-first DefaultPolicy rule | `CLAUDE.md:18-22`; `DEVELOPMENT_PLAN.md:48-60` | `DEVELOPMENT_PLAN.md` | CLAUDE links plan §2 | ~4 lines |
| Completed fuzz repairs and rule/policy audit | `PROGRESS.md:20-85`; recent changes at `:284-294`; implementation invariants at `docs/implementation-notes.md:124-144`; named reports | Reports own evidence; implementation notes own invariants; PROGRESS owns only current headline | Replace 66-line resume history with ~12 lines naming current pin, green state, reports, and next action | ~54 lines |
| Measurement protocol | `PROGRESS.md:107-133`; `DEVELOPMENT_PLAN.md:279-286,308-321,333-362` | `DEVELOPMENT_PLAN.md` | Keep only current paused/active window state in PROGRESS and link plan §8 | ~18 lines |
| Closed P4-CVG fixture results | `PROGRESS.md:175-192`; closure headline at `:87`; frozen closure report | Frozen report/run data own per-fixture evidence; PROGRESS owns closed/open headline | Replace eight result bullets with a 2–3 line closed headline and report link | ~15 lines |
| Immediate actions vs open items | `PROGRESS.md:94-105` and `:248-280`, especially `:250-257` | `PROGRESS.md` Resume block for ordered next actions | Remove repeated N4/N1/sparse-GC bullets from Open items and link Resume | ~6 lines |
| Rule catalog/disposition | Plan catalog `DEVELOPMENT_PLAN.md:187-226`; implementation inventory `docs/implementation-notes.md:757-800`; N1 status in PROGRESS | Plan owns catalog; calculus owns semantics; PROGRESS owns pending verification; implementation §16 owns X5's durable no-implement rationale | Replace §17 with a short disposition summary and make the checker compare plan/catalog, registry, calculus, and the explicit X5 record directly | ~34 lines |
| Factor generalization history | `docs/design/e2-factor-design.md`, `e2-factor-research.md`, `e2-factorsize-design.md`, calculus E2, implementation §5 | Calculus owns the rule; implementation notes own current engineering; reports own measurements | Compact only `e2-factorsize-design.md` after preserving its unique generalized proof argument and report links; current facts become links | ~90 lines in the frozen design |
| N1 pending state | `PROGRESS.md:10-15,99-102,255-257`; implementation rows `:41,52,57,774,784,786` | PROGRESS | Engineering rows retain only the durable deviation ID and semantics; status links to PROGRESS | ~7 lines after §17 removal |

`[INFERRED]` The largest avoidable living-doc repetition is therefore in
PROGRESS, not implementation notes: about 93 lines can be removed without losing
current tasks, gates, or evidence. That is separate from the §13 ownership split.

## 3. Missing or ambiguous ownership

1. **Scheduling-parity ledger.** `[VERIFIED]` It is half of implementation notes
   (`docs/implementation-notes.md:324-723`) but `docs/README.md:6-19` has no
   scheduling/default-policy register category. It is neither a guard/deviation
   note in the narrow engineering sense nor current status. Add an explicit
   living owner.

2. **Public rule-instance payload encodings.** `[VERIFIED]` The plan freezes the
   API shape (`DEVELOPMENT_PLAN.md:158-185`), the header attempts to enumerate
   payloads (`include/tactix.h:15-30`), rule source implements them, and calculus
   sometimes states the abstract instance, but no document owns the complete
   encoding table. The stale Sweep claim is the result. The public header should
   be authoritative for ABI encodings, generated/checked against each registry
   entry; calculus remains authoritative only for legal semantic instances.

3. **DefaultStrategy defaults and their rationale.** `[VERIFIED]` Exact values
   live in `src/policies/default_strategy_config.hpp`, while implementation
   notes, frozen designs, and checker TABLE strings all restate them. No ownership
   row says code is the value authority and the parity ledger is only the
   rationale/deviation authority. This caused the `vivify_flush`, reason-depth,
   Probe/HBR, and Factor-bound drift above.

4. **Rule-inventory disposition.** `[VERIFIED]` The plan owns the catalog, §17
   owns a duplicate disposition table, PROGRESS owns verification status, and
   `check-inventory.py` only connects plan text to §17 text. Assign the stable
   catalog to the plan, implemented truth to the registry, abstract truth to the
   calculus, no-implement rationale to a small explicit register, and current
   verification to PROGRESS.

5. **Machine-readable prose schemas.** `[INFERRED]` Invisible marker names,
   heading strings, table columns, roster punctuation, and exact status phrases
   are owned only by parser implementations. `docs/README.md` does not identify
   parser-owned regions. Add a “machine-parsed” convention and put an adjacent
   marker/schema warning at every such region; better, move structured data out
   of prose where practical.

6. **May-not-gate set.** `[VERIFIED]` Lessons assigns it to PROGRESS, but the set
   is absent (contradiction item 11).

7. **Cross-document synchronization of rule inventories.** `[VERIFIED]`
   `check-proof-rule-sync.py` compares calculus only with the HTML guide
   (`tools/docs/check-proof-rule-sync.py:17-34`); `check-inventory.py` compares
   plan tokens only with implementation prose (`tools/docs/check-inventory.py:32-52`);
   `check-effort-specs.py` compares the registry only with effort/fuzz tables.
   Nothing enforces registry ↔ plan catalog ↔ calculus ↔ public payload docs as
   one set. A mechanism can exist in source while one documentary surface omits
   it and all current checks remain green.

## 4. Machine-parser risks

### Parser-to-prose contracts

1. **Option coverage is the most fragile contract.**

   - `tools/docs/check-option-coverage.py:283-304` uses exact invisible begin/end
     markers and Python `str.index`; moving or deleting one aborts rather than
     producing a contextual schema error.
   - It treats the whole §13 block, two frozen designs, and a marked PROGRESS
     region as an undifferentiated lexical corpus (`:288-304`). Any option name,
     including a stale or negated historical claim, counts as CITED (`:440-453`).
   - CARRIED entries key on literal anchor strings, including frozen heading text
     such as `## 4. C3 Vivify` (`:98-213,409-429`). A harmless heading edit breaks
     coverage even if its content survives.
   - The §13.6 table parser requires the exact heading, row names, column order,
     pipes, and integer cells (`:307-326`).
   - The dismissal parser requires the exact bold headings, exactly three
     bullets, backtick token groups, parenthesized counts, an em dash with bold
     total, the literal `; cadical`, and the exact stop phrase
     (`:329-395`). Reasonable rewording or splitting a bullet breaks it.
   - The reference parser requires an exact `#define OPTIONS` continuation-chain
     shape and a hard-coded macro vocabulary (`:234-280`). It fails closed on a
     new macro spelling, which is good, but source reformatting is coupled to the
     docs checker.

   Preserve this checker, but point it at the new parity-ledger file and replace
   prose-table/roster parsing with a small checked data file or embedded fenced
   machine block from which the prose summary is generated.

2. **PROGRESS checking has already been silently defeated by editorial wording.**
   `[VERIFIED]` `check-progress.py:91-109` recognizes only the literal uppercase
   `NOTHING IN FLIGHT`; PROGRESS says “No Tactix benchmark job is in flight” at
   `PROGRESS.md:91-92`. Therefore the checker no longer validates claimed job IDs
   and merely warns when the queue is empty. Its tier logic still looks for an
   old banked Markdown row (`tools/docs/check-progress.py:146-160`); when no such
   row exists it only asks whether the date stamp appears anywhere, including a
   baseline or “unread” mention. The docstring promises stronger behavior at
   `:19-25` than the implementation now provides. It is also not registered in
   CTest (`tests/CMakeLists.txt:228-309` contains the other documentation tests).

3. **Document-size checks have two fail-open prose keys.**

   - N1–N4 are found only as bold tokens anywhere in CLAUDE
     (`tools/docs/check-doc-size.py:51-54`), not under Non-negotiables and not with
     their semantics.
   - Renaming/removing `## Recent changes` makes `partition(...)[2]` empty and
     reports zero recent entries (`:76-79`).
   - The hard cap has no warning/reserve band (`:38-41`), so exactly 800 is green
     even though the next invariant cannot be recorded.

4. **Inventory checking validates word occurrence, not rows or truth.**
   `check-inventory.py:26,35-47` requires an exact §17 heading, scans every
   plan table for `| M1 |`-shaped tokens rather than scoping §5.2, and accepts a
   family if its identifier occurs anywhere in the §17 suffix. A prose sentence,
   an evidence citation, or a malformed row can satisfy it. It explicitly does
   not test registry truth (`:11-15`).

5. **Proof-rule sync keys on exact presentation and hard-codes V6a.**
   `check-proof-rule-sync.py:20-22` recognizes only `### ID ·` headings, then adds
   V6a manually because it is a fourth-level heading. Changing heading depth or
   punctuation silently removes a calculus rule from the expected set; a matching
   omission in the HTML guide yields green. It never compares the registry.

6. **Section-reference resolution can resolve the wrong document.**
   `check-section-refs.py:43-52,83-112` recognizes a named `.md` only on the same
   line and within a 15-character connective. A line wrap or longer phrase turns
   it into a bare reference, which may then resolve against the current document
   or plan. A missing named target is skipped. The heading parser accepts only
   numeric ATX headings of depth 2–4 (`:55-71`).

7. **Citation checking is green with only 29.8% coverage.** `[VERIFIED]` The
   parser captures basenames only (`check-citations.py:39-44`) and skips any
   basename with multiple local candidates (`:77-87`). Current output was 64
   checked and 151 skipped. It tests only line ≤ EOF, not symbol/content identity
   (`:88-110`), excludes frozen designs, lessons, and the public header from its
   corpus (`:39-42`), and cannot use a directory-qualified citation to disambiguate
   because the regex discards the directory.

8. **Config-wiring has narrow source syntax and unchecked prose.**
   `check-config-wiring.py:166-184` starts at the literal
   `bve_retry_fixed_head`, accepts only four unsigned/bool scalar types with
   inline `=`, and stops at the first `\n};`. Enums, signed types, wrapper config,
   fields above the marker, or non-inline defaults are invisible. Writer detection
   recognizes only direct `field =` assignments (`:187-198`), not setters,
   aliases, compound writes, or constructor initialization. TABLE explanations
   are never validated, which allowed the stale defaults/coupling above.

9. **Effort checking can overwrite duplicates and misses helper-composed meters.**
   Registration and `EFFORT_SPECS` parsers are regex/dictionary based
   (`check-effort-specs.py:107-139`); duplicate names overwrite silently. It
   strips `//` but not block comments (`:98-105`), comma-splits rows, requires an
   exact positive literal `uint32_t` config initializer (`:142-151`), and checks
   hand-written rationale only by existence. This is why stale explanations stay
   green.

10. **Stat-ID checking has a concrete fail-open on Sweep's effort currency.**
    `check-stat-ids.py:134-149` extracts only direct `k.stats.X` reads from the
    meter body. `meter_sweep` delegates to `sweep_effort(k)`, so the parsed set is
    empty; the comparison runs only when that set is nonempty (`:253-262`). A
    wrong Sweep effort ID therefore passes. The brace matcher counts braces in
    raw text (`:118-131`) and the API parser requires exact `case N: ... return`
    layout plus a trailing macro-name comment (`:103-115,198-228`).

11. **Default-strategy config checking depends on an exact plan sentence.**
    `check-default-strategy-config.py:20-35` fails if the prose no longer contains
    “DefaultPolicy contains no magic strategy numbers.” Its banned-number list is
    finite (`:38-51`), so a new magic value outside that list silently passes.
    Source formatting and exact token uses are also hard-coded (`:53-69`).

12. **Rule/policy boundary checking is a useful heuristic, not a boundary proof.**
    `check-rule-policy-boundary.py:9-17` sees direct policy includes and
    `heuristics_*` calls only. The data-flow guard is a short hard-coded field list
    (`:18-36`), and the Sweep meter regex stops at the first `}` (`:38-41`). An
    alias, helper, neutral-looking kernel field, or different naming convention
    can evade it.

13. **Comment hygiene deliberately over-approximates and has a simplistic lexer.**
    It treats `//` inside strings as comments (`check-comment-hygiene.py:144-190`),
    does not handle escapes in its quote scanner, and its heavy-block detector
    counts only leading `//` runs (`:258-287`), not block comments. This tends to
    false positives rather than silent prose corruption, but the 359-line checker
    carries more narrative than several documents and should be simplified.

14. **`comment-only-diff.sh` can return success after checking nothing material.**
    New or deleted files are skipped (`tools/docs/comment-only-diff.sh:39-48`),
    and success depends only on `bad == 0` (`:63-66`), not on `skipped == 0` or
    `changed > 0`. Its comment says the approximate shell/CMake path can never
    miss a real change (`:12-15`), which is false for skipped files. Staged and
    unstaged name lists are concatenated (`:61`) and can duplicate work.

### General parser recommendation

`[INFERRED]` Keep prose readable and make schemas explicit. Machine-owned facts
should live in a small data block/file with a versioned schema; prose tables can
be generated or checked against it. Every remaining prose parser should have
negative fixture tests for heading rewrites, line wraps, malformed rows, missing
markers, duplicate entries, and “stale word occurs in historical sentence.”

## 5. Classification of every design document

`KEEP` means retain unchanged as a frozen record. `COMPACT` means preserve unique
evidence/obligations but replace redundant present-tense narrative with links in
a separately approved pass. `DELETE` still requires explicit approval and a final
inbound scan. Uncertainty defaults to KEEP.

| Design file | Class | Reason |
|---|---|---|
| `2026-08-04-n4-scheduling-parity-audit.md` | KEEP | Frozen provenance for the live N4 ledger and an explicit option-checker corpus; unique 216-mechanism audit evidence. |
| `2026-08-04-scheduling-sophistication-diagnosis.md` | KEEP | Frozen causal diagnosis, checker corpus, and source-reading rationale not reproduced by the live status. |
| `2026-08-05-g1-congruence-governor-design.md` | KEEP | Unique cost profile, N2 interruption argument, reachability evidence, and reopening test; cited by the live accepted deviation. |
| `2026-08-05-vivify-trail-reuse.md` | KEEP | Unique proof/soundness argument for reused prefixes and exact reach/A-B specification. |
| `2026-08-07-kissat-fidelity-revamp.md` | KEEP | No inbound basename references, but it preserves the user-restated fidelity decision, ordered increments, hard stops, and success criterion; uncertainty therefore keeps it. |
| `2026-08-07-reference-dosing.md` | KEEP | Primary cross-reference source study for the current five-layer dosing scheme. |
| `bve-tolerance-plan.md` | KEEP | Already a 24-line compact frozen result with one inbound reference and explicit reopening conditions. |
| `e1-bve-design.md` | KEEP | Already compact; preserves bound-before-mutation, proof order, and reconstruction obligations. |
| `e2-factor-design.md` | KEEP | Already compact; preserves the BVA ownership boundary and load-bearing proof/model invariants. |
| `e2-factor-research.md` | KEEP | Already compact historical source conclusion; it explicitly disclaims current ownership. |
| `e2-factorsize-design.md` | COMPACT | 138-line landed design contains stale present tense and a reversed density-gate conclusion; preserve §4's unique proof argument and measurements, target ~45–50 lines. |
| `h-relocation-design.md` | KEEP | Already compact and owns the durable null-H/rule-policy boundary rationale. |
| `independent-lineage-survey.md` | KEEP | Compact unique lineage evidence; seven still-open candidates are explicitly routed to PROGRESS. |
| `s2a-factor-matcher-design.md` | KEEP | Its banner explicitly requires preservation until `N1-FACTOR-INSTANCES` is resolved; unique policy-boundary design remains relevant. |
| `s2b-composite-grids-design.md` | KEEP | Already compact and preserves termination/proof boundaries for composite grids. |
| `s4-factor-dosing-design.md` | DELETE | Zero inbound basename references; all 24 lines restate the current SP-FACTOR-1 ledger or say reports own evidence, and it owns no action. Delete only after final exact-reference scan/approval. |
| `s6-walk-design.md` | KEEP | Already compact; durable H-only contract and deviations are not fully represented elsewhere. |
| `v5-sweep-design.md` | KEEP | Already compact and heavily referenced; owns the embedded-solver/proof-translation boundary. |
| `variable-retirement-design-v2.md` | KEEP | Its banner explicitly requires preservation until `N1-RETIRED-DECIDE` closes; unique measured-dead alternative and retirement design remain useful. |

Inbound basename scan counts were respectively 2, 5, 2, 2, 0, 2, 1, 3, 2, 1,
1, 1, 2, 3, 2, 0, 3, 6, and 2 in the table's order. The two zeroes were read in
full before classification.

## 6. Classification of every `tools/docs/` checker

Here `COMPACT` means retain the enforcement goal while simplifying or replacing
the fragile parser and stale narrative; it does not mean weaken the check.

| Checker | Class | Reason |
|---|---|---|
| `check-citations.py` | COMPACT | Keep citation validation, but basename-only resolution currently skips 151/215 citations; accept qualified paths/symbol anchors and report a minimum coverage ratio. |
| `check-comment-hygiene.py` | COMPACT | Useful policy, but 359 lines and ad-hoc lexing duplicate extensive rationale; keep rules, shorten narrative, and use language-aware extraction where needed. |
| `check-config-wiring.py` | COMPACT | Essential anti-unwired-field gate, but narrow declarations/writer regexes and stale TABLE prose need a typed manifest or AST/compile-time source. |
| `check-default-strategy-config.py` | KEEP | Small, useful guard for the typed config surface; replace the exact plan-sentence dependency and grow beyond a finite literal blacklist. |
| `check-doc-size.py` | COMPACT | Keep budgets, add warning/reserve bands and explicit split-ledger budgets, and make required headings/sections fail closed. |
| `check-effort-specs.py` | COMPACT | Essential registry↔governor↔fuzz coverage, but deduplicate parsing and remove unchecked/stale rationale from executable TABLE data. |
| `check-inventory.py` | COMPACT | Retain the phase-3 inventory gate but compare structured catalog/registry/calculus/no-implement records, not token occurrence in §17 prose. |
| `check-option-coverage.py` | KEEP | Critical exhaustive reference-option invariant; migrate it with §13 and move its table/roster to explicit structured data without reducing coverage. |
| `check-progress.py` | COMPACT | Rewrite around the current headline format and explicit state markers; exact idle phrase and obsolete banked-row logic already fail open. |
| `check-proof-rule-sync.py` | KEEP | Small and valuable; extend to registry/catalog and derive V6a rather than hard-coding it. |
| `check-rule-policy-boundary.py` | KEEP | Small useful architectural tripwire; document heuristic limits and add cases for aliases/helpers. |
| `check-section-refs.py` | KEEP | Useful wrong-section guard; require explicit cross-doc syntax and fail on missing named targets instead of fallback/skip. |
| `check-stat-ids.py` | KEEP | Important ABI/reward-channel check; fix helper-composed meters (especially Sweep), duplicate detection, and parser robustness. |
| `comment-only-diff.sh` | COMPACT | Retain the utility, but make new/deleted/skipped files fail closed and correct the “never miss” claim. |

No checker is recommended for deletion. Their goals are distinct; the defects are
in schemas, coverage, and duplicated explanatory state.

## 7. Concrete implementation-notes compaction and budget answer

### Preferred compaction: split §13 intact

Current section sizes, measured from headings:

| Material moved from `docs/implementation-notes.md` | Current lines | Destination | Net lines freed from implementation notes |
|---|---:|---|---:|
| §13 preamble/provenance/gate, lines 324-377 | 54 | New parity ledger introduction; retain N4 gate, cross-solver divergence contract, provenance, lower-bound warning, and reference-reading rules | 47 after charging a 7-line link stub here |
| §13.1 themes, lines 378-390 | 13 | Parity ledger “historical diagnostic themes,” clearly labelled as historical and linked to current dispositions | 13 |
| §13.2 high-severity dispositions, lines 391-472 | 82 | Parity ledger current disposition table | 82 |
| §13.3 accepted deviations, lines 473-605 | 133 | Parity ledger accepted-deviation register, unchanged | 133 |
| §13.4 later MED/LOW/high dispositions, lines 606-646 | 41 | Parity ledger disposition table, with neutral heading | 41 |
| §13.5 corrections, lines 647-663 | 17 | Parity ledger audit-corrections register | 17 |
| §13.6 coverage table/roster/stopping boundary, lines 664-703 | 40 | Parity ledger machine-parsed coverage section | 40 |
| §13.7 application-order audit, lines 704-722 | 19 | Parity ledger lifecycle-order section | 19 |
| Separator blank, line 723 | 1 | Removed | 1 |
| **Total** | **400** | | **393 net** |

The 7-line stub should say only that scheduling parity has its own living ledger,
that plan N4 and the phase-4 exit depend on it, and where the checker reads it.
The `<!-- parity-ledger:begin/end -->` markers move with the content.

No semantic fact is deleted. Specifically:

- N1–N4 remain unchanged in CLAUDE/plan; the N4 verification gate moves with the
  ledger and the stub links it.
- All open/closed/accepted findings, measurements, port traps, removal/reopening
  paths, ownership, and “must not cite” constraints move verbatim before any
  later editorial pass.
- All five accepted deviations and their full proof/cost contracts remain.
- §13.5 corrections remain a live warning against repeating bad source readings.
- §13.6's 418-option coverage arithmetic, three dismissal groups, and explicit
  stopping boundary remain machine checked.
- §13.7's lifecycle decisions and reopening conditions remain.
- Sections 1–12 and 14–17 stay in implementation notes, preserving every guard,
  engineering deviation, standing invariant, proof obligation, and durable
  non-scheduling design decision.

`check-option-coverage.py` must change only its `NOTES`/ledger target and section
lookup; its frozen audit/diagnosis and PROGRESS track corpus remain. The N4 ledger
must be added to `check-doc-size.py`, `check-citations.py`,
`check-section-refs.py`, and `check-progress.py`'s living-doc lists.

### Budget recommendation

`[INFERRED]` Split; do not raise 800.

After the split, implementation notes are approximately 407 lines. Set its hard
cap to **520**, with a warning at **470**. That leaves about 113 hard-cap lines
and 63 warning-free lines for new guards/invariants while keeping the engineering
file readable. Give the parity ledger a separate **600-line hard cap** with a
warning at **520**; it begins around 408 lines after adding its title/ownership
header. The exact warning numbers may be adjusted once formatted, but both files
must have explicit reserve, not merely “current length ≤ cap.”

If a split is rejected, the next-best lossless plan is still to move §13.2–§13.7
to a companion ledger and leave §13/§13.1 as an index; that frees at least 338
lines. Raising the monolith to 900 or 1000 is not recommended: it leaves two
owners and one fragile parser embedded in a file whose advertised role is guards
and engineering decisions.

### Secondary compaction after the split

Do not combine this with the structural move unless review bandwidth permits.
Once the new ledger is green, compact PROGRESS by the ~93 lines itemized in §2,
then redesign §17 for another ~34 lines. These changes are lower priority than
the split because they require editorial judgment about current headlines and
the phase-3 gate. Preserve the current self-contained next action exactly:
complete N4 verification, restore N1 evidence for Factor/Congruence/retired
Decide, then Tier D and T3 (`PROGRESS.md:94-105`).

## 8. Proposed validations after a patch lands

### Before editing

1. Record `git status --short --untracked-files=all` and verify the only pre-existing
   changes are the declared CLAUDE and DEVELOPMENT_PLAN additions.
2. Scan inbound basename and exact-path references for every moved/compacted
   design and for `implementation-notes.md §13`.

### Documentation checks

Run every checker directly with bytecode writes disabled, because
`check-progress.py` and `comment-only-diff.sh` are not ordinary CTest coverage:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-citations.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-comment-hygiene.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-config-wiring.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-default-strategy-config.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-doc-size.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-effort-specs.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-inventory.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-option-coverage.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-progress.py --warn
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-proof-rule-sync.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-rule-policy-boundary.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-section-refs.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/docs/check-stat-ids.py
```

Run `tools/docs/comment-only-diff.sh HEAD` only if the patch also changes source
comments, and treat any skipped file as failure until that utility is repaired.

Also run the registered documentation tests from the existing configured build:

```sh
ctest --test-dir build --output-on-failure \
  -R 'config_wiring|effort_specs|option_coverage|stat_ids|citations|section_refs|inventory|comment_hygiene|doc_size|proof_rule_sync|default_strategy_config_check|rule_policy_boundary'
```

Use the actual configured build directory if `build/` is not the active one.

### Semantic and negative tests

1. Add parser fixture tests before calling the migration safe: missing/duplicated
   ledger markers, renamed headings, reordered table columns, wrapped named
   section references, duplicate registry/spec rows, a stale option named only in
   historical prose, and a helper-composed effort meter.
2. Add a cross-inventory check: registered proof rules/policy ops ↔ plan §5.2 ↔
   calculus headings ↔ HTML guide cards ↔ public payload documentation. X5 and
   internal V6a must be explicit typed exceptions, not hard-coded surprise rows.
3. Add assertions for the facts that drifted: `touch_redundant=true`,
   `vivify_flush=true`, reason depth 1 (or deliberately change the implementation
   and decision together), Factor receives the posed BVE bound, Reorder 0/1/2
   semantics, and Sweep exposes seed instances.
4. Run targeted tests covering those contracts: `test_subsume`, `test_vivify`,
   `test_slot_pose`, `test_scheduling`, `test_reorder`, `test_sweep`,
   `test_api_instances`, and `test_analyze`, then the full unit suite.
5. For any calculus/guard/API semantic correction, run both DRAT and native-LRAT
   proof checks plus SAT model validation and the relevant N1 adversarial API
   test. Documentation-only relocation does not authorize changing semantics.

### Final audit

1. Recount both new budgets and require reserve below the warning thresholds.
2. Confirm the option checker still reports 161 Kissat + 257 CaDiCaL options,
   83 dismissed, 125 carried, 208 cited, and 2 explicit exemptions, unless a
   reviewed source/reference change explains the difference.
3. Confirm the report/diff contains no edits to the two user-owned uncommitted
   additions and no unrelated files.
4. Read the rendered new ledger once without checker knowledge: all active
   deviations, gates, reopening paths, N1–N4 constraints, and the self-contained
   next step must remain intelligible to a human.

## Final priority order

1. Split §13 and establish explicit ownership/budgets.
2. Correct the live factual contradictions and the public Sweep payload comment.
3. Repair fail-open parser contracts, starting with progress state, citation
   coverage, inventory truth, and helper-composed meters.
4. Compact PROGRESS history and §17 only after their authoritative destinations
   and phase-3/N1 gate semantics are decided.
5. Compact/delete frozen designs only in a separately approved pass.

This order preserves all active requirements, exact rule semantics, proof
obligations, current tasks, gates, accepted deviations, durable decisions, and
the current next step while immediately eliminating the 800/800 writing failure.
