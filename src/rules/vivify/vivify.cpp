// See vivify.hpp for the reference/deviation record.
#include "rules/vivify/vivify.hpp"

#include "diagnostics/rule_journal.hpp"
#include "diagnostics/event_trace.hpp"
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <unordered_map>

namespace tactix {

#ifdef TACTIX_ENABLE_RULE_JOURNAL
namespace {
struct VivifyJournalSnapshot {
  uint64_t subsumed, shrunken, implied, asymmetric, instantiated, deleted;
  uint64_t noop, ticks;
  uint64_t clause_id;
  uint32_t size, glue;
  bool live, redundant, prioritized;
};

struct VivifyJournalFacts {
  std::string probes = "-";
  std::string learned = "-";
  std::string replacement_pose = "-";
  std::string replacement_levels = "-";
  std::string conflict_clause = "-";
  std::string reasons = "-";
  std::string trail = "-";
  std::string selected_cause = "-";
  int implied = 0;
  bool conflict = false;
  bool subsumer = false;
  bool shrinkable = false;
  uint32_t reused = 0, made = 0, assumed = 0;
  const char *terminal = "none";
};
static thread_local bool vivify_journal_active;
static thread_local VivifyJournalFacts vivify_journal_facts;

static std::string vivify_journal_literals(const Kernel &k,
                                           const std::vector<Lit> &lits) {
  std::string out;
  for (const Lit lit : lits) {
    if (!out.empty())
      out.push_back(',');
    out += std::to_string(kernel_externalize(k, lit));
  }
  return out;
}

static std::string vivify_journal_selected_cause(const Kernel &k) {
  const int selected = std::abs(rule_journal_selected_literal());
  if (!selected)
    return "-";
  for (const Lit lit : k.trail) {
    const int external = kernel_externalize(k, lit);
    if (std::abs(external) != selected)
      continue;
    const Assigned &a = k.assigned[var_of(lit)];
    std::string result = std::to_string(external) + ':';
    if (a.reason == DECISION_REASON)
      return result + "decision";
    if (a.reason == UNIT_REASON)
      return result + "unit";
    if (a.binary)
      return result + "binary(" +
             std::to_string(kernel_externalize(
                 k, a.jumped ? k.jumped_over[var_of(lit)] : a.reason)) +
             ')';
    const Clause *reason = clause_at(k.arena, a.reason);
    std::vector<Lit> lits(reason->begin(), reason->end());
    return result + "large(" + vivify_journal_literals(k, lits) + ')';
  }
  return "unassigned";
}

static VivifyJournalSnapshot vivify_journal_snapshot(const Kernel &k,
                                                      const Clause *c) {
  return {k.stats.vivify_subsumed,     k.stats.vivify_shrunken,
          k.stats.vivify_implied,      k.stats.vivify_asymmetric,
          k.stats.vivify_instantiated, k.stats.vivify_deleted,
          k.stats.vivify_noop,         k.stats.ticks,
          c ? c->id() : 0,             c ? c->size : 0,
          c ? c->glue() : 0,           c && !c->garbage(),
          c && c->redundant(),         c && c->vivify()};
}

static std::string vivify_journal_candidate(const Kernel &k, const Clause *c,
                                             uint64_t ordinal,
                                             uint32_t rank_count) {
  std::string out = "clause=" + std::to_string(c->id()) +
                    ";round=" + std::to_string(k.stats.vivify_rounds) +
                    ";scope=" + std::to_string(k.vivify_scope) +
                    ";ordinal=" + std::to_string(ordinal) +
                    ";rank-count=" + std::to_string(rank_count) +
                    ";lits=";
  bool first = true;
  for (const Lit lit : *c) {
    if (!first)
      out.push_back(',');
    first = false;
    out += std::to_string(kernel_externalize(k, lit));
  }
  return out;
}

static std::string vivify_journal_selected_clause_pose(const Kernel &k) {
  const char *value = std::getenv("TACTIX_RULE_JOURNAL_CLAUSE_HASH");
  if (!value || !*value)
    return "-";
  const uint64_t selected = std::strtoull(value, nullptr, 16);
  std::string result;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      std::vector<int> external;
      external.reserve(c->size);
      for (const Lit lit : *c)
        external.push_back(kernel_externalize(k, lit));
      std::vector<int> sorted = external;
      std::sort(sorted.begin(), sorted.end());
      uint64_t hash = UINT64_C(1469598103934665603);
      for (const int external_lit : sorted)
        hash = (hash ^ static_cast<uint64_t>(
                           static_cast<int64_t>(external_lit))) *
               UINT64_C(1099511628211);
      hash = (hash ^ c->size) * UINT64_C(1099511628211);
      if (hash == selected) {
        if (!result.empty())
          result.push_back('|');
        result += std::to_string(c->id()) + ':';
        for (size_t i = 0; i < external.size(); i++) {
          if (i)
            result.push_back(',');
          result += std::to_string(external[i]);
        }
      }
    }
    ref += clause_actual_words(c);
  }
  return result.empty() ? "-" : result;
}

static void vivify_journal_emit_candidate(
    const Kernel &k, const std::string &candidate,
    const VivifyJournalSnapshot &pre, uint64_t pre_digest,
    uint64_t formula_epoch, uint64_t root_epoch,
    const std::string &selected_pose_pre, ApplyResult result) {
  const VivifyJournalSnapshot post = vivify_journal_snapshot(k, nullptr);
  const char *outcome = result == ApplyResult::Unsat
                            ? "refuted"
                            : result == ApplyResult::Ok ? "applied" : "no-op";
  const char *delta = "none";
  if (post.subsumed != pre.subsumed)
    delta = "delete-subsumed";
  else if (post.shrunken != pre.shrunken)
    delta = "rewrite-shrunken";
  else if (post.implied != pre.implied)
    delta = "delete-implied";
  else if (post.asymmetric != pre.asymmetric)
    delta = "delete-asymmetric";
  else if (post.instantiated != pre.instantiated)
    delta = "rewrite-instantiated";
  else if (post.deleted != pre.deleted)
    delta = "delete-root-satisfied";
  RuleJournalRecord record;
  record.rule = "Vivify";
  record.event = "candidate-attempt";
  record.candidate = candidate;
  record.trigger = "vivify-schedule-pop";
  record.context = "scope=" + std::to_string(k.vivify_scope) +
                   ";reuse=" + std::to_string(k.vivify_reuse) +
                   ";level-before=0";
  record.formula_epoch = formula_epoch;
  record.root_epoch = root_epoch;
  record.pre_digest = pre_digest;
  record.guards = "live=" + std::to_string(pre.live) +
                  ";size-ge-3=" + std::to_string(pre.size >= 3) +
                  ";redundant=" + std::to_string(pre.redundant) +
                  ";glue=" + std::to_string(pre.glue) +
                  ";prioritized=" + std::to_string(pre.prioritized) +
                  ";conflict=" +
                  std::to_string(vivify_journal_facts.conflict) +
                  ";implied=" + std::to_string(vivify_journal_facts.implied) +
                  ";subsumer=" +
                  std::to_string(vivify_journal_facts.subsumer) +
                  ";shrinkable=" +
                  std::to_string(vivify_journal_facts.shrinkable);
  record.antecedents = "clause-id=" + std::to_string(pre.clause_id);
  record.outcome = outcome;
  record.decline = result == ApplyResult::NoOp ? "no-deduction" : "none";
  record.deltas = delta;
  record.witnesses =
      "terminal=" + std::string(vivify_journal_facts.terminal) +
      ";probes=" + vivify_journal_facts.probes +
      ";learned=" + vivify_journal_facts.learned +
      ";replacement-pose=" + vivify_journal_facts.replacement_pose +
      ";replacement-levels=" + vivify_journal_facts.replacement_levels +
      ";conflict-clause=" + vivify_journal_facts.conflict_clause +
      ";trail=" + vivify_journal_facts.trail +
      ";selected-cause=" + vivify_journal_facts.selected_cause +
      ";selected-pose-pre=" + selected_pose_pre +
      ";selected-pose-post=" + vivify_journal_selected_clause_pose(k) +
      ";reasons=" + vivify_journal_facts.reasons +
      ";reused=" + std::to_string(vivify_journal_facts.reused) +
      ";made=" + std::to_string(vivify_journal_facts.made) +
      ";assumed=" + std::to_string(vivify_journal_facts.assumed);
  record.effort = post.ticks - pre.ticks;
  record.budget = k.work_budget;
  record.post_digest = rule_journal_digest(k);
  rule_journal_emit(k, record);
}
} // namespace
#endif

RuleEligibility vivify_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct (D-SPLIT-3): this is a pure
  // F-rewrite, sound regardless of a recorded conflict.
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0);
}

// A retained probing trail makes k.value() speculative, so every candidacy and
// probe-list decision reads the root value (kissat `kissat_fixed`). The
// root-satisfied deletion path uses the same test, and a clause satisfied only
// under an assumption must never be deleted (design §3.1).
// Does `c` fall in the band `k.vivify_scope` selects? The redundant side is
// split into kissat's three tiers (vivify.c:236-256) so a policy can spend
// three separate shares. Design §2.5.
static bool in_scope(const Kernel &k, const Clause *c) {
  const bool red = c->redundant();
  const uint32_t glue = c->glue();
  switch (k.vivify_scope) {
  case 1:
    return red && glue <= k.tier2; // the whole redundant band (legacy)
  case 2:
    return !red;                        // irredundant
  case 3:
    return red && glue <= k.tier1;      // tier1
  case 4:
    return red && glue > k.tier1 && glue <= k.tier2; // tier2
  case 5:
    return red && glue > k.tier2;       // tier3 (C10 hand-off H3)
  default:
    // Scope 0 is the unscoped N1 path, so it means every band, tier3
    // included: bounding it by tier2 would remove a capability rather than
    // decline by default (tests/test_vivify_n1.cpp).
    return true;
  }
}

// Kissat has no clause-size cutoff here: the round's propagation-tick budget
// is its only work bound (vivify.c:1254-1327).  In particular a large probe
// still updates the retained prefix and watch/search state even when it makes
// no formula edit, so silently omitting it also changes later candidates.
static bool is_candidate(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->size < 3)
    return false;
  // The live tier, not the static default (kissat `vivify_tier2_limit`,
  // vivify.c:132, whose `vivifyfocusedtiers` ships on). The cut binds only for
  // the scopes that bound the redundant side by tier2 (1/3/4); on 0 and 5 it
  // would contradict `in_scope` and re-close the tier3 band.
  if (c->redundant() && c->glue() > k.tier2 && k.vivify_scope != 0 &&
      k.vivify_scope != 5)
    return false;
  for (const Lit l : *c)
    if (kernel_root_value(k, l) > 0)
      return false;
  return true;
}

// --- the round's global literal order ---------------------------------------
// What makes cross-candidate reuse possible is the order, not the reuse loop
// (cadical vivify.cpp:35-42; kissat vivify_sort_clause_by_counts). The side
// vector is the comparison key, but a canonical round also commits that pose
// to each full-size scheduled clause before rebuilding large watches.  This is
// observable: propagation can otherwise select a different valid conflict and
// hence a different Vivify transaction on the same formula.
struct VivifyOrder {
  const Kernel *kernel = nullptr;
  std::vector<uint32_t> counts; // per-literal occurrences; persistent scratch
  std::vector<Lit> touched;     // ...restored in O(touched), never O(nvars)
  bool operator()(Lit a, Lit b) const { // a ≺ b: more occurrences first
    if (counts[a] != counts[b])
      return counts[a] > counts[b];
    // Kissat's branch-free `more_occurrences` comparator breaks equal counts
    // by descending raw internal literal (vivify.c:24-31).  This tie is not
    // cosmetic: the in-place pose is committed before all large watches are
    // rebuilt, so it can select a different valid antecedent in propagation.
    return kernel_import_lit_order(*kernel, a) >
           kernel_import_lit_order(*kernel, b);
  }
};

static VivifyOrder &vivify_order() {
  static thread_local VivifyOrder ord;
  return ord;
}

static void order_clear(VivifyOrder &ord) {
  for (const Lit l : ord.touched)
    ord.counts[l] = 0;
  ord.touched.clear();
}

static void order_sort(const VivifyOrder &ord, std::vector<Lit> &out) {
  std::sort(out.begin(), out.end(), [&ord](Lit a, Lit b) { return ord(a, b); });
}

#ifdef TACTIX_ENABLE_RULE_JOURNAL
static void vivify_journal_occurrence_control(const Kernel &k,
                                              const char *event,
                                              uint64_t scheduled = 0,
                                              uint64_t prioritized = 0,
                                              bool rearmed = false) {
  const int selected = rule_journal_selected_literal();
  if (!selected)
    return;
  uint64_t selected_hash = 0;
  if (const char *value = std::getenv("TACTIX_RULE_JOURNAL_CLAUSE_HASH"))
    selected_hash = std::strtoull(value, nullptr, 16);
  std::vector<uint64_t> signatures;
  std::string details = "selected=";
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      bool contains = false;
      std::vector<int> external;
      external.reserve(c->size);
      for (const Lit lit : *c) {
        const int value = kernel_externalize(k, lit);
        contains |= value == selected;
        external.push_back(value);
      }
      if (contains) {
        std::sort(external.begin(), external.end());
        uint64_t hash = UINT64_C(1469598103934665603);
        for (const int value : external)
          hash = (hash ^ static_cast<uint64_t>(static_cast<int64_t>(value))) *
                 UINT64_C(1099511628211);
        hash = (hash ^ c->size) * UINT64_C(1099511628211);
        signatures.push_back(hash);
        if (selected_hash && hash == selected_hash) {
          if (details.size() > 9)
            details.push_back('|');
          details += std::to_string(c->id()) + "(red=" +
                     std::to_string(c->redundant()) + ",glue=" +
                     std::to_string(c->glue()) + ",priority=" +
                     std::to_string(c->vivify()) + "):";
          for (size_t i = 0; i < external.size(); i++) {
            if (i)
              details.push_back(',');
            details += std::to_string(external[i]);
          }
        }
      }
    }
    ref += clause_actual_words(c);
  }
  std::sort(signatures.begin(), signatures.end());
  std::string witnesses = details + ";signatures=";
  char hex[17];
  for (size_t i = 0; i < signatures.size(); i++) {
    if (i)
      witnesses.push_back(',');
    std::snprintf(hex, sizeof hex, "%016llx",
                  static_cast<unsigned long long>(signatures[i]));
    witnesses += hex;
  }
  RuleJournalRecord record;
  record.kind = "control";
  record.rule = "Vivify";
  record.event = event;
  record.candidate = "literal=" + std::to_string(selected);
  record.trigger = "second-schedule-pass";
  record.context = "scope=" + std::to_string(k.vivify_scope);
  record.pre_digest = rule_journal_digest(k);
  record.guards = "occurrences=" + std::to_string(signatures.size()) +
                  ";scheduled=" + std::to_string(scheduled) +
                  ";prioritized=" + std::to_string(prioritized) +
                  ";rearmed=" + std::to_string(rearmed);
  record.antecedents = "all-live-large-clauses";
  record.outcome = "observed";
  record.decline = "none";
  record.deltas = "none";
  record.witnesses = witnesses;
  record.post_digest = record.pre_digest;
  rule_journal_emit(k, record);
}
#endif

// The non-root-fixed literals of `c`, in probe order — clause order unless
// `ordered`, which is off for the no-reuse arm and for the standalone path,
// where `counts` was never filled and no next candidate shares a prefix.
static void probe_list(const Kernel &k, const Clause *c,
                       const VivifyOrder &ord, std::vector<Lit> &out,
                       bool ordered) {
  out.clear();
  for (const Lit l : *c)
    if (kernel_root_value(k, l) == 0)
      out.push_back(l);
  if (ordered)
    order_sort(ord, out);
}

// The scheduling side-effects a ROUND needs and a const query must not have.
struct VivifySchedule {
  uint64_t prioritized = 0; // candidates carrying the leftover bit
  uint64_t flushed = 0;     // prefix-subsumed before any propagation
  uint64_t root_changed = 0; // root-satisfied/false schedule simplifications
  bool rearmed = false;     // ...the set was empty, so the scope was re-armed
  bool root_unit = false;
  bool refuted = false;
};

// `commit` writes the re-arm back into the clause headers; the const query
// applies the same rule virtually, reporting the order a round would use.
static void vivify_schedule(const Kernel &k, std::vector<Ref> &out,
                            VivifySchedule *commit) {
  out.clear();
  VivifyOrder &ord = vivify_order();
  ord.kernel = &k;
  if (ord.counts.size() < 2 * k.nvars)
    ord.counts.resize(2 * k.nvars, 0);
  order_clear(ord);
  struct Cand {
    Ref ref;
    uint64_t arena_order;
    uint32_t prioritized, redundant, band, size;
    uint32_t key_off, key_len; // literal snapshot at scheduling time
  };
  std::vector<Cand> cands;
  std::vector<Lit> keys;
  uint64_t prioritized = 0;

  const auto append_candidate = [&](Ref ref, bool priority) {
    const Clause *c = clause_at(k.arena, ref);
    uint32_t band = 0;
    if (c->redundant()) {
      if (c->glue() <= k.tier1)
        band = 1;
      else if (c->glue() <= k.tier2)
        band = 2;
      else
        band = 3;
    }
    const uint32_t key_off = static_cast<uint32_t>(keys.size());
    for (const Lit lit : *c)
      keys.push_back(lit);
    cands.push_back({ref,
                     kernel_clause_position(k, ref),
                     priority ? 1u : 0u,
                     c->redundant() ? 1u : 0u, band, c->size, key_off,
                     c->size});
    prioritized += priority;
  };
  const auto append_priority = [&](bool priority) {
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (c->vivify() == priority && is_candidate(k, c) && in_scope(k, c))
        append_candidate(ref, priority);
      ref += clause_actual_words(c);
    }
  };
  // Kissat schedules in two arena passes: unprioritized clauses first, then
  // prioritized clauses.  The occurrence-count pass is the *second* arena
  // pass.  Consequently root simplifications of unprioritized candidates are
  // already visible to the counts, while each prioritized clause is counted
  // immediately before its own simplification.  Moving the count before both
  // passes changes equal-prefix ranks even when the final clause set matches.
  const auto simplify_priority = [&](bool prioritized) {
    Kernel &mut = const_cast<Kernel &>(k);
    const Ref original_words = static_cast<Ref>(k.arena.size());
    for (Ref ref = 0; ref < original_words;) {
      Clause *c = clause_at(mut.arena, ref);
      const uint32_t words = clause_actual_words(c);
      if (!c->garbage() && c->size >= 3 && in_scope(k, c) &&
          c->vivify() == prioritized) {
        const bool retain_priority = c->vivify();
        Ref replacement = ref;
        uint64_t changed = 0;
        const RootFlushOutcome outcome =
            kernel_flush_root_clause(mut, &replacement, &changed);
        commit->root_changed += changed;
        if (retain_priority && outcome == RootFlushOutcome::Rewritten)
          clause_at(mut.arena, replacement)->set_vivify(true);
        if (outcome == RootFlushOutcome::Unit)
          commit->root_unit = true;
        else if (outcome == RootFlushOutcome::Refuted)
          commit->refuted = true;
        // vivify.c:schedule_vivification_candidates checks the tier and
        // priority filters before simplify_vivification_candidate, then pushes
        // the simplified in-place record without rechecking its (possibly
        // lowered) glue. Tactix rewrites a shortened clause at a fresh Ref, so
        // carry that pass-time membership to the replacement explicitly. A
        // tier-2 clause shrinking to glue 2 still belongs to this tier-2
        // schedule; it moves bands only on the next round.
        if ((outcome == RootFlushOutcome::None ||
             outcome == RootFlushOutcome::Rewritten) &&
            is_candidate(k, clause_at(k.arena, replacement)))
          append_candidate(replacement, prioritized);
      }
      ref += words;
    }
  };
  if (commit)
    simplify_priority(false);
  else
    append_priority(false);
  if (commit && (commit->root_unit || commit->refuted)) {
    out.clear();
    return;
  }

  // The count_clause call precedes tier and priority filters, so this is the
  // complete live database after pass zero, not merely the active band.
  if (k.vivify_reuse)
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage())
        for (const Lit l : *c) {
          if (ord.counts[l]++ == 0)
            ord.touched.push_back(l);
        }
      ref += clause_actual_words(c);
    }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  vivify_journal_occurrence_control(k, "occurrence-count");
#endif

  if (commit)
    simplify_priority(true);
  else
    append_priority(true);
  if (commit && (commit->root_unit || commit->refuted)) {
    out.clear();
    return;
  }

  // Diagnostic-only parity boundary: these are the exact counts consumed by
  // Kissat's `more_occurrences` sorter after both scheduling passes.  Literal
  // selection lives in the compile-time event observer, never in the solver.
  event_trace_vivify_counts(k, ord.counts.data(), ord.counts.size());

  uint64_t live_irredundant = 0, live_redundant = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage())
      (c->redundant() ? live_redundant : live_irredundant)++;
    ref += clause_actual_words(c);
  }
  const bool radix_reverse_irredundant =
      k.vivify_scope == 2 && !k.vivify_flush &&
      live_irredundant / 10 > live_redundant;

  // The occurrence counts are complete only after the second arena pass has
  // been counted. Sort each pass-time snapshot only when Kissat sorts this
  // schedule. In the large-irredundant regime it deliberately skips global
  // literal and candidate sorting; vivify_one still sorts the probe stack of
  // each clause that the tick budget actually reaches.
  if (k.vivify_reuse && !radix_reverse_irredundant)
    for (const Cand &cand : cands)
      std::sort(keys.begin() + cand.key_off,
                keys.begin() + cand.key_off + cand.key_len,
                [&ord](Lit a, Lit b) { return ord(a, b); });
  // kissat vivify.c:296-301: an empty leftover set re-arms the whole scope.
  // The bit is a priority mechanism and never a suppressor.
  const bool rearm = prioritized == 0 && !cands.empty();
  if (rearm)
    for (Cand &c : cands)
      c.prioritized = 1;
  if (commit) {
    commit->rearmed = rearm;
    commit->prioritized = rearm ? cands.size() : prioritized;
    if (rearm) {
      Arena &arena = const_cast<Arena &>(k.arena);
      for (const Cand &c : cands)
        clause_at(arena, c.ref)->set_vivify(true);
    }
  }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  vivify_journal_occurrence_control(k, "schedule-ready", cands.size(),
                                    prioritized, rearm);
#endif

  // kissat's precise redundant-band sorter writes the count order into each
  // scheduled clause before `watch_large_clauses`.  Retaining it only in
  // `keys` reproduces candidate/probe order but not propagation's antecedent
  // order.  Clauses with root-false literals are left to vivify_one's existing
  // proof-bearing simplification path; the common full-size case is a pure
  // permutation and can be committed here.
  if (commit && k.vivify_reuse) {
    Arena &arena = const_cast<Arena &>(k.arena);
    for (const Cand &cand : cands) {
      Clause *clause = clause_at(arena, cand.ref);
      // Kissat's precise redundant-tier sorter commits the count pose to the
      // clause.  Its irredundant sorter is deliberately imprecise (one count
      // key) and leaves the clause pose untouched; vivify_clause sorts only a
      // temporary probe stack there.
      if (!cand.redundant || cand.key_len != clause->size)
        continue;
      for (uint32_t i = 0; i < cand.key_len; i++)
        clause->lits[i] = keys[cand.key_off + i];
    }
  }

  const Lit *kb = keys.data();
  const auto key_less = [&](const Cand &a, const Cand &b) {
    if (a.band != b.band)
      return a.band < b.band;
    const uint32_t n = std::min(a.key_len, b.key_len);
    for (uint32_t i = 0; i < n; i++) {
      const Lit x = kb[a.key_off + i], y = kb[b.key_off + i];
      if (x != y)
        return ord(x, y);
    }
    return a.key_len < b.key_len;
  };
  const auto key_prefix = [&](const Cand &a, const Cand &b) {
    if (a.band != b.band || a.key_len > b.key_len)
      return false;
    for (uint32_t i = 0; i < a.key_len; i++)
      if (kb[a.key_off + i] != kb[b.key_off + i])
        return false;
    return true;
  };

  if (commit && k.vivify_flush && cands.size() > 1) {
    std::stable_sort(cands.begin(), cands.end(), key_less);
    Arena &arena = const_cast<Arena &>(k.arena);
    Kernel &mut = const_cast<Kernel &>(k);
    uint32_t write = 0;
    const Cand *prev[4] = {nullptr, nullptr, nullptr, nullptr};
    for (const Cand &cand : cands) {
      const uint32_t part = cand.band;
      if (prev[part] && key_prefix(*prev[part], cand)) {
        kernel_delete_clause(mut, clause_at(arena, cand.ref));
        commit->flushed++;
      } else {
        cands[write++] = cand;
        prev[part] = &cands[write - 1];
      }
    }
    cands.resize(write);
  }

  // Kissat skips candidate sorting entirely when irredundant clauses outnumber
  // redundant clauses by more than 10:1: its arena is stable, so popping the
  // schedule already gives reverse arena order. Tactix appends replacements
  // and records their old in-place position in `arena_order`; comparison-
  // sorting millions of clauses merely reconstructed that same order. Restore
  // it with a stable radix pass instead. Priority pass zero precedes pass one,
  // so partitioning in ascending order and reversing reproduces Kissat's pop.
  if (radix_reverse_irredundant && cands.size() > 1) {
    uint64_t max_order = 0;
    for (const Cand &cand : cands)
      max_order = std::max(max_order, cand.arena_order);
    std::vector<Cand> scratch(cands.size());
    std::vector<size_t> counts(size_t{1} << 16);
    bool data_in_cands = true;
    for (unsigned shift = 0; shift < 64; shift += 16) {
      std::fill(counts.begin(), counts.end(), 0);
      const std::vector<Cand> &src = data_in_cands ? cands : scratch;
      std::vector<Cand> &dst = data_in_cands ? scratch : cands;
      for (const Cand &cand : src)
        counts[(cand.arena_order >> shift) & 0xffffu]++;
      size_t offset = 0;
      for (size_t &count : counts) {
        const size_t next = offset + count;
        count = offset;
        offset = next;
      }
      for (const Cand &cand : src)
        dst[counts[(cand.arena_order >> shift) & 0xffffu]++] = cand;
      data_in_cands = !data_in_cands;
      if (shift == 48 || !(max_order >> (shift + 16)))
        break;
    }
    if (!data_in_cands)
      cands.swap(scratch);
    scratch.clear();
    scratch.reserve(cands.size());
    for (const Cand &cand : cands)
      if (!cand.prioritized)
        scratch.push_back(cand);
    for (const Cand &cand : cands)
      if (cand.prioritized)
        scratch.push_back(cand);
    cands.swap(scratch);
    std::reverse(cands.begin(), cands.end());
  } else {
    std::sort(cands.begin(), cands.end(),
              [&](const Cand &a, const Cand &b) {
              if (a.prioritized != b.prioritized)
                return a.prioritized > b.prioritized; // leftovers first
              if (!k.vivify_reuse) {
                if (a.redundant != b.redundant)
                  return a.redundant < b.redundant; // irredundant first
                if (a.size != b.size)
                  return a.size < b.size;
                return a.arena_order < b.arena_order;
              }
              if (!a.redundant && !b.redundant) {
                // For a large irredundant population Kissat skips candidate
                // sorting altogether (vivify.c:1443). The schedule was pushed
                // in arena order and is popped from the back.
                if (live_irredundant / 10 > live_redundant)
                  return a.arena_order > b.arena_order;
                // COUNTREF_COUNTS == 1 in Kissat's irredundant path: rank by
                // the maximum occurrence count, then inverse size. Radix sort
                // is stable in arena order and the schedule is popped from
                // the back, hence descending reference is the final tie.
                const uint32_t ac = ord.counts[kb[a.key_off]];
                const uint32_t bc = ord.counts[kb[b.key_off]];
                if (ac != bc)
                  return ac > bc;
                if (a.key_len != b.key_len)
                  return a.key_len < b.key_len;
                return a.arena_order > b.arena_order;
              }
              // lexicographic under the global literal order, so candidates
              // sharing a leading occurrence profile become adjacent. Kissat's
              // `worse_candidate` compares COUNTS at every position, not the
              // literal identity; only the final posed literal breaks a fully
              // equal count vector, then the arena reference. Its stack is
              // popped from the back, hence both final ties are descending in
              // this forward schedule.
              const uint32_t n = std::min(a.key_len, b.key_len);
              for (uint32_t i = 0; i < n; i++) {
                const Lit x = kb[a.key_off + i], y = kb[b.key_off + i];
                if (ord.counts[x] != ord.counts[y])
                  return ord.counts[x] > ord.counts[y];
              }
              if (a.key_len != b.key_len)
                return a.key_len < b.key_len; // shorter prefix first
              const Lit ax = kb[a.key_off + a.key_len - 1];
              const Lit bx = kb[b.key_off + b.key_len - 1];
              const uint64_t ao = kernel_import_lit_order(k, ax);
              const uint64_t bo = kernel_import_lit_order(k, bx);
              if (ao != bo)
                return ao > bo;
              return a.arena_order > b.arena_order;
              });
  }
  out.reserve(cands.size());
  for (const Cand &c : cands)
    out.push_back(c.ref);
}

void vivify_candidates(const Kernel &k, std::vector<Ref> &out) {
  vivify_schedule(k, out, nullptr);
}

// decision-free probe assumption / abandonment (Lucky's primitives)
static void assume(Kernel &k, Lit lit) {
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  kernel_assign(k, lit, k.level, DECISION_REASON, false);
}

// Discard the whole probing trail and close the scope.  Kissat needs this for
// unit/binary rewrites, but deliberately keeps a valid prefix across large
// rewrites and deletions.  Those paths are handled in vivify_one below.
static void reset_to_root(Kernel &k) {
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  if (k.level > 0)
    kernel_backtrack(k, 0, /*update_phases=*/false);
  // Hand search back the watch order and `searched` hints these propagations
  // churned. Must run before rewrite() touches the arena, the rollback log
  // holding Refs; every rewrite site below is reached through here.
  kernel_probing_end(k);
}

// Kissat's `vivify_watch_clause` chooses the two watched literals under the
// retained probing assignment immediately after a large rewrite.  The
// replacement clause lives at a new arena reference in Tactix, so pose the
// literal vector before kernel_replace_clause installs its watches.
void vivify_pose_replacement_watches(Kernel &k, std::vector<Lit> &lits) {
  const auto pose = [&](uint32_t start) {
    uint32_t best_pos = start;
    const int8_t first_value = k.value(lits[start]);
    uint32_t best_level = k.var_level(var_of(lits[start]));
    // Kissat deliberately keeps the loop condition tied to the original
    // first literal.  In particular, finding an unassigned literal after the
    // conflict backtrack does not stop the scan: a later unassigned literal,
    // or a later false literal at a higher retained level, still wins.
    for (uint32_t i = start + 1; first_value < 0 && i < lits.size(); i++) {
      const Lit lit = lits[i];
      const int8_t value = k.value(lit);
      if (value < 0) {
        const uint32_t level = k.var_level(var_of(lit));
        if (level <= best_level)
          continue;
        best_level = level;
      }
      best_pos = i;
    }
    std::swap(lits[start], lits[best_pos]);
  };
  pose(0);
  pose(1);
}

void vivify_reestablish_watch_invariant(Kernel &k, const Lit *lits,
                                        uint32_t size) {
  if (!k.level || size < 2)
    return;
  const Lit first = lits[0], second = lits[1];
  const int8_t first_value = k.value(first);
  const int8_t second_value = k.value(second);
  const uint32_t first_level =
      first_value ? k.var_level(var_of(first)) : 0;
  const uint32_t second_level =
      second_value ? k.var_level(var_of(second)) : 0;
  uint32_t new_level = 0;
  if (first_value >= 0 && second_value >= 0)
    return;
  if (first_value < 0 && !second_value)
    new_level = first_level;
  else if (first_value < 0 && second_value > 0) {
    if (first_level >= second_level)
      return;
    new_level = first_level;
  } else if (second_value < 0 && !first_value)
    new_level = second_level;
  else if (second_value < 0 && first_value > 0) {
    if (second_level >= first_level)
      return;
    new_level = second_level;
  } else {
    assert(first_value < 0 && second_value < 0);
    new_level = std::min(first_level, second_level);
  }
  assert(new_level);
  kernel_backtrack(k, new_level - 1, /*update_phases=*/false);
}

void vivify_trim_replacement_prefix(Kernel &k, Lit implied,
                                    uint32_t replacement_size) {
  assert(replacement_size > 2);
  const uint32_t retained = replacement_size - 2;
  if (implied == INVALID_LIT && k.level > retained)
    kernel_backtrack(k, retained, /*update_phases=*/false);
}

// last == 0 means the sorted entries already end in the conflicting hint
// (the implied literal's own reason); otherwise `last` closes the chain
static void emit_chain(Kernel &k, uint64_t id, const std::vector<Lit> &lits,
                       std::vector<std::pair<uint32_t, uint64_t>> &entries,
                       uint64_t last) {
  kernel_build_lrat_chain(entries, last, k.lrat_chain);
  k.proof->add_rup(id, lits.data(), static_cast<uint32_t>(lits.size()),
                   k.lrat_chain.data(),
                   static_cast<uint32_t>(k.lrat_chain.size()));
}

// Kissat shortens large clauses inside their existing arena records. Tactix's
// packed replacement is appended physically but inherits the source's current
// position in kernel_replace_clause. Restore those logical positions at the
// band-end collection: the next Vivify band rebuilds every large watch in
// arena order, so this is semantic state rather than a storage detail.
struct VivifyArenaOrder final : ArenaOrdering {
  VivifyReclamationBatch &batch;

  explicit VivifyArenaOrder(VivifyReclamationBatch &b) : batch(b) {}

  void retire() { batch.changed = true; }

  void inherit(Ref, Ref) { batch.changed = true; }

  void order(const Kernel &k, std::vector<Ref> &refs) override {
    kernel_sort_refs_by_logical_clause_position(k, refs);
  }

};

void VivifyReclamationBatch::collect(Kernel &k) {
  if (!changed)
    return;
  struct LogicalOrder final : ArenaOrdering {
    void order(const Kernel &kernel, std::vector<Ref> &refs) override {
      kernel_sort_refs_by_logical_clause_position(kernel, refs);
    }
  } logical_order;
  ArenaOrdering *const saved = k.arena_ordering;
  k.arena_ordering = &logical_order;
  kernel_collect_garbage(k);
  k.arena_ordering = saved;
  changed = false;
}

// Replace `c` (at cref) by `lits`, proof add before delete; `entries`+`last`
// form the LRAT chain. The replacement keeps the candidate's redundancy and
// takes the shared post-shrink glue (kernel_shrunken_glue). No
// partner-redundancy question arises: the strengthener is the propagation
// itself, so the rewrite is implied by F ∪ R and subsumes the original.
static ApplyResult rewrite(Kernel &k, Ref cref, const std::vector<Lit> &lits,
                           std::vector<std::pair<uint32_t, uint64_t>> &entries,
                           uint64_t last, VivifyArenaOrder &arena_order) {
  Clause *c = clause_at(k.arena, cref);
  const bool lrat = k.proof && k.proof->lrat();
  const bool redundant = c->redundant();
  const uint32_t glue = kernel_shrunken_glue(
      k, c->glue(), static_cast<uint32_t>(lits.size()));
  const uint64_t id = ++k.next_clause_id;
  if (k.proof) {
    if (lrat)
      emit_chain(k, id, lits, entries, last);
    else
      k.proof->add_rup(0, lits.data(), static_cast<uint32_t>(lits.size()),
                       nullptr, 0);
  }
  k.stats.vivified++;
  // Kissat rewrites a large candidate in place: it explicitly removes the
  // candidate's two old large watches before installing the shortened pair.
  // Tactix allocates a replacement clause, so do the same unwatch here before
  // kernel_replace_clause marks the old arena record garbage.  Leaving those
  // two dead entries for propagation to discover changed the tick ledger by a
  // few ticks per later candidate and moved the budget boundary.
  if (lits.size() > 2) {
    kernel_unwatch_large(k, c->lits[0], cref);
    kernel_unwatch_large(k, c->lits[1], cref);
  }
  // shared replacement tail; glue is a redundant-clause concept, 0 otherwise
  Ref replacement_ref = INVALID_REF;
  switch (kernel_replace_clause(k, c, id, lits.data(),
                                static_cast<uint32_t>(lits.size()), redundant,
                                redundant ? glue : 0, &replacement_ref)) {
  case ReplaceOutcome::Refuted:
    return ApplyResult::Unsat;
  case ReplaceOutcome::Unit:
    // Kissat keeps consuming the pre-built schedule after a Vivify unit.  It
    // first asserts and propagates the unit at the root; every later candidate
    // rechecks root values before probing, while its arena reference remains
    // valid until the round-end collection.
    k.unit_ids[var_of(lits[0])] = id;
    kernel_assign_unit(k, lits[0]);
    arena_order.retire();
    if (propagate_service(k) == ApplyResult::Unsat)
      return ApplyResult::Unsat;
    return ApplyResult::Ok;
  case ReplaceOutcome::Binary:
    // Kissat creates Vivify's binary replacement through `new_clause`, which
    // is a defragmentation checkpoint.  Large Vivify rewrites stay in place
    // there and specialized binary constructors in other rules are not
    // consumers of this checkpoint.
    k.watches.defrag_if_needed();
    arena_order.retire();
    return ApplyResult::Ok;
  case ReplaceOutcome::Large:
    arena_order.inherit(replacement_ref, cref);
    // Kissat first shrinks and rewatches the live candidate in place, then
    // repairs the watched-pair invariant at the common terminal.  Tactix's
    // packed replacement changes the Ref, so perform the same repair on the
    // new live record after kernel_replace_clause has installed it.  Doing
    // this before replacement reverses the source lifecycle: it backtracks
    // while the old clause is still live and only then mutates the database.
    c = clause_at(k.arena, replacement_ref);
    vivify_reestablish_watch_invariant(k, c->lits, c->size);
    return ApplyResult::Ok;
  default:
    assert(false);
    return ApplyResult::Unsat;
  }
}

struct VivifyDeduction {
  std::vector<Lit> learned;
  Ref subsuming = INVALID_REF;
  bool binary_subsuming = false;
  bool used_redundant = false;
};

// Vivify conflict analysis is itself a clause-use site. Kissat recomputes the
// glue of every redundant large reason it traverses (without setting the
// general conflict-analysis `used` counter) and promotes it immediately.
// The new tier is visible to later bands in this same Vivify invocation.
static void vivify_recompute_and_promote(Kernel &k, Clause *c) {
  if (!c->redundant() || c->glue() <= 1)
    return;
  const uint32_t old_glue = c->glue();
  uint32_t glue = 0;
  for (const Lit lit : *c) {
    const uint32_t level = k.var_level(var_of(lit));
    if (!k.marked_levels[level]) {
      k.marked_levels[level] = 1;
      k.touched_levels.push_back(level);
      if (++glue == old_glue)
        break;
    }
  }
  for (const uint32_t level : k.touched_levels)
    k.marked_levels[level] = 0;
  k.touched_levels.clear();
  if (glue < old_glue) {
    event_trace_clause_glue(k, "vivify", c, old_glue, glue);
    c->set_glue(glue);
    k.stats.promoted++;
  }
}

// Turn a learned clause into permanent formula material.  Vivify is the one
// rule which can discover that an irredundant candidate is subsumed by a
// redundant reason.  Deleting the former is equisatisfiable only if the latter
// leaves Reduce's population at the same instant (kissat vivify.c:1145-1163).
static void promote_irredundant(Kernel &k, Ref ref) {
  Clause *c = clause_at(k.arena, ref);
  assert(!c->garbage() && c->redundant());
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_clause_transition(k, "promote", c->lits, c->size,
                                      c->id());
#endif
  c->set_redundant(false);
  for (const Lit l : *c) {
    // Pinned Kissat promotes this clause through `mark_added_literals`, which
    // arms forward subsumption and Factor but deliberately not Eliminate.
    kernel_touch_var(k, var_of(l));
    kernel_mark_factor_literal(k, l);
    if (k.dense)
      k.dense->large[l].push_back(ref);
  }
  k.stats.vivify_promoted++;
}

// Analyze the implication graph under the candidate assumptions.  `analyzed`
// marks FALSE literals by variable (the true mate owns the reason).  The
// learned clause is the negation of the decision frontier plus an optional
// implied candidate literal.  During the same walk, a reason wholly contained
// in the candidate is a subsumer and every redundant reason is remembered for
// the asymmetric-tautology terminal guard.
static VivifyDeduction deduce(Kernel &k, Ref cref,
                              Lit implied,
                              const std::vector<uint8_t> &marked,
                              std::vector<uint8_t> &analyzed,
                              std::vector<Var> &touched) {
  VivifyDeduction d;
  std::vector<Lit> todo;
  const auto push_false = [&](Lit f) {
    const Var v = var_of(f);
    if (!k.var_level(v) || analyzed[v])
      return;
    analyzed[v] = 1;
    touched.push_back(v);
    todo.push_back(f);
  };
  const auto seed_reason = [&](const Lit *begin, const Lit *end,
                               bool may_subsume, bool redundant,
                               Ref reason_ref) {
    d.used_redundant |= redundant;
    bool subsumes = may_subsume;
    for (const Lit *p = begin; p != end; ++p) {
      const Lit l = *p;
      if (kernel_root_value(k, l) < 0)
        continue;
      if (!marked[l])
        subsumes = false;
      push_false(l);
    }
    if (may_subsume && redundant)
      vivify_recompute_and_promote(k, clause_at(k.arena, reason_ref));
    if (subsumes)
      d.subsuming = reason_ref;
  };

  if (implied != INVALID_LIT) {
    push_false(negate(implied));
    d.learned.push_back(implied);
  } else if (k.conflict_valid) {
    if (k.conflict_ref == INVALID_REF) {
      const Lit a = k.conflict_bin[0], b = k.conflict_bin[1];
      if (marked[a] && marked[b])
        d.binary_subsuming = true;
      else {
        push_false(a);
        push_false(b);
      }
    } else {
      const Clause *reason = clause_at(k.arena, k.conflict_ref);
      seed_reason(reason->begin(), reason->end(), true, reason->redundant(),
                  k.conflict_ref);
    }
  } else {
    const Clause *candidate = clause_at(k.arena, cref);
    seed_reason(candidate->begin(), candidate->end(), false,
                candidate->redundant(), cref);
  }

  size_t next = 0;
  while (next < todo.size() && d.subsuming == INVALID_REF &&
         !d.binary_subsuming) {
    const Lit false_lit = todo[next++];
    const Var v = var_of(false_lit);
    const Assigned &a = k.assigned[v];
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    const auto journal_reason = [&](const std::string &kind) {
      if (!vivify_journal_active)
        return;
      if (vivify_journal_facts.reasons == "-")
        vivify_journal_facts.reasons.clear();
      if (!vivify_journal_facts.reasons.empty())
        vivify_journal_facts.reasons.push_back('|');
      vivify_journal_facts.reasons +=
          std::to_string(kernel_externalize(k, false_lit)) + ':' + kind;
    };
#endif
    if (a.reason == DECISION_REASON) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      journal_reason("decision");
#endif
      d.learned.push_back(false_lit);
      continue;
    }
    if (a.reason == UNIT_REASON) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      journal_reason("unit");
#endif
      continue;
    }
    const Lit true_lit = negate(false_lit);
    if (a.binary) {
      // Kissat's Vivify walks the assignment's semantic binary reason, which
      // can be a virtual resolvent produced by reason jumping (vivify.c:
      // 525-542).  Using jumped_over here expands that resolvent too early:
      // it can hide a binary reason wholly contained in the candidate and
      // turn a subsumption into a binary rewrite.  Native proof walks still
      // expand the virtual reason through jumped_over in kernel_reason_walk.
      const Lit other = a.reason;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      journal_reason("binary(" +
                     std::to_string(kernel_externalize(k, other)) + ')');
#endif
      if (marked[true_lit] && marked[other]) {
        d.binary_subsuming = true;
        break;
      }
      push_false(other);
      continue;
    }
    const Ref rref = a.reason;
    const Clause *reason = clause_at(k.arena, rref);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    if (vivify_journal_active) {
      std::vector<Lit> reason_lits(reason->begin(), reason->end());
      journal_reason("large(id=" + std::to_string(reason->id()) +
                     ";lits=" +
                     vivify_journal_literals(k, reason_lits) + ')');
    }
#endif
    d.used_redundant |= reason->redundant();
    bool subsumes = true;
    for (const Lit other : *reason) {
      if (!marked[other])
        subsumes = false;
      if (other == true_lit || kernel_root_value(k, other) < 0)
        continue;
      push_false(other);
    }
    if (reason->redundant())
      vivify_recompute_and_promote(k, clause_at(k.arena, rref));
    if (subsumes)
      d.subsuming = rref;
  }
  return d;
}

static bool shrinkable(const Kernel &k, const std::vector<Lit> &probes,
                       const std::vector<Lit> &learned,
                       const std::vector<uint8_t> &analyzed, bool conflict) {
  if (learned.size() == probes.size())
    return false;
  uint32_t implied = 0;
  for (const Lit l : probes) {
    const int8_t value = k.value(l);
    if (!value)
      return true;
    if (value > 0) {
      if (conflict || implied++)
        return true;
    } else {
      const Assigned &a = k.assigned[var_of(l)];
      if (!analyzed[var_of(l)] || a.reason != DECISION_REASON)
        return true;
    }
  }
  return false;
}

// Probe one candidate; on an actionable outcome, run the complete reference
// terminal dispatch. `reuse` means the
// round hands over a live probing trail and owns the scope; otherwise this
// path opens and closes its own scope around a root-to-root probe. In both
// modes reset_to_root() runs before any mutation (design §3).
static ApplyResult vivify_one(Kernel &k, Ref cref, bool reuse,
                              VivifyArenaOrder &arena_order) {
  Clause *c = clause_at(k.arena, cref);
  if (!is_candidate(k, c)) {
    // re-check root satisfaction for deletion (kissat/CaDiCaL delete a
    // satisfied candidate here). Root, not current: with a retained trail a
    // speculatively satisfied clause is not deletable.
    if (!c->garbage() && c->size >= 3) {
      bool satisfied = false;
      for (const Lit l : *c)
        satisfied |= kernel_root_value(k, l) > 0;
      if (satisfied && in_scope(k, c)) {
        // Kissat marks a scheduled candidate garbage as soon as a root-fixed
        // literal satisfies it and preserves the current probing prefix.  The
        // deletion neither moves the arena nor installs watches, so Tactix can
        // do the same; the round-end reset precedes the deferred collection.
        kernel_delete_clause(k, c);
        arena_order.retire();
        k.stats.vivify_deleted++;
        return ApplyResult::Ok;
      }
    }
    return ApplyResult::NoOp;
  }
  k.stats.vivify_checked++;
  if (c->redundant() && c->glue() > k.tier2)
    k.stats.vivify_tier3++; // 0 here means the tier3 band is inert
  const bool lrat = k.proof && k.proof->lrat();
  // probe literals in the round's global literal order (design §2.2) — the
  // order is what makes a prefix shared with the next candidate, so it is not
  // separable from the reuse loop below
  std::vector<Lit> probes;
  std::vector<uint64_t> absorbed; // unit ids of root-falsified lits (LRAT)
  probes.reserve(c->size);
  probe_list(k, c, vivify_order(), probes, reuse);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (vivify_journal_active)
    vivify_journal_facts.probes = vivify_journal_literals(k, probes);
#endif
  if (lrat)
    for (const Lit l : *c)
      if (kernel_root_value(k, l) < 0)
        absorbed.push_back(k.unit_ids[var_of(l)]);
  if (probes.size() <= 2)
    return ApplyResult::NoOp; // effectively binary: C2 territory
  const uint64_t cid = c->id();
  if (reuse && k.level > 0) {
    // (a) The candidate may be the reason of a retained trail literal, the
    // prefix having been propagated ignoring the previous candidate (kissat
    // vivify.c:995-1005). Not only a quality question: in the implied-negation
    // case the candidate's id is the closing hint, so a reason walk reaching
    // it too would emit that id twice in one chain (design §3.2).
    for (const Lit l : *c) {
      const Var v = var_of(l);
      if (k.value(l) > 0 && k.var_level(v) > 0) {
        const Assigned &a = k.assigned[v];
        if (!a.binary && a.reason == cref) {
          kernel_backtrack(k, a.level - 1, /*update_phases=*/false);
          break;
        }
      }
    }
    // (b) Trim assumptions the previous (longer) candidate made that are not
    // literals of this one: VP3 makes them inert, and trimming keeps a value
    // from being read under an assumption outside the candidate (§3.4).
    if (k.level > probes.size())
      kernel_backtrack(k, static_cast<uint32_t>(probes.size()),
                       /*update_phases=*/false);
  }
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  // Persistent proof-walk and deduction scratch, restored in O(touched).
  static thread_local std::vector<uint8_t> seen;
  static thread_local std::vector<Var> seen_touched;
  if (lrat) {
    if (seen.size() < k.nvars)
      seen.resize(k.nvars, 0);
    seen_touched.clear();
  }
  // Marks on the probe literals (kissat vivify_mark_sorted_literals), so a
  // propagation can be scanned for an implied clause literal at once.
  // Mandatory under a count-based probe order, which can put the implied
  // literal anywhere the lazy `v > 0` test below would miss it.
  static thread_local std::vector<uint8_t> marked;
  static thread_local std::vector<uint8_t> analyzed;
  static thread_local std::vector<Var> analyzed_touched;
  if (marked.size() < 2 * k.nvars)
    marked.resize(2 * k.nvars, 0);
  if (analyzed.size() < k.nvars)
    analyzed.resize(k.nvars, 0);
  analyzed_touched.clear();
  for (const Lit l : probes)
    marked[l] = 1;

  ApplyResult res = ApplyResult::NoOp;
  // Watch-list and clause-header churn from the assumption propagations below
  // is speculative and is handed back to search when the scope closes. In
  // reuse mode the round owns the scope, so it is not reopened per candidate.
  if (!reuse)
    kernel_probing_begin(k, /*rollback_pose=*/false);
  uint32_t reused = 0, made = 0;
  uint32_t assumed = 0;
  Lit implied = INVALID_LIT;
  for (uint32_t i = 0; i < probes.size(); i++) {
    const Lit li = probes[i];
    const uint32_t lvl = assumed + 1;
    if (lvl <= k.level) {
      // Reuse test on the frame decision, not on the value: a literal false
      // for some other reason is not a reused assumption, and a value may
      // only be read once exactly this candidate's assumptions hold (VP3).
      if (k.trail[k.frames[lvl]] == negate(li)) {
        reused++;
        assumed++;
        continue;
      }
      kernel_backtrack(k, lvl - 1, /*update_phases=*/false);
    }
    assert(k.level == lvl - 1); // VP3: only this candidate's assumptions hold
    const int8_t v = k.value(li);
    if (v > 0) {
      implied = li;
      break;
    }
    if (v < 0)
      continue;
    const uint32_t before = static_cast<uint32_t>(k.trail.size());
    assume(k, negate(li));
    assumed++;
    made++;
    if (kernel_propagate_probing(k, cref)) {
      break;
    }
    // Eager implied literal (kissat vivify.c:1078-1090): scan what this
    // assumption propagated for another literal of the candidate, giving
    // (probes[0..i], implied) — stronger than waiting for position j > i.
    // Kissat's `better_implied` refinement is completed below after the
    // assumption loop: this eager scan only discovers that an implication
    // exists, while the later scan selects the earliest candidate literal in
    // trail order (vivify.c:1101-1117).
    // From before + 1: trail[before] is the assumption itself, a marked
    // literal's negation and never an implied one.
    for (uint32_t t = before + 1; t < k.trail.size(); t++)
      if (marked[k.trail[t]]) {
        implied = k.trail[t];
        break;
      }
    if (implied != INVALID_LIT)
      break;
  }

  // The first marked true literal in trail order is the canonical implied
  // witness, independent of which probe happened to notice it.
  if (!k.conflict_valid && implied != INVALID_LIT)
    for (const Lit l : probes)
      if (k.value(l) > 0 &&
          k.assigned[var_of(l)].trail < k.assigned[var_of(implied)].trail)
        implied = l;

  const bool conflict = k.conflict_valid;
  const uint32_t level_after_assumptions = k.level;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (vivify_journal_active)
    vivify_journal_facts.trail = vivify_journal_literals(k, k.trail);
  if (vivify_journal_active)
    vivify_journal_facts.selected_cause =
        vivify_journal_selected_cause(k);
  if (vivify_journal_active && conflict) {
    std::vector<Lit> conflict_lits;
    if (k.conflict_ref == INVALID_REF) {
      conflict_lits.push_back(k.conflict_bin[0]);
      conflict_lits.push_back(k.conflict_bin[1]);
    } else {
      const Clause *conflict_clause = clause_at(k.arena, k.conflict_ref);
      conflict_lits.assign(conflict_clause->begin(), conflict_clause->end());
    }
    vivify_journal_facts.conflict_clause =
        vivify_journal_literals(k, conflict_lits);
  }
#endif
  VivifyDeduction d =
      deduce(k, cref, implied, marked, analyzed, analyzed_touched);
  c = clause_at(k.arena, cref);
  const bool has_subsumer = d.binary_subsuming || d.subsuming != INVALID_REF;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (vivify_journal_active) {
    vivify_journal_facts.implied =
        implied == INVALID_LIT ? 0 : kernel_externalize(k, implied);
    vivify_journal_facts.conflict = conflict;
    vivify_journal_facts.subsumer = has_subsumer;
    vivify_journal_facts.learned = vivify_journal_literals(k, d.learned);
    vivify_journal_facts.reused = reused;
    vivify_journal_facts.made = made;
    vivify_journal_facts.assumed = assumed;
  }
#endif
  if (has_subsumer) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    if (vivify_journal_active)
      vivify_journal_facts.terminal = "subsumed";
#endif
    const bool candidate_redundant = c->redundant();
    if (!d.binary_subsuming) {
      Clause *sub = clause_at(k.arena, d.subsuming);
      if (candidate_redundant && sub->redundant() && c->glue() < sub->glue())
        sub->set_glue(c->glue());
      else if (!candidate_redundant && sub->redundant())
        promote_irredundant(k, d.subsuming);
    }
    c = clause_at(k.arena, cref);
    kernel_delete_clause(k, c);
    arena_order.retire();
    k.stats.vivify_subsumed++;
    res = ApplyResult::Ok;
  } else {
    const bool can_shrink =
        shrinkable(k, probes, d.learned, analyzed, conflict);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    if (vivify_journal_active)
      vivify_journal_facts.shrinkable = can_shrink;
#endif
    if (can_shrink) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (vivify_journal_active)
        vivify_journal_facts.terminal = "shrunken";
#endif
    uint64_t last = 0;
    if (lrat) {
      if (implied != INVALID_LIT)
        kernel_reason_walk(k, var_of(implied), seen, entries, &seen_touched);
      else if (conflict)
        last = kernel_conflict_reason_walk(k, seen, entries, &seen_touched);
      else {
        for (const Lit l : probes) {
          const Assigned &a = k.assigned[var_of(l)];
          if (k.value(l) < 0 && a.reason != DECISION_REASON)
            kernel_reason_walk(k, var_of(l), seen, entries, &seen_touched);
        }
        last = cid;
      }
      for (const uint64_t u : absorbed)
        entries.emplace_back(0, u);
    }
    // Units and binaries force a root-level database transition in Kissat.
    // A large learned clause instead retains the valid assumption prefix. If
    // there is no implied literal, Kissat always trims that prefix to
    // new_size-2 before choosing replacement watches (vivify.c:
    // vivify_learn_large), not only on the explicit-conflict path.  The trim
    // changes which high-level false literals are still assigned and is thus
    // part of the replacement-watch precondition, not cleanup.
    std::vector<Lit> replacement = d.learned;
    if (replacement.size() <= 2) {
      reset_to_root(k);
    } else {
      vivify_trim_replacement_prefix(
          k, implied, static_cast<uint32_t>(replacement.size()));
      vivify_pose_replacement_watches(k, replacement);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (vivify_journal_active) {
        vivify_journal_facts.replacement_pose =
            vivify_journal_literals(k, replacement);
        vivify_journal_facts.replacement_levels.clear();
        for (const Lit lit : replacement) {
          if (!vivify_journal_facts.replacement_levels.empty())
            vivify_journal_facts.replacement_levels.push_back(',');
          vivify_journal_facts.replacement_levels +=
              std::to_string(kernel_externalize(k, lit)) + ':' +
              std::to_string(k.var_level(var_of(lit)));
        }
      }
#endif
    }
    res = rewrite(k, cref, replacement, entries, last, arena_order);
    k.stats.vivify_shrunken++;
    } else if (implied != INVALID_LIT && c->redundant()) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (vivify_journal_active)
        vivify_journal_facts.terminal = "implied-delete";
#endif
    kernel_delete_clause(k, clause_at(k.arena, cref));
    arena_order.retire();
    k.stats.vivify_implied++;
    res = ApplyResult::Ok;
    } else if ((conflict || implied != INVALID_LIT) && !c->redundant() &&
             !d.used_redundant) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (vivify_journal_active)
        vivify_journal_facts.terminal = "asymmetric-delete";
#endif
    kernel_delete_clause(k, clause_at(k.arena, cref));
    arena_order.retire();
    k.stats.vivify_asymmetric++;
    res = ApplyResult::Ok;
    } else if (implied != INVALID_LIT) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (vivify_journal_active)
        vivify_journal_facts.terminal = "implied-decline";
#endif
    k.stats.vivify_noop++;
    } else if (implied == INVALID_LIT && assumed == probes.size()) {
    // No deduction: reverse the last assumption and test whether that literal
    // can be removed (kissat vivify.c:1180-1200).  The candidate is ignored;
    // a conflict proves the shortened clause by RUP.
    const Lit remove = probes.back();
    kernel_backtrack(k, k.level - 1, /*update_phases=*/false);
    // Kissat's conflict is a local return value.  The first deduction has
    // consumed it; the reverse-last-literal transaction starts clean.
    k.conflict_valid = false;
    k.conflict_ref = INVALID_REF;
    assume(k, remove);
    made++;
    if (kernel_propagate_probing(k, cref)) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (vivify_journal_active)
        vivify_journal_facts.terminal = "instantiated";
#endif
      // Kissat strengthens a large instantiation in the candidate's current
      // physical order, then backtracks two levels before selecting the new
      // watches (vivify.c:large_strengthen_after_instantiation).  Rebuilding
      // from `probes` after a root reset preserves the clause set but loses
      // that pose; the very next candidate can then use a different reason.
      // Tactix allocates a packed replacement, so reconstruct the exact
      // in-place result before handing it to the common rewrite primitive.
      std::vector<Lit> shorter;
      c = clause_at(k.arena, cref);
      shorter.reserve(c->size - 1);
      for (const Lit l : *c)
        if (l != remove && kernel_root_value(k, l) >= 0)
          shorter.push_back(l);
      // The chain is walked before any backtrack: it reads the conflict and
      // the trail positions this propagation produced.
      uint64_t last = 0;
      if (lrat) {
        last = kernel_conflict_reason_walk(k, seen, entries, &seen_touched);
        if (seen[var_of(remove)])
          entries.emplace_back(k.assigned[var_of(remove)].trail, cid);
        for (const uint64_t u : absorbed)
          entries.emplace_back(0, u);
      }
      // Kissat splits the instantiation result on the assumption level
      // (vivify_strengthen_after_instantiation): at level 3 the surviving
      // non-root literals are exactly two and
      // binary_strengthen_after_instantiation returns to the root before
      // installing the binary; deeper, large_strengthen_after_instantiation
      // keeps level-2 and re-poses the watched pair. `shorter` already holds
      // that same root-filtered result, so branch on it rather than on the
      // level — a root literal forced by this round's own probing propagation
      // can shrink a deeper instantiation to a binary as well, and only the
      // large case has a watched pair to pose.
      if (shorter.size() > 2) {
        const uint32_t retained = k.level > 1 ? k.level - 2 : 0;
        kernel_backtrack(k, retained, /*update_phases=*/false);
        vivify_pose_replacement_watches(k, shorter);
      } else {
        reset_to_root(k);
      }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (vivify_journal_active) {
        vivify_journal_facts.replacement_pose =
            vivify_journal_literals(k, shorter);
        vivify_journal_facts.replacement_levels.clear();
        for (const Lit lit : shorter) {
          if (!vivify_journal_facts.replacement_levels.empty())
            vivify_journal_facts.replacement_levels.push_back(',');
          vivify_journal_facts.replacement_levels +=
              std::to_string(kernel_externalize(k, lit)) + ':' +
              std::to_string(k.var_level(var_of(lit)));
        }
      }
#endif
      res = rewrite(k, cref, shorter, entries, last, arena_order);
      k.stats.vivify_instantiated++;
    } else {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (vivify_journal_active)
        vivify_journal_facts.terminal = "instantiation-decline";
#endif
      const uint32_t keep = k.level > 1 ? k.level - 2 : 0;
      kernel_backtrack(k, keep, /*update_phases=*/false);
      k.stats.vivify_noop++;
    }
    } else {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (vivify_journal_active)
        vivify_journal_facts.terminal = "no-deduction";
#endif
    k.stats.vivify_noop++;
    }
  }
  for (const Lit l : probes) // restore the persistent mark scratch all-zero
    marked[l] = 0;
  for (const Var v : analyzed_touched)
    analyzed[v] = 0;
  k.stats.vivify_reused += reused;
  k.stats.vivify_probes += made;
  if (reused && made == 0 && assumed == probes.size())
    k.stats.vivify_reuse_full++; // wholly covered: nothing new was assumed
  if (!reuse) {
    reset_to_root(k); // standalone mode returns to the root every candidate
  } else {
    // Kissat's conflict is a local return value.  If the terminal path kept
    // the whole conflicting assumption stack, drop at least its last frame;
    // otherwise preserve the valid prefix selected by the rewrite/no-op path.
    if (conflict && k.level == level_after_assumptions && k.level)
      kernel_backtrack(k, k.level - 1, /*update_phases=*/false);
    c = clause_at(k.arena, cref);
    if (!c->garbage())
      vivify_reestablish_watch_invariant(k, c->lits, c->size);
    k.conflict_valid = false;
    k.conflict_ref = INVALID_REF;
  }
  for (const Var tv : seen_touched) // restore the persistent scratch all-zero
    seen[tv] = 0;
  return res;
}

// canonical instance: one full round over the tier-aware schedule
ApplyResult vivify_apply_batched(Kernel &k, VivifyReclamationBatch &batch) {
  assert(vivify_guard(k));
  // The caller poses Kissat's cumulative ceiling before entering the round.
  // Scheduling and watch rebuilding consume that same probing-tick allowance;
  // do not restart the relative clock after the schedule has been built.
  const uint64_t tick_start = k.stats.ticks;
  // D-LEVEL guard purification: complete the root closure U (no-op at a
  // fixpoint); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.vivify_rounds++;
  // The round owns the probing scope from here and must not inherit one: a
  // scope left recording by an earlier rule would make the `!recording` test
  // below skip opening this round's. Idempotent outside a scope.
  kernel_probing_end(k);
  // Kissat enters every Vivify round through `kissat_flush_large_watches`:
  // large watches are disconnected and duplicate binaries are retired before
  // root simplification, occurrence counting, and candidate scheduling.  In
  // particular duplicates learned during one dose must not survive into the
  // next dose's probing cost or physical antecedent order.
  kernel_flush_large_watches(k);
  std::vector<Ref> candidates;
  VivifySchedule sched;
  vivify_schedule(k, candidates, &sched);
  if (sched.refuted)
    return ApplyResult::Unsat;
  if (sched.root_unit) {
    if (inprocess_reach_root(k))
      return ApplyResult::Unsat;
    return ApplyResult::Ok;
  }
  // Kissat flushes large watches before scheduling and reconnects all live
  // clauses after committing the candidate literal order.  The resulting
  // watch pose controls which conflict/reason a probing propagation records.
  event_trace_slot_boundary(k, "vivify-dense-resume", "entry");
  // A preceding band would normally have collected into logical clause
  // order.  While reclamation is deferred, reproduce that exact rewatch stream
  // from the persistent source-position keys without moving any arena record.
  kernel_rebuild_large_watches(k, batch.changed);
  event_trace_slot_boundary(k, "vivify-dense-resume", "resume");
  k.stats.vivify_prioritized += sched.prioritized;
  k.stats.vivify_reprioritized += sched.rearmed;
  k.stats.vivify_flushed += sched.flushed;
  uint64_t changed = sched.flushed + sched.root_changed;
  const bool reuse = k.vivify_reuse;
  // k.work_budget is the cumulative Vivify ceiling in propagation ticks.
  // Zero is unbounded only when work_budget_bounded is false.
  uint64_t ordinal = 0;
  VivifyArenaOrder arena_order(batch);
  if (sched.root_changed)
    arena_order.retire();
  for (const Ref cref : candidates) {
    if (k.work_budget_bounded || k.work_budget) {
      const uint64_t used = k.stats.ticks - tick_start;
      if (k.work_budget_offset > k.work_budget ||
          used > k.work_budget - k.work_budget_offset) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
        event_trace_vivify_candidate(
            k, "vivify_budget_stop", ordinal, cref,
            k.work_budget_offset + used, k.work_budget);
#endif
        break;
      }
    }
    // Termination poll on every candidate (kissat vivify.c:1291), with no
    // `!(++tick & 255)` window: a window an instance never reaches makes the
    // abort path vacuous. The two counters are separate on purpose.
    k.stats.vivify_polls++;
    if (kernel_terminating(k)) {
      k.stats.vivify_terminated++;
      break;
    }
    // Reopen the scope only when the previous candidate closed it by mutating
    // the database. Test `recording`, not `level == 0`: a candidate can
    // decline at the root without closing the scope, and reopening a
    // recording scope rolls the round's watch state back.
    if (reuse && !k.probing.recording)
      kernel_probing_begin(k, /*rollback_pose=*/false);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_vivify_candidate(
        k, "vivify_candidate", ordinal, cref,
        k.work_budget_offset + (k.stats.ticks - tick_start), k.work_budget);
#endif
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    Clause *journal_clause = clause_at(k.arena, cref);
    uint32_t journal_rank_count = 0;
    for (const Lit lit : *journal_clause)
      journal_rank_count =
          std::max(journal_rank_count, vivify_order().counts[lit]);
    const std::string journal_candidate =
        vivify_journal_candidate(k, journal_clause, ordinal,
                                 journal_rank_count);
    const bool journal =
        rule_journal_wants(k, "Vivify", journal_candidate);
    VivifyJournalSnapshot journal_pre{};
    uint64_t journal_digest = 0, journal_formula_epoch = 0;
    uint64_t journal_root_epoch = 0;
    std::string journal_selected_pose_pre;
    if (journal) {
      vivify_journal_facts = VivifyJournalFacts{};
      vivify_journal_active = true;
      journal_pre = vivify_journal_snapshot(k, journal_clause);
      journal_digest = rule_journal_digest(k);
      journal_formula_epoch = k.arena_generation;
      journal_root_epoch = k.touch_fixed_head;
      journal_selected_pose_pre = vivify_journal_selected_clause_pose(k);
    }
#endif
    const ApplyResult r = vivify_one(k, cref, reuse, arena_order);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_vivify_candidate(
        k, "vivify_candidate_end", ordinal, cref,
        k.work_budget_offset + (k.stats.ticks - tick_start), k.work_budget);
#endif
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    if (journal) {
      vivify_journal_emit_candidate(
          k, journal_candidate, journal_pre, journal_digest,
          journal_formula_epoch, journal_root_epoch,
          journal_selected_pose_pre, r);
      vivify_journal_active = false;
    }
#endif
    ordinal++;
    // Cleared once a candidate has been tried, so a budget cut or termination
    // leaves exactly the untried tail armed for the next round (kissat
    // vivify.c:1311). A garbage clause still has its header until the collect.
    clause_at(k.arena, cref)->set_vivify(false);
    if (r == ApplyResult::Unsat)
      return ApplyResult::Unsat;
    if (r == ApplyResult::Ok)
      changed++;
  }
  reset_to_root(k); // the round's single exit: budget cut, termination, and
                    // schedule completion all leave through here
  if (!changed)
    return ApplyResult::NoOp;
  return ApplyResult::Ok;
}

ApplyResult vivify_apply(Kernel &k) {
  VivifyReclamationBatch batch;
  const ApplyResult result = vivify_apply_batched(k, batch);
  if (result != ApplyResult::Unsat && !k.inconsistent)
    batch.collect(k);
  return result;
}

ApplyResult vivify_try(Kernel &k, Ref candidate) {
  assert(vivify_guard(k));
  VivifyReclamationBatch batch;
  VivifyArenaOrder arena_order(batch);
  // Standalone: its own root-to-root probing scope, no retained trail — the N1
  // surface must not depend on a round having run first.
  const ApplyResult r =
      vivify_one(k, candidate, /*reuse=*/false, arena_order);
  if (r == ApplyResult::Ok)
    batch.collect(k);
  return r;
}

static uint32_t vivify_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Ref> candidates;
  vivify_candidates(k, candidates);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(candidates.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = candidates[i];
  return n;
}

static ApplyResult vivify_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL guard purification: complete the root closure U (no-op at a
  // fixpoint); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return vivify_apply(k);
  for (Ref ref = 0; ref < k.arena.size();) {
    if (ref == inst)
      return vivify_try(k, ref);
    if (ref > inst)
      break;
    ref += clause_actual_words(clause_at(k.arena, ref));
  }
  return ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(Vivify, RuleTier::Inprocess, vivify_guard,
                     vivify_apply, vivify_instances, vivify_apply_instance);

} // namespace tactix
