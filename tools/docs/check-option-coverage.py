#!/usr/bin/env python3
"""SCHED-A3: every REFERENCE option must be accounted for by the parity ledger.

WHY THIS EXISTS. The scheduling-parity ledger (`docs/implementation-notes.md`
§13) is only as good as the set it was generated from. Pass 1 enumerated OUR
rule families, so it could not surface a mechanism we never knew to look for.
Pass 2 (SCHED-A3) regenerated the row list from the REFERENCE's option sets —
`kissat/src/options.h` and `cadical/src/options.hpp` — and that is what found
the four HIGH gaps §13.2 marks *(A3)* and the six corrections in §13.5.

That pass is a snapshot. A reference bump adds an option and the ledger goes
back to being a lower bound with no sign that it slipped. This check turns the
snapshot into an invariant: **enumerate the reference option sets from source,
and fail on any option the ledger does not account for.**

    DISMISSED  the option is named in §13.6's dismissal roster (parsed FROM the
               doc, not restated here) with that roster's reason. Build/IO/debug
               knobs, whole techniques off at their own default, kissat's
               structural BVA.
    CARRIED    an explicit roster entry BELOW, naming the ledger row or audit
               family section that covers it. Needed because a family row covers
               a family of options without spelling every member out — e.g.
               SP-EFFORT-CEIL-1 covers `NAMEmineff`/`NAMEmaxeff` as a scheme.
               Each entry names an ANCHOR string that must still be present in
               the corpus, so deleting the covering row fails HERE.
    CITED      the option's name appears literally in the ledger corpus (§13 +
               the frozen audit + the diagnosis + the frozen Track SCHED
               archive). This channel is derived, never restated, so it cannot
               go stale.

An option in NONE of the three fails. That is the whole point: a new option in a
future reference bump must fail rather than pass silently.

WHAT THIS CHECK IS NOT. It is a COVERAGE check, not a semantic one. It cannot
know whether a row's claim about an option is true — §13.5 exists because six of
them were not, and the standing method note applies to every future entry here:
read the option at its USE SITE, through macros (cadical's `factorthresh` is
token-pasted via `opts.NAME##thresh`, so the identifier never appears literally
and reading it as dead code produced two wrong published claims), and never
infer polarity from a name (kissat's `factordelay`=0 is the MOST delayed
setting).

Three further failure modes, because a roster rots in both directions:
  - a DISMISSED or CARRIED name that is no longer a reference option (stale);
  - an option macro spelling in the reference header that this file does not
    enumerate (silent under-count of the very set being checked);
  - §13.6's published coverage table disagreeing with the live enumeration.

    tools/docs/check-option-coverage.py        report; exit 1 on any failure
"""
import fnmatch
import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True, check=True).stdout.strip()
NOTES = os.path.join(ROOT, "docs", "implementation-notes.md")
# The parity ledger was section 13 of the notes until it grew to half that
# file. Its headings, row ids and markers moved verbatim, so only the path
# changed here.
LEDGER = os.path.join(ROOT, "docs", "scheduling-parity-ledger.md")
SCHED_TRACK_ARCHIVE = os.path.join(
    ROOT, "docs", "log", "2026-09-19-pre-am311-progress.md")
AUDIT = os.path.join(ROOT, "docs", "design",
                     "2026-08-04-n4-scheduling-parity-audit.md")
DIAGNOSIS = os.path.join(ROOT, "docs", "design",
                         "2026-08-04-scheduling-sophistication-diagnosis.md")

# The reference clones are untracked (CLAUDE.md) and live at the top of the
# SHARED checkout, so a git worktree must look one level out: `--git-common-dir`
# points into the shared `.git`, whose parent is that checkout.
def reference_root():
    common = subprocess.run(["git", "rev-parse", "--git-common-dir"],
                            capture_output=True, text=True,
                            check=True).stdout.strip()
    shared = os.path.dirname(os.path.abspath(common))
    for cand in (ROOT, shared):
        if os.path.exists(os.path.join(cand, "kissat", "src", "options.h")):
            return cand
    return ROOT


REFROOT = reference_root()
KISSAT_H = os.path.join(REFROOT, "kissat", "src", "options.h")
CADICAL_H = os.path.join(REFROOT, "cadical", "src", "options.hpp")

# The macro spellings each header uses inside its `#define OPTIONS` block. A
# line in that block matching NONE of these is a failure, not a skip: a new
# spelling would silently shrink the set this whole check is taken over.
KISSAT_MACROS = ("OPTION", "DBGOPT", "EMBOPT", "LOGOPT", "NQTOPT")
CADICAL_MACROS = ("OPTION", "LOGOPT", "QUTOPT")

# ---------------------------------------------------------------------------
# The CARRIED roster.
#
# Each entry: anchor -> (reason, {"kissat": [...], "cadical": [...]}).
# The ANCHOR must appear verbatim in the ledger corpus; deleting the covering
# row therefore fails here rather than orphaning these options in silence.
# Names are EXACT — no globs anywhere in this file's rosters, because a glob is
# how a newly added option slips through a family entry unnoticed.
# ---------------------------------------------------------------------------
CARRIED = {
    "SP-EFFORT-CEIL-1": (
        "the two effort schemes are audited as SCHEMES, not option by option: "
        "the row names the `NAMEmineff`/`NAMEmaxeff` clamps and their per-"
        "technique constants, and the missing upper clamp IS the gap",
        {"cadical": ["elimeffort", "elimlimited", "elimmaxeff", "elimmineff",
                     "subsumeeffort", "subsumelimited", "subsumemaxeff",
                     "subsumemineff", "transredmaxeff", "transredmineff",
                     "walkmineff"]}),
    "SP-TIER-2": (
        "the dynamic-tier row is about the CONSUMER count, so it covers the "
        "whole tier-computation option family on both sides",
        {"cadical": ["recomputetier", "reducetier1glue", "reducetier2glue",
                     "tier1limit", "tier1minglue", "tier2limit",
                     "tier2minglue"]}),
    "SP-PREPROC-2": (
        "the row names kissat's reduced preprocessing pipeline, which IS the "
        "`preprocessX` switch set",
        {"kissat": ["preprocessbackbone", "preprocesscongruence",
                    "preprocessfactor", "preprocessprobe", "preprocessweep"]}),
    "SP-BUMP-2": (
        "the reason-side-bumping row covers the bump gate ladder on both sides",
        {"kissat": ["bumpreasons"], "cadical": ["bumpreason", "bumpreasonrate"]}),
    "SP-ARENA-1": (
        "the arena-order row covers cadical's arena layout switches",
        {"cadical": ["arenacompact"]}),
    "sub-limit ramps": (
        "§13.6 item 3 — the sweep sub-limit ramps were read in full and have no "
        "counterpart to compare against; the audit's inventory row is the home",
        {"kissat": ["sweepclauses", "sweepcomplete", "sweepdepth",
                    "sweepfliprounds", "sweepmaxclauses", "sweepmaxdepth",
                    "sweepmaxvars", "sweepvars"],
         "cadical": ["sweepclauses", "sweepcomplete", "sweepcountbinary",
                     "sweepdepth", "sweepfliprounds", "sweepmaxclauses",
                     "sweepmaxdepth", "sweepmaxvars", "sweepvars"]}),
    "minimize depth": (
        "§13.3 accepted deviation — ported and measured byte-identical across "
        "the whole reference range",
        {"kissat": ["minimizedepth"], "cadical": ["minimizedepth"]}),
    "classification.small": (
        "§13.5 C3 — kissat's formula classification, read at its use site and "
        "found write-only on this half",
        {"kissat": ["smallclauses"]}),
    "elimboundmax": (
        "the BVE bound-ramp rows cover the ramp's own endpoints",
        {"cadical": ["elimboundmin"]}),
    "fastel": (
        "the preprocessing-admission and warm-up rows are exactly the fast-"
        "elimination pass on both sides",
        {"kissat": ["fastelclslim", "fasteloccs", "fastelrounds", "fastelsub"],
         "cadical": ["fastelimbound", "fastelimclslim", "fastelimocclim",
                     "fastelimrounds"]}),
    "duplicate-binary": (
        "the round-entry duplicate-binary pre-pass row covers cadical's "
        "deduplication switch",
        {"cadical": ["deduplicate"]}),
    "chronolevels": (
        "§13.6 confirmed at parity — the chronological-backtracking level cap",
        {"cadical": ["chronolevelim"]}),
    "SP-RANDEC-1": (
        "the randec row states the reference defaults it was ported at; the "
        "interval seed is part of that same mechanism",
        {"kissat": ["randecinit"], "cadical": ["randecinit"]}),
    "## 1. E1 BVE": (
        "audit family section — BVE's dose knobs are audited as rows there",
        {"kissat": ["eliminateeffort"]}),
    "## 2. E2 Factor / BVA": (
        "audit family section — Factor's grid-size bound is audited there",
        {"kissat": ["factorsize"], "cadical": ["factorsize"]}),
    "## 3. C1 SubsumeForward + C2 Strengthen": (
        "audit family section — the fused-pass and dose rows cover these",
        {"kissat": ["forwardeffort"], "cadical": ["subsumestr"]}),
    "## 4. C3 Vivify": (
        "audit family section — the tier-band split, candidate ranking and dose "
        "rows cover the whole vivify option family",
        {"kissat": ["vivifyeffort", "vivifyirr", "vivifysort", "vivifytier1",
                    "vivifytier2", "vivifytier3"],
         "cadical": ["vivifydemote", "vivifyeffort", "vivifyirred",
                     "vivifyirredeff", "vivifytier1", "vivifytier1eff",
                     "vivifytier2", "vivifytier2eff", "vivifytier3"]}),
    "## 5. V6 Congruence (+ V3 Substitute)": (
        "audit family section, refined by §13.5 C7 — the per-gate-kind "
        "extraction switches and arity limits",
        {"kissat": ["congruenceands", "congruencebinaries", "congruenceites",
                    "congruenceonce", "congruencexors"],
         "cadical": ["congruenceand", "congruencebinaries", "congruenceite",
                     "congruencexor"]}),
    "SP-SUBST-2": (
        "the Substitute budget row covers both references' two-round caps",
        {"cadical": ["decomposerounds"]}),
    "## 6. V1 Probe / V2 Backbone / V4 TransitiveReduce": (
        "audit family section — the probing-family round caps and interval seeds",
        {"kissat": ["backbonemaxrounds", "probeinit"],
         "cadical": ["backbonemaxrounds"]}),
    "## 8. Reduce": (
        "audit family section — the reduce schedule and its variants",
        {"cadical": ["reduceopt"]}),
    "## 9. Restart / SwitchMode / Rephase / Walk / Decide / Analyze / Reorder": (
        "audit family section — restart/mode/rephase/walk/phase machinery is "
        "audited there row by row, including the EMA warm-up and the reluctant "
        "sequence",
        {"kissat": ["emafast", "forcephase", "modeinit", "phasesaving",
                    "reluctantint", "reluctantlim", "reorderinit",
                    "reordermaxsize", "rephaseinit", "rephaseint", "restartint",
                    "restartmargin", "restartreusetrail"],
         "cadical": ["emadecisions", "emagluefast", "emaglueslow", "forcephase",
                     "reluctantint", "reluctantmax", "rephaseint", "restartint",
                     "restartmarginfocused", "restartreusetrail",
                     "stabilizeinit", "walknonstable"]}),
    "Lucky": (
        "our `Lucky` rule is the counterpart and the audit reaches these only "
        "through its `search_ticks` scope row; neither side exposes a dose or "
        "an order knob for it",
        {"kissat": ["lucky", "luckyearly", "luckylate"],
         "cadical": ["lucky", "luckyearly", "luckylate"]}),
}

# ---------------------------------------------------------------------------
# The one exemption this file grants on its own authority, named and reasoned
# rather than hidden by loosening the check (the judgement A2 made when it
# tagged `factor_bound` API rather than POLICY). These are §13.6 dismissal-
# roster SHAPE — not scheduling at all — that the roster does not list, so they
# cannot be resolved by parsing the doc. Two options; enumerate, never glob.
# ---------------------------------------------------------------------------
EXEMPT = {
    ("cadical", "radixsortlim"): "sort-implementation cutoff; not a schedule",
    ("cadical", "terminateint"): "termination-callback cadence; API plumbing",
}


def die(msg):
    print(f"  FAIL  {msg}")
    print("check-option-coverage: aborted")
    sys.exit(1)


def options_block(path, macros, label):
    """Every option name in the header's `#define OPTIONS` block.

    Fails on a line the macro set does not cover, rather than skipping it.
    """
    if not os.path.exists(path):
        die(f"reference source {path} is missing. This check is taken OVER the "
            f"reference option sets, so it cannot run without them — clone "
            f"kissat and cadical beside the checkout (CLAUDE.md: untracked "
            f"reference clones). Skipping would make it vacuously green, which "
            f"is worse than red.")
    lines = open(path).read().split("\n")
    start = next((i for i, l in enumerate(lines)
                  if l.startswith("#define OPTIONS")), None)
    if start is None:
        die(f"{label}: no `#define OPTIONS` block in {path} — the header was "
            f"restructured and this parser is measuring the wrong set.")
    names, bad = [], []
    pat = re.compile(r"^\s*(" + "|".join(macros) + r")\s*\(\s*(\w+)\s*,")
    for raw in lines[start + 1:]:
        line = raw
        if not raw.rstrip().endswith("\\"):
            break                       # the continuation chain ends here
        line = re.sub(r"\\\s*$", "", line)
        line = re.sub(r"/\*.*?\*/", "", line)
        if not line.strip():
            continue
        m = pat.match(line)
        if m:
            names.append(m.group(2))
        else:
            bad.append(line.strip())
    # The last option line carries no trailing backslash, so pick it up here.
    for raw in lines[start + 1:]:
        m = pat.match(raw)
        if m and m.group(2) not in names:
            names.append(m.group(2))
        if not raw.rstrip().endswith("\\") and raw.strip():
            break
    if bad:
        die(f"{label}: {len(bad)} line(s) in the OPTIONS block match none of the "
            f"macro spellings {macros} — first: {bad[0]!r}. A spelling this file "
            f"does not know silently shrinks the set the whole check is taken "
            f"over, so it fails instead of skipping.")
    if len(set(names)) != len(names):
        die(f"{label}: duplicate option names in the OPTIONS block.")
    return names


def section(text, start_marker, end_marker):
    i = text.index(start_marker)
    return text[i:text.index(end_marker, i)]


def ledger_corpus():
    """§13 + the frozen audit + the diagnosis + the frozen Track SCHED.

    The completed scheduling track remains in the corpus because §13 names it
    as the remediation home: a CLOSED track item is the analogue a row would
    otherwise describe.  PROGRESS compaction froze that track verbatim in the
    dated log.  Invisible marker comments delimit it so prose headings may move
    without silently changing option coverage.
    """
    notes = open(LEDGER).read()
    s13 = section(notes, "<!-- parity-ledger:begin -->",
                  "<!-- parity-ledger:end -->")
    track_archive = open(SCHED_TRACK_ARCHIVE).read()
    track = section(track_archive, "<!-- track-sched:begin -->",
                    "<!-- track-sched:end -->")
    return s13 + open(AUDIT).read() + open(DIAGNOSIS).read() + track


def coverage_table():
    """§13.6's published per-source coverage numbers."""
    notes = open(LEDGER).read()
    s = section(notes, "### 13.6 Coverage of the SCHED-A3 pass", "\n**Dismissal")
    out = {}
    for src, key in (("kissat/src/options.h", "kissat"),
                     ("cadical/src/options.hpp", "cadical"),
                     ("total", "total")):
        m = re.search(r"^\|\s*`?" + re.escape(src) + r"`?\s*\|(.+)$", s, re.M)
        if not m:
            die(f"§13.6's coverage table has no row for `{src}` — the table is "
                f"the ledger's own statement of what it covered, so a reshape "
                f"must be re-read here, not parsed past.")
        cells = [c.strip().strip("*") for c in m.group(1).split("|")]
        try:
            out[key] = (int(cells[0]), int(cells[3]))   # options, dismissed
        except (IndexError, ValueError):
            die(f"§13.6's coverage row for `{src}` no longer parses as "
                f"(options | newly read | carried | dismissed).")
    return out


def dismissal_roster(kissat, cadical):
    """The 3 dismissal groups, parsed FROM §13.6 (one fact, one home).

    Globs are expanded against the LIVE option set and the group's published
    count re-checked, so a reference bump that adds a `block*` option fails
    here instead of hiding inside the glob.
    """
    notes = open(LEDGER).read()
    s = section(notes, "**Dismissal roster**", "\n**What A3 did NOT reach")
    bullets = [b for b in s.split("\n- ")[1:]]
    if len(bullets) != 3:
        die(f"the dismissal roster has {len(bullets)} bullets, expected 3 "
            f"(NOT_SCHEDULING · technique-off-by-default · kissat structural "
            f"BVA). Re-read the roster here before trusting a green run.")
    fails, out = [], set()

    def expand(tok, pool, src):
        hits = [o for o in pool if fnmatch.fnmatch(o, tok)]
        if not hits:
            fails.append(f"dismissal roster names `{tok}` for {src}, which "
                         f"matches NO option in that reference — stale entry, "
                         f"or the option was renamed by a reference bump.")
        return [(src, h) for h in hits]

    def tokens_with_counts(text, pool, src):
        """`a`(4), `b`+`c`(5), `d e f` -> groups whose published counts hold.

        A backtick block may hold one token or a whitespace-separated list; a
        `(n)` closes the group accumulated since the last one.
        """
        got, pending = [], []
        for m in re.finditer(r"`([\w*][\w* ]*)`(?:\s*\((\d+)\))?", text):
            for tok in m.group(1).split():
                pending.extend(expand(tok, pool, src))
            if m.group(2):
                if len(pending) != int(m.group(2)):
                    fails.append(
                        f"dismissal group ending at `{m.group(1)}` publishes "
                        f"{m.group(2)} options but expands to {len(pending)} "
                        f"({sorted(n for _, n in pending)}) — the reference set "
                        f"moved under the roster.")
                got.extend(pending)
                pending = []
        got.extend(pending)
        return got

    # Each bullet is `<group> — <bold count>: <the list>. <rationale prose>`,
    # and the rationale cites option names too (`vivifyretry`/`vivifyonce`), so
    # the list is cut at the first sentence break closing a backtick or a count.
    bullets = [re.split(r"(?<=[`)])\.\s", b, maxsplit=1)[0] for b in bullets]

    # bullet 1: explicit names, kissat segment then cadical segment
    b = bullets[0]
    kseg, _, cseg = b.partition("; cadical")
    out.update(tokens_with_counts(kseg.partition("kissat")[2], kissat, "kissat"))
    out.update(tokens_with_counts(cseg, cadical, "cadical"))
    # bullet 2: cadical only, glob groups with per-group counts
    out.update(tokens_with_counts(bullets[1], cadical, "cadical"))
    # bullet 3: kissat only
    out.update(tokens_with_counts(bullets[2], kissat, "kissat"))

    for b, label in ((bullets[0], "roster bullet 1"), (bullets[1], "bullet 2"),
                     (bullets[2], "bullet 3")):
        if not re.search(r"—\s*\*\*\d+\*\*", b):
            fails.append(f"{label} publishes no group total — the roster's own "
                         f"arithmetic is what makes it auditable.")
    return out, fails


def main():
    fails, notes_out = [], []
    kissat = options_block(KISSAT_H, KISSAT_MACROS, "kissat")
    cadical = options_block(CADICAL_H, CADICAL_MACROS, "cadical")
    live = {"kissat": kissat, "cadical": cadical}

    dismissed, fails_r = dismissal_roster(kissat, cadical)
    fails += fails_r

    corpus = ledger_corpus()

    # CARRIED: the anchor must still exist, and the names must still be options.
    carried = {}
    for anchor, (why, byref) in CARRIED.items():
        if anchor not in corpus:
            fails.append(f"CARRIED roster entry `{anchor}` ({why}) — that anchor "
                         f"is GONE from the ledger corpus, so the "
                         f"{sum(len(v) for v in byref.values())} option(s) it "
                         f"covers are orphaned. Restore the row or re-home them.")
        for src, names in byref.items():
            for n in names:
                if n not in live[src]:
                    fails.append(f"CARRIED entry `{anchor}` lists {src} option "
                                 f"`{n}`, which no longer exists — stale row.")
                if (src, n) in carried:
                    fails.append(f"{src} option `{n}` is carried twice "
                                 f"(`{anchor}` and `{carried[(src, n)]}`).")
                if (src, n) in dismissed:
                    fails.append(f"{src} option `{n}` is BOTH dismissed by "
                                 f"§13.6's roster and carried by `{anchor}` — "
                                 f"two homes for one fact; drop one.")
                carried[(src, n)] = anchor

    for src, n in EXEMPT:
        if (src, n) in dismissed:
            fails.append(f"{src} option `{n}` is EXEMPT here and also dismissed "
                         f"by §13.6's roster — drop the exemption, the doc "
                         f"covers it.")
        if n not in live[src]:
            fails.append(f"EXEMPT lists {src} option `{n}`, which no longer "
                         f"exists — stale exemption.")

    tally = {"dismissed": 0, "carried": 0, "cited": 0, "exempt": 0}
    unaccounted = []
    for src in ("kissat", "cadical"):
        for n in live[src]:
            if (src, n) in dismissed:
                tally["dismissed"] += 1
            elif (src, n) in carried:
                tally["carried"] += 1
            elif (src, n) in EXEMPT:
                tally["exempt"] += 1
            elif re.search(r"\b" + re.escape(n) + r"\b", corpus):
                tally["cited"] += 1
            else:
                unaccounted.append((src, n))

    for src, n in unaccounted:
        fails.append(f"UNACCOUNTED reference option `{src}:{n}` — it is in "
                     f"{os.path.basename(KISSAT_H if src == 'kissat' else CADICAL_H)}"
                     f" and the parity ledger says NOTHING about it. Read it at "
                     f"its USE SITE (through macros — token-pasted names never "
                     f"appear literally), then either give it a §13 row, add it "
                     f"to §13.6's dismissal roster with a reason, or add a "
                     f"CARRIED entry here naming the row that already covers it. "
                     f"'No row' must never mean 'at parity'.")

    # §13.6's own coverage table must describe the LIVE reference set.
    pub = coverage_table()
    for src in ("kissat", "cadical"):
        n_opt, n_dis = pub[src]
        live_dis = sum(1 for s, _ in dismissed if s == src)
        if n_opt != len(live[src]):
            fails.append(f"§13.6 publishes {n_opt} {src} options; the header has "
                         f"{len(live[src])}. The ledger's coverage table is its "
                         f"claim about what it enumerated — a reference bump "
                         f"must update it, not be absorbed silently.")
        if n_dis != live_dis:
            fails.append(f"§13.6 publishes {n_dis} dismissed {src} options; the "
                         f"roster expands to {live_dis}.")
    if pub["total"][0] != len(kissat) + len(cadical):
        fails.append(f"§13.6's total row says {pub['total'][0]} options; live "
                     f"enumeration gives {len(kissat) + len(cadical)}.")
    if pub["total"][1] != len(dismissed):
        fails.append(f"§13.6's total row says {pub['total'][1]} dismissed; the "
                     f"roster expands to {len(dismissed)}.")

    notes_out.append(f"kissat  {len(kissat):3} options  ·  cadical {len(cadical):3}")
    notes_out.append(f"dismissed {tally['dismissed']:3} (roster, parsed from §13.6)")
    notes_out.append(f"carried   {tally['carried']:3} "
                     f"({len(CARRIED)} roster entries, anchors re-checked)")
    notes_out.append(f"cited     {tally['cited']:3} (named in the ledger corpus)")
    notes_out.append(f"exempt    {tally['exempt']:3} (named in EXEMPT, with a reason)")

    for n in notes_out:
        print(f"  ok    {n}")
    for f in fails:
        print(f"  FAIL  {f}")
    print(f"check-option-coverage: {len(kissat) + len(cadical)} reference options, "
          f"{len(unaccounted)} unaccounted, {len(fails)} failure(s)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
