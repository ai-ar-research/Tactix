// DefaultPolicy (plan §5): the statically composed micro-loop — one
// instantiation of the Policy interface, no special status. Calls the
// SAME rule implementations the registry exposes, but through direct
// (inlinable) calls — the "static dispatch" side of the two-tier design.
// Owns every scheduling heuristic it uses: the focused-mode restart
// schedule (kissat's bias-corrected per-mode glue EMAs), the stable-mode
// reluctant-doubling restart schedule, the arithmetic reduce interval, the
// mode-switch alternation (kissat mode.c: first switch after `modeinit`
// conflicts, stable phases last the preceding focused phase's ticks,
// focused intervals grow as modeint·n·log⁴n), the stable-only rephase
// schedule (kissat rephase.c: rephaseint·n·log³n interval, B,W,I,B,W,O
// rotation), and the inprocessing slot (every registry Inprocess-tier
// rule, on kissat's size-scaled eliminate.c intervals). Constants are the
// published kissat/cadical/Glucose defaults unless a comment says
// otherwise.
#pragma once

#include "policies/policy.hpp"
#include "policies/subpolicy.hpp"

#include <cstdlib>

namespace tactix {

// canonical branching: the rule/kernel ranking's top pick (VMTF
// last-enqueued-unassigned, saved phase — §5.1)
class DefaultBranching final : public BranchingPolicy {
public:
  const char *name() const override { return "vmtf-evsids"; }
  Lit pick(Kernel &) override;

  // Random decisions (kissat decide.c `randec`). A pure H operation: it
  // changes WHICH variable is decided, never the formula, so there is no proof
  // delta and no calculus entry — it is policy, like Rephase or Reorder.
  // kissat ships randec=1 with randecfocused=1 and randecstable=0, so the
  // sequences run in FOCUSED mode only. Length is `randeclength * LOGN(count)`
  // conflicts (options.h:103-108).
  //
  // Deviation: the sequence is tracked as an END CONFLICT rather than a
  // countdown. Equivalent to kissat's per-conflict decrement, and it needs no
  // new hook on the conflict path.
  static constexpr uint64_t RANDEC_INIT = 500;   // randecinit
  static constexpr uint64_t RANDEC_LENGTH = 10;  // randeclength
  uint64_t randec_until = 0;      // conflict count at which the sequence ends
  uint64_t randec_next = RANDEC_INIT; // next conflict count that may start one
  uint64_t randec_sequences = 0;
  uint64_t randec_rng = 0x9E3779B97F4A7C15ull; // deterministic, seeded fixed
  bool randec_enabled = true;
};

// canonical reduction volume: the rule's canonical fraction of the ranked
// worst-first candidates (reduce_canonical_victims — defined rule-side so
// the two cannot drift)
class GlueTieredReduction final : public ReductionPolicy {
public:
  const char *name() const override { return "glue-tiered"; }
  size_t victims(const Kernel &, size_t candidates) override;
  // kissat's ramped fraction instead of the flat canonical one; set by
  // DefaultPolicy together with its √-cadence arm (the two go together).
  bool ramp = false;
};

// canonical BVE growth-slack ramp: kissat's cheap-first doubling 0,1,2,4,8
// (eliminate.c set_next_elimination_bound), capped at slack-8 — see ceiling().
class DefaultBveBoundRamp final : public BveBoundPolicy {
public:
  const char *name() const override { return "kissat-ramp"; }
  // Growth-slack ceiling: 8, a deliberate deviation from kissat's 16. kissat
  // reaches slack-16 gradually across many interleaved eliminate calls and
  // reins in harmful growth with mode/effort feedback our single preprocessing
  // blast does not have. The empirical warrant is ONE Tier B A/B and Tier B has
  // no measured noise floor, so treat the exact value as unvalidated beyond one
  // tier; TACTIX_BVE_RAMP_MAX=N re-sweeps it.
  static uint32_t ceiling() {
    if (const char *s = std::getenv("TACTIX_BVE_RAMP_MAX"))
      return static_cast<uint32_t>(std::atoi(s));
    return 8;
  }
  uint32_t num_passes() const override {
    const uint32_t cap = ceiling();
    static constexpr uint32_t levels[6] = {0, 1, 2, 4, 8, 16};
    uint32_t n = 0;
    for (uint32_t i = 0; i < 6; i++)
      if (levels[i] <= cap)
        n = i + 1;
    return n ? n : 1;
  }
  uint32_t bound(uint32_t pass) const override {
    static constexpr uint32_t levels[6] = {0, 1, 2, 4, 8, 16};
    return levels[pass < 6 ? pass : 5];
  }
};

// canonical Factor grid selection: watch-list-size scoring. Watch sizes are
// ≤ 2^24, so the double→uint32_t round-trip at the rule's integer heap key /
// tie-break comparison is order-exact (no tie flips). accept() stays the
// minimum-gain floor and is deliberately NOT tightened — kissat's effective
// acceptance is ≈ delta>0 (docs/design/s2a-factor-matcher-design.md §2b). The
// knob exists for N1 / learned-policy arms, not for the default.
class DefaultFactorMatch final : public FactorMatchPolicy {
public:
  const char *name() const override { return "watches-size"; }
  double seed_score(const Kernel &k, Lit s) override {
    return k.watches.heads[s].size; // == the rule's watch_score, today's key
  }
  double next_factor_score(const Kernel &k, Lit c, uint32_t /*cofactor*/,
                           Lit /*seed*/) override {
    return k.watches.heads[c].size; // == today's tie-break
  }
  // Mirror of Kernel::factor_bound (kissat's `reduction > bound`,
  // factor.c:939), copied in at the attach site. The rule's own acceptance is
  // `fm ? fm->accept(...) : delta > k.factor_bound`, and DefaultPolicy ALWAYS
  // attaches this matcher — so without this member the kernel scalar would be
  // unreachable under the shipped policy. `bound = 0` is `delta >= 1`.
  uint32_t bound = 0;
  bool accept(uint32_t, uint32_t, int delta) override {
    return delta >= 1 && delta > static_cast<int>(bound);
  }
};

// --- the two inprocessing CLOCKS (ledger SP-SLOT-1 / SP-SLOT-6) ------------
// Both references keep the probing pipeline and the elimination pipeline on
// INDEPENDENT conflict schedules, and the two diverge over the solve:
//   kissat  probe.c:85     UPDATE_CONFLICT_LIMIT(probe, probings, NLOGN)
//                          with probeint=100
//           eliminate.c:600 UPDATE_CONFLICT_LIMIT(eliminate, eliminations,
//                          NLOG2N) with eliminateint=500
//   cadical probe.cpp:980  delta = 25·inprobeint·log10(inprobingphases+9)
//           elim.cpp:1161  delta = scale(elimint·(elimphases+1))
// Under one clock everything rides the elimination cadence and the probing
// family fires ~5x less often than the references run it, with the gap
// widening. GROUP MEMBERSHIP IS POLICY (N1), like the order: it decides which
// clock offers a rule its occasions, never whether the rule may be applied.
enum class SlotGroup : uint8_t { Probe, Eliminate };

// --- the authored inprocessing phase order (ledger SP-SLOT-4, theme T6) -----
// The order in which DefaultPolicy offers inprocessing occasions is DATA, not
// an emergent property of rule names. ORDER IS A POLICY CHOICE (N1): no rule
// guard and no `instances()` may read this table, and it narrows nothing the
// public API can apply — it fixes only the sequence THIS policy walks.
struct SlotOrderRow {
  const char *name;
  SlotGroup group;
};
// The authored list, in order. Inprocess-tier rules absent from it are still
// scheduled (appended, registry order) so a newly registered rule is never
// silently dropped — but an authored position is what it should get.
const SlotOrderRow *default_slot_order(size_t *count);

class DefaultPolicy final : public Policy {
public:
  // scheduling constants (each exists exactly once; the struct
  // initializers below must use these, never repeat the values).
  // Restart: kissat's focused trigger — bias-corrected per-mode glue
  // EMAs (smooth.c warm-up), margin (100+restartmargin)/100, interval
  // restartint + log10(restarts+9)-1. The constants presuppose the smooth
  // warm-up + per-mode persistence: on plain seeded EMAs they measured
  // WORSE than Glucose's, so port the package, not the numbers. Mode:
  // kissat mode.c / options.h defaults — modeinit (first switch) and
  // modeint (growth factor) are distinct options that happen to share a
  // value.
  static constexpr double RESTART_MARGIN = 1.10;    // restartmargin 10
  static constexpr uint64_t RESTART_INTERVAL = 1;   // restartint
  static constexpr double EMA_FAST_WINDOW = 33;     // emafast
  static constexpr double EMA_SLOW_WINDOW = 1e5;    // emaslow
  static constexpr uint64_t REDUCE_INTERVAL_INIT = 2000;
  static constexpr uint64_t REDUCE_INTERVAL_STEP = 300;
  static constexpr uint64_t REDUCE_INTERVAL_KISSAT = 1000; // reduceint
  static constexpr uint64_t MODE_INITIAL_CONFLICTS = 1000; // modeinit
  static constexpr uint64_t MODE_INTERVAL = 1000;          // modeint
  static constexpr uint64_t RELUCTANT_PERIOD = 1024;    // reluctantint
  static constexpr uint64_t RELUCTANT_LIMIT = 1 << 20;  // reluctantlim
  static constexpr uint64_t REPHASE_INTERVAL_INIT = 1000; // rephaseinit
  static constexpr uint64_t REPHASE_INTERVAL = 1000;      // rephaseint
  static constexpr uint64_t REORDER_INTERVAL_INIT = 10000; // reorderinit
  static constexpr uint64_t REORDER_INTERVAL = 10000;      // reorderint
  // reason-side bumping (kissat bumpreasonslimit/bumpreasonsrate; cadical
  // agrees on the limit and says 100 for the rate — the one constant the
  // references disagree on, hence the sweep knob)
  static constexpr uint32_t BUMP_REASONS_LIMIT = 10;
  // Deviation: 20, neither kissat's 10 nor cadical's 100. Bumping fires when
  // the decision rate is BELOW this, so a larger value fires it in more
  // situations; the whole plateau {15, 20, 25} measured equivalent and clearly
  // better than 10. TACTIX_BUMP_REASONS_RATE re-sweeps it.
  static constexpr double BUMP_REASONS_RATE = 20.0;
  // Inprocessing-slot interval, in the kissat eliminate slot (eliminate.c
  // kissat_eliminating / kimits.h UPDATE_CONFLICT_LIMIT(eliminate, ...,
  // NLOG2N, true)): interval = eliminateint · n·log10(n+9)², init
  // eliminateinit, BOTH scaled by kissat_scale_delta's formula-size factor.
  // The constants presuppose that scaling, exactly as the restart constants
  // presuppose the EMA warm-up.
  static constexpr uint64_t SUBSUME_INTERVAL = 500;      // eliminateint
  // The PROBING clock's base interval — kissat `probeint` (options.h:98),
  // scaled by NLOGN rather than the eliminate clock's NLOG2N (probe.c:85).
  // 100 vs 500 with one fewer log factor is why the probing pipeline runs
  // several times per elimination phase in both references.
  static constexpr uint64_t PROBE_INTERVAL = 100;        // probeint
  // Effort budgets (docs/design/effort-scheduler-design.md). kissat kimits.h
  // SET_EFFORT_LIMIT: a technique's dose is a fixed per-mille fraction of the
  // SEARCH work done since that technique last ran, floored by an absolute
  // minimum — simplification effort is a bounded fraction of search effort,
  // which is self-tuning and pathology-proof. Bounding by APPLICATION COUNT
  // instead is size-blind and does not bound simplification at all.
  // kissat mineffort=10 ("in millions"); our tick unit is within 1.6× of
  // kissat's, so the constant transfers directly.
  static constexpr uint64_t MIN_EFFORT_TICKS = 10000000; // mineffort · 1e6
  // Outer rounds per simplification phase (kissat eliminaterounds): each round
  // runs BVE to its budget + the batch simplifiers (Subsume/Strengthen) once,
  // so the batch rules run a few times per phase, not once per BVE variable.
  static constexpr int INPROCESS_PHASE_ROUNDS = 3;
  // chronological backtracking (kissat learn.c kissat_determine_new_level,
  // options chrono=1/chronolevels=100): jumping over more than this many
  // levels is considered inefficient — backtrack one level instead. A pure
  // policy choice through backjump_apply_to (N1: no rule guard changes).
  // Chrono is only measurable once analyze backtracks to the conflict level
  // first; otherwise the gap that gates it is computed from the wrong level and
  // most chronological backtracks silently drop the conflict as a stale pending
  // clause. TACTIX_CHRONO_LEVELS=N overrides (N=0 = always chronological).
  static constexpr uint32_t CHRONO_LEVELS = 100;
  uint32_t chrono_levels = CHRONO_LEVELS;

  int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) override;

  // sub-policies (swappable without touching any rule; null = built-in)
  BranchingPolicy *branching = nullptr;
  ReductionPolicy *reduction = nullptr;
  BveBoundPolicy *bve_bound = nullptr;       // BVE growth-slack ramp
  FactorMatchPolicy *factor_match = nullptr; // Factor grid selection

  // bias-corrected EMA (kissat smooth.c): value = biased / (1 - beta^n)
  // — the Adam-style warm-up makes early averages meaningful, which the
  // kissat restart constants presuppose
  struct Smooth {
    double value = 0, biased = 0, alpha = 0, beta = 0, exp = 1.0;
    void init(double window) {
      alpha = 1.0 / window;
      beta = 1.0 - alpha;
      value = biased = 0;
      exp = 1.0;
    }
    void update(double y) {
      biased += alpha * (y - biased);
      if (exp) {
        const double old_exp = exp;
        exp *= beta;
        if (exp == old_exp) { // beta^n underflow-stuck: correction done
          exp = 0;
          value = biased;
        } else {
          value = biased / (1.0 - exp);
        }
      } else {
        value = biased;
      }
    }
  };
  // per-mode glue averages (kissat averages.h averages[2]): initialized
  // lazily once per mode, PERSIST across mode switches
  struct ModeAverages {
    bool initialized = false;
    Smooth fast_glue, slow_glue;
    // decisions per conflict, per mode (kissat averages.h decision_rate,
    // window emaslow). Gates reason-side bumping: kissat only pays for it
    // while the search is averaging few decisions per conflict.
    Smooth decision_rate;
    uint64_t saved_decisions = 0;
  };

  // scheduling state — public so tests (and future presets) can pose
  // schedules directly; this is policy state, not kernel H-state
  ModeAverages averages[2]; // indexed by kernel.h->stable
  struct {
    // kissat limits.restart.conflicts: gates BOTH modes; updated after
    // focused restarts and on entering focused, stale-but-kept in stable
    uint64_t next_conflicts = RESTART_INTERVAL;
  } restart;
  struct {
    uint64_t next_conflicts = REDUCE_INTERVAL_INIT;
  } reduce;
  // mode-switch alternation (kissat mode.c/limits): even count = focused
  // (conflicts limit pending), odd = stable (ticks limit pending)
  struct {
    uint64_t count = 0;
    uint64_t conflicts_limit = MODE_INITIAL_CONFLICTS;
    uint64_t ticks_limit = 0;
    uint64_t ticks_at_focus_entry = 0;
  } mode;
  // reluctant doubling (Luby-style; kissat reluctant.c) — stable restarts
  struct {
    uint64_t u = 1, v = 1;
    uint64_t wait = RELUCTANT_PERIOD;
    bool trigger = false;
  } reluctant;
  // rephase schedule (kissat rephase.c: stable-only trigger, interval
  // grows rephaseint·n·log³n; rotation B,W,I,B,W,O — the W slots run Walk)
  struct {
    uint64_t next_conflicts = REPHASE_INTERVAL_INIT;
    // rephase EVENTS (rotation index + interval growth). Distinct from
    // stats.rephased, which counts rephase OPS: a B+W slot is one event and
    // two ops. Identical to stats.rephased on the pre-Walk path.
    uint64_t count = 0;
  } rephase;
  // Reorder schedule (kissat reorder.c/kimits.c: initial `reorderinit`,
  // then LINEAR growth `reorderint · count`). OFF unless TACTIX_REORDER is set
  // — see want_reorder.
  struct {
    uint64_t next_conflicts = REORDER_INTERVAL_INIT;
    uint64_t count = 0;
  } reorder;
  // Walk effort, per mille of the search work since the last walk (floor
  // MIN_EFFORT_TICKS). Deviation: NOT kissat's walkeffort = 50, because the
  // budget is a UNIT MISMATCH there — kissat and we scale from cache-line
  // search ticks but SPEND in per-entry walk steps, while cadical spends in
  // cache-line ticks too, so cadical's walkeffort = 80 buys ~13x more walk
  // scanning than 50 does here (docs/design/s6-walk-design.md §6). 560 is a
  // cadical-equivalent dose, chosen at the low end of a flat plateau; Tier C
  // shows no trend at all across 400..800, so do not read precision into it.
  // "How hard to walk" is a plan §5.6 dosing exhibit — TACTIX_WALK_PERMILLE.
  static constexpr uint64_t WALK_PERMILLE = 560;
  uint64_t walk_anchor = 0; // search_ticks when the last walk ran
  // --- the inprocessing SLOT (registry-driven scheduler) -----------------
  // Schedule-as-data: the slot is a plain vector of {rule id, enabled}
  // entries built from the registry (RuleTier::Inprocess — the tier's
  // reader, §5.3), in registry order (name-sorted, deterministic),
  // executed in order at a quiescent root on the kissat eliminate-slot
  // schedule below. PUBLIC and overridable: tests, presets, and the
  // phase-6 learner pose slots by editing entries — these choice points
  // are the arm space (§5.6). CMS's comma-string simplify schedule is the
  // design precedent for slot-as-ordered-list.
  //
  // The live composition: every Inprocess-tier rule enabled; phases =
  // probing family → simplifiers → dosed BVE (density-gated, deferred out
  // of conflicts=0 — kissat fastel=0) → Factor last; effort-ledger budgets
  // per entry; growing NLOG2N conflict interval. WHY it works: elimination
  // runs on a formula probing already simplified, doses are bounded in each
  // rule's own work currency, and the density gates keep during-search
  // elimination off the sparse instances it measurably harms (plan §5.6
  // elimination-dosing exhibit — the gates are the phase-6 learner's named
  // target). During-search phases are SAT-safe only because they are
  // budgeted and material-gated; the unbudgeted version was not. Every rule
  // stays registered, N1-exposed, and random-policy-fuzzed regardless of
  // its `enabled` bit.
  struct SlotEntry {
    int rule = -1;
    bool enabled = true;
    // Which clock offers this entry its occasions (see SlotGroup). Set from
    // the authored table when the slot is built; a POSED slot chooses its own,
    // and the default puts an unclassified entry on the probing clock.
    SlotGroup group = SlotGroup::Probe;
    // --- effort ledger ----------------------------------------------------
    // `permille` is this entry's share of the search work preceding its dose
    // (kissat options.h NAMEeffort); `meter` reads the rule's OWN work counter
    // — its currency (BVE resolutions, Strengthen candidate checks, ticks for
    // the propagating rules), since one unit of "work" is not comparable
    // across rules. kissat likewise budgets a resolvent count with a delta
    // derived from ticks: the ratio transfers, the unit does not. A null meter
    // means all-or-nothing (Congruence/Factor/Substitute expose only the
    // canonical instance, so there is nothing to titrate).
    uint32_t permille = 0;
    uint64_t (*meter)(const Kernel &) = nullptr;
    // This entry's PRIVATE tick counter inside Kernel::stats, or null when its
    // currency is not ticks (BVE resolutions, Strengthen/SubsumeForward
    // candidate checks, Factor's own factor_ticks, which the rule charges
    // itself). The policy scopes `Kernel::tick_sink` to this around the apply,
    // so kernel-side propagation done ON THIS RULE'S BEHALF lands here and
    // nowhere else.
    uint64_t Stats::*tick_counter = nullptr;
    // cadical's accumulate-until-worthwhile threshold (`opts.NAMEthresh`,
    // limit.hpp:150 — reached by TOKEN-PASTE, which is why it reads as dead
    // code to grep). If this occasion's DELTA is smaller than `thresh` per
    // irredundant clause, decline WITHOUT advancing the anchor, so the
    // entitlement accrues and a later occasion can afford a worthwhile pass.
    // 0 = no threshold. cadical ships factor 7, vivify 20, ternary 6.
    //
    // This is the ONLY budget-based decline a batch entry has: `limit` is
    // re-baselined at the point of use (set_entry_limit), so a `meter > limit`
    // pre-check can never fire for one.
    uint64_t thresh = 0;
    bool dosed = false;   // apply per-instance to a budget vs one canonical round
    bool retry_advice = false; // rule maintains a per-variable retry bit to consult
    // BATCH rule whose canonical apply reads Kernel::work_budget in its own
    // currency and stops mid-round; the policy hands it the remaining ledger
    // budget around r.apply(k) (see EffortSpec in the .cpp).
    bool budget_aware = false;
    uint64_t anchor = 0;  // search_ticks when this entry last ran (cadical last.NAME.ticks)
    uint64_t limit = 0;   // meter ceiling, recomputed per occasion (set_entry_limit)
    // Defer this entry out of the conflicts==0 preprocessing phase (kissat
    // fastel=0 — elimination only ever runs on a formula probing has already
    // simplified, in dosed during-search phases). BVE only.
    bool defer_preprocess = false;
    // New-material watermark for Factor's arena side. `binaries_mark` alone was
    // exact only while Factor was binary-only; once Kernel::factor_size > 2 its
    // material also includes small LARGE clauses, and a binary-only watermark
    // then silently skips re-runs that had new work to do. Arena size is a
    // conservative proxy: it grows whenever a clause is added, so the entry may
    // re-run needlessly but never wrongly declines.
    uint64_t arena_mark = 0;
    // DIAGNOSTIC cap: stop scheduling this entry after `max_applies` phases
    // (0 = uncapped, the default and the only shipped setting). Exists because
    // a rule's tier damage can be driven by how many ROUNDS it gets rather than
    // by its dose, and a rate change shrinks each round without ever removing
    // one. Set via TACTIX_SLOT_MAX_APPLIES=Name:N. Never read by a guard: this
    // is the POLICY declining to schedule, not the rule declining to apply (N1).
    uint32_t max_applies = 0;
    uint32_t applies = 0;
    // --- telemetry ----------------------------------------------------------
    // Diagnostic only: nothing here is ever READ by a scheduling decision, so
    // the numbers cannot feed back into behaviour. They exist because otherwise
    // a dose experiment cannot distinguish "this entry was STARVED of budget"
    // from "this entry ran and found nothing" — both look like a flat stat.
    struct Telem {
      uint64_t occasions = 0;   // times the slot loop reached this entry
      uint64_t admitted = 0;    // times it got as far as apply
      uint64_t dec_disabled = 0, dec_cap = 0, dec_preprocess = 0;
      uint64_t dec_guard = 0, dec_budget = 0, dec_delay = 0;
      uint64_t dec_material = 0, dec_warmup = 0, dec_density = 0;
      uint64_t dec_oneshot = 0, dec_nonviable = 0;
      uint64_t handed = 0;      // budget handed to budget-aware entries
      uint64_t spent = 0;       // meter delta actually consumed
      uint64_t ok = 0;          // applies returning Ok
      uint64_t completed = 0;   // rounds that finished inside their budget
    } tm;

    // Per-entry new-material watermarks: an UNMETERED entry (Congruence /
    // Factor / Substitute — full canonical closure per run, nothing to
    // titrate) re-runs in a during-search phase only when the formula gained
    // material it can act on since ITS last run (new binaries / new root-fixed
    // variables). Re-running these closures every phase is a large diffuse
    // Tier C cost, but dropping them entirely loses instances that depend on
    // Congruence — so gate the repetition, keep the rule.
    uint64_t binaries_mark = 0;
    uint64_t root_fixed_mark = 0;
    // Budgeted applies of this entry this solve. Read for the first-dose
    // allowance (Factor's kissat factoriniticks — doses == 0 means the opening
    // round has not run yet).
    uint32_t doses = 0;
    // kissat delay ladder (kimits.c): after an UNPRODUCTIVE run in a
    // during-search phase — kissat vivifyirr criterion, vivified/tried < 1 % —
    // skip this entry's next `delay_current` scheduled occasions (bump:
    // current += 1; productive run: current /= 2; both reset count = current).
    // Backs a technique off the instances where its dose keeps buying nothing
    // while leaving it every-phase where it produces. Policy scheduling state
    // only — the rule stays N1-applicable via the API throughout.
    uint32_t delay_count = 0;
    uint32_t delay_current = 0;
  };
  std::vector<SlotEntry> inprocess_slot; // built lazily at solve() entry
  // Compute this entry's meter ceiling at the point it is reached.
  void set_entry_limit(SlotEntry &, Kernel &);
  // One `c slot` line per entry. Diagnostic only — see the .cpp.
  void report_slot(const Kernel &) const;
  // factordelay analog (kissat factor.c:1068-1085,
  // docs/design/s4-factor-dosing-design.md §3.b): count of dosed BVE rounds
  // executed this solve. Factor's entry fires only when log10(active vars) ≤
  // this + 4 — instances up to 10^4 active vars factor from the very first
  // probe; each BVE round buys one more decade.
  uint32_t bve_rounds_run_ = 0;
  // A/B knob (TACTIX_FORCE_MODE, read once per solve): 0 = off,
  // 1 = pin focused (decline every switch), 2 = pin stable (allow only
  // the first switch). Pure scheduling state — SwitchMode stays
  // guard-true and N1-appliable.
  uint32_t force_mode_ = 0;
  // Slot-order arm (TACTIX_SLOT_ORDER=legacy, read once when the slot is
  // built): false = the authored order, true = the pre-authored registry
  // (name-sorted) order with BVE and Factor moved to the back. Reported on the
  // `c slot-order` line so an A/B arm can be shown to have taken effect.
  bool slot_order_legacy_ = false;
  // Closure delay ladder, default ON (TACTIX_NO_CLOSURE_DELAY reverts;
  // read once per solve): delay unproductive unmetered closures (see the
  // slot's unmetered branch).
  bool closure_delay_ = true;
  // Reason-side bumping (DEFAULT ON; TACTIX_NO_BUMP_REASONS reverts, read
  // once per solve). 0 = off; otherwise the per-conflict mark limit handed to
  // ConflictAnalysis. Ships together with dynamic_tiers_ — see the solve()
  // comment for why the pair is one mechanism.
  uint32_t bump_reasons_limit_ = 0;
  double bump_reasons_rate_ = BUMP_REASONS_RATE;
  // Factor's FIRST-DOSE allowance (docs/design/s4-factor-dosing-design.md
  // §3.b). We follow kissat `factoriniticks` 700 (options.h:59, millions); the
  // references disagree — cadical's `factoriniticks` is 300 (options.hpp:123),
  // i.e. 2.3x less room for the opening BVA burst. Arm:
  // TACTIX_FACTOR_INIT_TICKS (raw ticks).
  static constexpr uint64_t FACTOR_INIT_TICKS = 700000000ull;
  uint64_t factor_init_ticks_ = FACTOR_INIT_TICKS;
  // kissat's delay ladder (kimits.c bump/reduce_delay): every round that
  // hits the mark limit doubles the number of conflicts the pass is skipped
  // for, every round that fits halves it. This is what keeps the extra
  // marking (and the wider per-conflict VMTF sort) off instances where it
  // would only cost throughput.
  struct {
    uint32_t count = 0, current = 0;
  } bump_reasons_delay_;
  // Reduce-cadence arm (TACTIX_REDUCE_SQRT, read once per solve): kissat's
  // reduceint·√reductions interval + ramped deletion fraction instead of our
  // arithmetic interval + flat 75 %.
  bool reduce_sqrt_ = false;
  // Dynamic retention tiers (DEFAULT ON; TACTIX_NO_DYNAMIC_TIERS reverts):
  // kissat 4.0 / cadical 3.0 both ship this ON and both use our fixed 2/6
  // only as the no-data fallback.
  bool dynamic_tiers_ = true;
  struct {
    uint64_t next_conflicts = 2;
    uint64_t interval = 2;
  } retier_;
  // During-search BVE bound level (kissat bounds.eliminate.additional_clauses,
  // eliminate.c set_next_elimination_bound): starts 0 (non-growing
  // eliminations only) and DOUBLES 0→1→2→4→8 only when a dose COMPLETES
  // (schedule exhausted within budget). Running the whole 0..8 ramp inside one
  // dose over-eliminates instead. The preprocessing-phase ramp is untouched.
  // Factor's acceptance bound reads this level too (s4-factor-dosing §3.e).
  uint32_t bve_level_ = 0;
  OccIndex inprocess_occ; // shared occurrence index, reused per phase
  // One inprocessing clock. Two live — `probe_clock` and `elim_clock` — on the
  // formulas in update_slot_limit; see SlotGroup for the reference evidence.
  struct SlotClock {
    uint64_t next_conflicts = 0;
    uint64_t rounds = 0; // phases run; also the interval growth key
  };
  SlotClock probe_clock, elim_clock;
  // Slot timing: the first limit of each clock is size-scaled at solve entry
  // (kissat inits limits after parsing); after that each clock fires on its own
  // growing interval.
  struct {
    bool initialized = false;
    // Ticks spent inside simplification phases. The kernel keeps ONE global
    // propagation meter covering search and probing alike; kissat keeps them
    // apart (search_ticks vs probing_ticks) because every effort budget — and
    // the stable-phase length — is denominated in SEARCH work. Subtracting this
    // accumulator recovers the split policy-side, with no kernel change and no
    // scheduling identity leaking into the rules.
    uint64_t ticks = 0;
  } inprocess;
  // search work only (kissat statistics.search_ticks)
  uint64_t search_ticks(const Kernel &k) const {
    return k.stats.ticks - inprocess.ticks;
  }

  // the scheduling predicates and their transitions, exposed so tests
  // (and future presets) can pose schedules directly
  bool want_restart(const Kernel &) const;
  bool want_reduce(const Kernel &) const;
  bool want_switch_mode(const Kernel &) const;
  bool want_rephase(const Kernel &) const;
  bool want_reorder(const Kernel &) const;
  // One predicate per clock (see SlotGroup). Both are checked in the search
  // loop, probing first — kissat search.c:214-217, cadical internal.cpp:322-325.
  bool want_slot_phase(const Kernel &, SlotGroup) const;
  void tick_reluctant();                   // per-conflict in stable mode
  void update_mode_limits(const Kernel &); // after a switch
  void update_restart_limit(const Kernel &); // after a restart
  void update_reduce_limit(const Kernel &);  // after a reduction
  void update_rephase_limit(const Kernel &); // after a rephase
  void update_reorder_limit(const Kernel &); // after a reorder
  void update_slot_limit(const Kernel &, SlotGroup); // after a slot phase

private:
  // One inprocessing phase on one clock: descend to a quiescent root, walk this
  // group's slot entries for INPROCESS_PHASE_ROUNDS outer rounds, reclaim.
  // Returns 20 when the phase refuted the formula, 0 otherwise.
  int run_slot_phase(Kernel &, SlotGroup, volatile sig_atomic_t *stop);
  void observe_glue(const Kernel &, uint32_t glue); // per-conflict EMAs
  void observe_decision_rate(const Kernel &);       // before each analysis
  uint32_t want_bump_reasons(const Kernel &);       // the kissat gate ladder
  DefaultBranching default_branching_;
  GlueTieredReduction default_reduction_;
  DefaultBveBoundRamp default_bve_ramp_;
  DefaultFactorMatch default_factor_match_;
  std::vector<Ref> reduce_scratch_;
  std::vector<uint64_t> inprocess_payloads_; // instance snapshot, reused
};

// convenience wrapper: solve under a fresh DefaultPolicy
int default_solve(Kernel &, volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
