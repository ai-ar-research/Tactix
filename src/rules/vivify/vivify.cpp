// See vivify.hpp for the reference/deviation record.
#include "rules/vivify/vivify.hpp"

#include "diagnostics/rule_journal.hpp"
#include "diagnostics/event_trace.hpp"
#include "rules/instance_helpers.hpp"
#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <unordered_map>

namespace tactix {

#ifdef TACTIX_ENABLE_RULE_JOURNAL
namespace {
// Diagnostic spelling of literals as comma-separated DIMACS literals.
template <class Lits>
std::string vivify_journal_literals(const Kernel &k, const Lits &lits) {
  std::string out;
  for (const Lit lit : lits) {
    if (!out.empty())
      out.push_back(',');
    out += std::to_string(kernel_externalize(k, lit));
  }
  return out;
}

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
  int implied = 0;
  bool conflict = false;
  bool subsumer = false;
  bool shrinkable = false;
  uint32_t reused = 0, made = 0, assumed = 0;
  const char *terminal = "none";
};
static thread_local bool vivify_journal_active;
static thread_local VivifyJournalFacts vivify_journal_facts;

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
                    ";lits=" + vivify_journal_literals(k, *c);
  return out;
}

static void vivify_journal_emit_candidate(
    const Kernel &k, const std::string &candidate,
    const VivifyJournalSnapshot &pre, uint64_t pre_digest,
    uint64_t formula_epoch, uint64_t root_epoch, ApplyResult result) {
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
      ";selected-cause=-;selected-pose-pre=-;selected-pose-post=-" +
      ";reasons=" + vivify_journal_facts.reasons +
      ";reused=" + std::to_string(vivify_journal_facts.reused) +
      ";made=" + std::to_string(vivify_journal_facts.made) +
      ";assumed=" + std::to_string(vivify_journal_facts.assumed);
  record.effort = post.ticks - pre.ticks;
  record.budget = k.work_budget;
  record.post_digest = rule_journal_digest(k);
  rule_journal_emit(k, record);
}

// Name the terminal reached by the journaled candidate.
static void vivify_journal_terminal(const char *terminal) {
  if (vivify_journal_active)
    vivify_journal_facts.terminal = terminal;
}

// The replacement's posed literals and their current decision levels.
static void vivify_journal_replacement(const Kernel &k,
                                       const std::vector<Lit> &replacement) {
  if (!vivify_journal_active)
    return;
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
} // namespace
#else
static void vivify_journal_terminal(const char *) {}
static void vivify_journal_replacement(const Kernel &,
                                       const std::vector<Lit> &) {}
#endif

RuleEligibility vivify_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct (D-SPLIT-3): this is a pure
  // F-rewrite, sound regardless of a recorded conflict.
  return root_level_guard(k);
}

// With a retained probing trail k.value() is speculative, so candidacy, probe
// lists and root-satisfied deletion read the root value (kissat `kissat_fixed`;
// design §3.1). `in_scope`: does `c` fall in the band `k.vivify_scope` selects?
// The redundant side uses kissat's three tiers (vivify.c:236-256; design §2.5).
static bool in_scope(const Kernel &k, const Clause *c) {
  const bool red = c->redundant();
  const uint32_t glue = c->glue();
  switch (k.vivify_scope) {
  case 1:
    return red && glue <= k.tier2; // the whole redundant band
  case 2:
    return !red;                        // irredundant
  case 3:
    return red && glue <= k.tier1;      // tier1
  case 4:
    return red && glue > k.tier1 && glue <= k.tier2; // tier2
  case 5:
    return red && glue > k.tier2;       // tier3 (C10 hand-off H3)
  default:
    // Scope 0 is the unscoped N1 path: every band, tier3 included
    // (tests/test_vivify_n1.cpp).
    return true;
  }
}

// No clause-size cutoff: the tick budget is Kissat's only work bound
// (vivify.c:1254-1327), and even a no-edit probe updates retained state.
static bool is_candidate(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->size < 3)
    return false;
  // The live tier (kissat `vivify_tier2_limit`, vivify.c:132,
  // `vivifyfocusedtiers`), binding only for scopes 1/3/4.
  if (c->redundant() && c->glue() > k.tier2 && k.vivify_scope != 0 &&
      k.vivify_scope != 5)
    return false;
  for (const Lit l : *c)
    if (kernel_root_value(k, l) > 0)
      return false;
  return true;
}

// --- the round's global literal order ---
// The order enables cross-candidate reuse (cadical vivify.cpp:35-42; kissat
// vivify_sort_clause_by_counts). A canonical round also commits that pose to
// each full-size scheduled clause before rebuilding large watches, which
// affects which conflict propagation finds. One candidate: its reference,
// Kissat's `countref` fields and the offset of its literal snapshot in `keys`.
struct VivifyCand {
  Ref ref;
  uint32_t first_count; // leading key count; fills the pre-arena-order padding
  uint64_t arena_order;
  uint32_t prioritized, redundant, band, size;
  uint32_t key_off, key_len; // literal snapshot at scheduling time
};

// Canonical irredundant ranking needs no literal snapshot or band metadata.
struct VivifyRank {
  uint64_t arena_order;
  Ref ref;
  uint32_t count, inverse_size;
};

struct VivifyOrder {
  const Kernel *kernel = nullptr;
  std::vector<uint32_t> counts; // per-literal occurrences; persistent scratch
  std::vector<Lit> touched;     // ...restored in O(touched), never O(nvars)
  // Schedule and snapshot kept across calls for their capacity.
  std::vector<VivifyCand> cands;
  std::vector<Lit> keys;
  std::vector<VivifyRank> ranks, radix_scratch;
  std::vector<size_t> radix_counts;
  bool operator()(Lit a, Lit b) const { // a ≺ b: more occurrences first
    if (counts[a] != counts[b])
      return counts[a] > counts[b];
    // Kissat's `more_occurrences` breaks equal counts by descending internal
    // literal (vivify.c:24-31); the pose is committed, so the tie is
    // observable.
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

// Non-root-fixed literals of `c` in probe order (clause order unless
// `ordered`).
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

// Stable LSD radix sort of `items` by the low `bits` of `key`, 16 bits per
// pass; `scratch` and `counts` are caller-owned buffers.
template <class Item, class Key>
static void radix_sort_16(std::vector<Item> &items, std::vector<Item> &scratch,
                          std::vector<size_t> &counts, Key key,
                          unsigned bits) {
  scratch.resize(items.size());
  counts.resize(size_t{1} << 16);
  for (unsigned shift = 0; shift < bits; shift += 16) {
    std::fill(counts.begin(), counts.end(), 0);
    for (const Item &item : items)
      ++counts[(key(item) >> shift) & 0xffffu];
    size_t offset = 0;
    for (size_t &count : counts) {
      const size_t next = offset + count;
      count = offset;
      offset = next;
    }
    for (const Item &item : items)
      scratch[counts[(key(item) >> shift) & 0xffffu]++] = item;
    items.swap(scratch);
  }
}

// The 16-bit passes that cover `max`: at least one, at most four.
static unsigned radix_bits(uint64_t max) {
  unsigned bits = 16;
  while (bits < 64 && (max >> bits))
    bits += 16;
  return bits;
}

// Only after both root-rewrite passes; Refs keep pass-time band membership.
static void vivify_compact_schedule(const Kernel &k, VivifyOrder &ord,
                                    std::vector<Ref> &out,
                                    VivifySchedule &commit,
                                    uint64_t prioritized) {
  Arena &arena = const_cast<Arena &>(k.arena);
  const bool rearm = prioritized == 0 && !out.empty();
  commit.rearmed = rearm;
  commit.prioritized = rearm ? out.size() : prioritized;
  if (rearm)
    for (Ref ref : out)
      clause_at(arena, ref)->set_vivify(true);

  if (k.vivify_scope != 2) {
    // Precise redundant ordering commits the literal pose before rebuilding
    // watches, including clauses whose root rewrite moved them to a new tier.
    for (Ref ref : out) {
      Clause *c = clause_at(arena, ref);
      std::sort(c->begin(), c->end(), [&ord](Lit a, Lit b) { return ord(a, b); });
    }
    std::sort(out.begin(), out.end(), [&](Ref ar, Ref br) {
      const Clause *a = clause_at(arena, ar), *b = clause_at(arena, br);
      if (a->vivify() != b->vivify())
        return a->vivify();
      const uint32_t n = std::min(a->size, b->size);
      for (uint32_t i = 0; i < n; ++i) {
        const uint32_t ac = ord.counts[a->lits[i]];
        const uint32_t bc = ord.counts[b->lits[i]];
        if (ac != bc)
          return ac > bc;
      }
      if (a->size != b->size)
        return a->size < b->size;
      const uint64_t ao = kernel_import_lit_order(k, a->lits[a->size - 1]);
      const uint64_t bo = kernel_import_lit_order(k, b->lits[b->size - 1]);
      if (ao != bo)
        return ao > bo;
      return kernel_clause_position(k, ar) > kernel_clause_position(k, br);
    });
    return;
  }

  uint64_t live_irredundant = 0, live_redundant = 0;
  for (const auto [ref, c] : arena_records(arena)) {
    if (!c->garbage())
      (c->redundant() ? live_redundant : live_irredundant)++;
  }
  const bool skip_counts = strategy::vivify_skip_global_order(
      live_irredundant, live_redundant,
      strategy::VivifyParameters::from_kernel(k));
  if (out.size() < 2)
    return;
  auto &ranks = ord.ranks;
  auto &scratch = ord.radix_scratch;
  auto &counts = ord.radix_counts;
  ranks.clear();
  ranks.reserve(out.size());
  uint64_t max_order = 0;
  for (Ref ref : out) {
    const Clause *c = clause_at(arena, ref);
    uint32_t maximum = 0;
    if (!skip_counts)
      for (Lit lit : *c)
        maximum = std::max(maximum, ord.counts[lit]);
    const uint64_t position = kernel_clause_position(k, ref);
    max_order = std::max(max_order, position);
    ranks.push_back({position, ref, maximum, ~c->size});
  }
  // Stable LSD passes give ascending (count, inverse size, logical position);
  // reversing each priority partition gives the native pop order.
  const auto radix = [&](auto key, unsigned bits) {
    radix_sort_16(ranks, scratch, counts, key, bits);
  };
  radix([](const VivifyRank &rank) { return rank.arena_order; },
        radix_bits(max_order));
  if (!skip_counts) {
    radix([](const VivifyRank &rank) { return rank.inverse_size; }, 32);
    radix([](const VivifyRank &rank) { return rank.count; }, 32);
  }
  out.clear();
  for (bool priority : {true, false})
    for (auto i = ranks.rbegin(); i != ranks.rend(); ++i)
      if (clause_at(arena, i->ref)->vivify() == priority)
        out.push_back(i->ref);
}

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
  using Cand = VivifyCand;
  std::vector<Cand> &cands = ord.cands;
  std::vector<Lit> &keys = ord.keys;
  cands.clear();
  keys.clear();
  uint64_t prioritized = 0;
  const bool compact = commit && k.vivify_reuse && !k.vivify_flush &&
                       k.vivify_scope >= 1 && k.vivify_scope <= 5;

  const auto append_candidate = [&](Ref ref, bool priority) {
    if (compact) {
      out.push_back(ref);
      prioritized += priority;
      return;
    }
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
    cands.push_back({ref, 0,
                     kernel_clause_position(k, ref),
                     priority ? 1u : 0u,
                     c->redundant() ? 1u : 0u, band, c->size, key_off,
                     c->size});
    prioritized += priority;
  };
  const auto append_priority = [&](bool priority) {
    for (const auto [ref, c] : arena_records(k.arena)) {
      if (c->vivify() == priority && is_candidate(k, c) && in_scope(k, c))
        append_candidate(ref, priority);
    }
  };
  // Kissat schedules unprioritized clauses first, then prioritized; counting is
  // the second pass, so each prioritized clause is counted just before its own
  // simplification.
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
            kernel_flush_root_clause(mut, &replacement, &changed,
                                     /*mark_binary_added_material=*/true,
                                     /*clamp_glue=*/true);
        commit->root_changed += changed;
        if (retain_priority && outcome == RootFlushOutcome::Rewritten)
          clause_at(mut.arena, replacement)->set_vivify(true);
        if (outcome == RootFlushOutcome::Unit)
          commit->root_unit = true;
        else if (outcome == RootFlushOutcome::Refuted)
          commit->refuted = true;
        // vivify.c:schedule_vivification_candidates filters tier and priority
        // before simplification and does not recheck the lowered glue; carry
        // the pass-time membership to the fresh Ref.
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
    for (const auto [ref, c] : arena_records(k.arena)) {
      if (!c->garbage())
        for (const Lit l : *c) {
          if (ord.counts[l]++ == 0)
            ord.touched.push_back(l);
        }
    }

  if (commit)
    simplify_priority(true);
  else
    append_priority(true);
  if (commit && (commit->root_unit || commit->refuted)) {
    out.clear();
    return;
  }

  // Diagnostic: the counts consumed by Kissat's `more_occurrences` sorter.
  event_trace_vivify_counts(k, ord.counts.data(), ord.counts.size());

  if (compact) {
    vivify_compact_schedule(k, ord, out, *commit, prioritized);
    return;
  }

  uint64_t live_irredundant = 0, live_redundant = 0;
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (!c->garbage())
      (c->redundant() ? live_redundant : live_irredundant)++;
  }
  const bool radix_reverse_irredundant =
      k.vivify_scope == 2 && !k.vivify_flush &&
      strategy::vivify_skip_global_order(
          live_irredundant, live_redundant,
          strategy::VivifyParameters::from_kernel(k));

  // Sort snapshots only when Kissat sorts; the large-irredundant regime skips
  // global sorting and vivify_one sorts each reached probe stack.
  if (k.vivify_reuse && !radix_reverse_irredundant)
    for (const Cand &cand : cands) {
      const auto begin = keys.begin() + cand.key_off;
      const auto end = begin + cand.key_len;
      if (k.vivify_scope == 2 && !k.vivify_flush) {
        // The pure irredundant schedule uses only the maximum count (Kissat
        // init_countref COUNTREF_COUNTS == 1) and does not commit a pose.
        const auto best = std::max_element(begin, end, [&ord](Lit a, Lit b) {
          return ord.counts[a] < ord.counts[b];
        });
        std::iter_swap(begin, best);
      } else {
        std::sort(begin, end, [&ord](Lit a, Lit b) { return ord(a, b); });
      }
    }
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

  // kissat's precise redundant-band sorter writes the count order into each
  // clause before `watch_large_clauses`. Clauses with root-false literals are
  // left to vivify_one; full-size ones are permuted here.
  if (commit && k.vivify_reuse) {
    Arena &arena = const_cast<Arena &>(k.arena);
    for (const Cand &cand : cands) {
      Clause *clause = clause_at(arena, cand.ref);
      // Kissat's irredundant sorter leaves the clause pose untouched.
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

  // Kissat skips candidate sorting when irredundant clauses outnumber redundant
  // by more than 10:1 (reverse arena order); a stable radix pass over
  // `arena_order` restores that order, pass zero before pass one.
  if (radix_reverse_irredundant && cands.size() > 1) {
    uint64_t max_order = 0;
    for (const Cand &cand : cands)
      max_order = std::max(max_order, cand.arena_order);
    std::vector<Cand> scratch;
    std::vector<size_t> counts;
    radix_sort_16(cands, scratch, counts,
                  [](const Cand &cand) { return cand.arena_order; },
                  radix_bits(max_order));
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
    // Cache the first comparison key; refill on every call.
    if (k.vivify_reuse)
      for (Cand &cand : cands)
        cand.first_count = ord.counts[kb[cand.key_off]];
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
                // Large irredundant population: no sorting (vivify.c:1443);
                // popped from the back.
                if (strategy::vivify_skip_global_order(
                        live_irredundant, live_redundant,
                        strategy::VivifyParameters::from_kernel(k)))
                  return a.arena_order > b.arena_order;
                // Kissat COUNTREF_COUNTS == 1: maximum count, then inverse
                // size; descending reference is the final tie.
                const uint32_t ac = a.first_count;
                const uint32_t bc = b.first_count;
                if (ac != bc)
                  return ac > bc;
                if (a.key_len != b.key_len)
                  return a.key_len < b.key_len;
                return a.arena_order > b.arena_order;
              }
              // Lexicographic over counts (Kissat `worse_candidate`), then the
              // final posed literal, then the arena reference, both final ties
              // descending.
              if (a.first_count != b.first_count)
                return a.first_count > b.first_count;
              const uint32_t n = std::min(a.key_len, b.key_len);
              for (uint32_t i = 1; i < n; i++) {
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

// Discard the probing trail and close the scope (Kissat does so for unit/binary
// rewrites; large rewrites keep the prefix). Runs before rewrite() touches the
// arena, since the rollback log holds Refs.
static void reset_to_root(Kernel &k) { probe_unwind(k); }

// Kissat's `vivify_watch_clause` poses the watches under the retained probing
// assignment; pose before kernel_replace_clause installs them.
void vivify_pose_replacement_watches(Kernel &k, std::vector<Lit> &lits) {
  const auto pose = [&](uint32_t start) {
    uint32_t best_pos = start;
    const int8_t first_value = k.value(lits[start]);
    uint32_t best_level = k.var_level(var_of(lits[start]));
    // Kissat ties the loop condition to the original first literal.
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

// Replace `c` by `lits`, add before delete, keeping redundancy and the
// post-shrink glue (kernel_shrunken_glue). The rewrite is implied by F ∪ R.
static ApplyResult rewrite(Kernel &k, Ref cref, const std::vector<Lit> &lits,
                           bool instantiated = false) {
  Clause *c = clause_at(k.arena, cref);
  const bool redundant = c->redundant();
  const uint32_t glue = kernel_shrunken_glue(
      k, c->glue(), static_cast<uint32_t>(lits.size()));
  const uint64_t id = ++k.next_clause_id;
  emit_rup_lemma(k, id, lits);
  k.stats.vivified++;
  // Kissat overwrites a large candidate in its arena allocation; this is
  // search-visible, so shrink in place.
  if (lits.size() > 2) {
    // vivify_learn_large rearms every old irredundant literal; instantiation
    // marks only the removed literal.
    if (!redundant && !instantiated)
      for (const Lit old : *c)
        if (std::find(lits.begin(), lits.end(), old) != lits.end())
          kernel_mark_bve_retry(k, var_of(old), "vivify-learn-large", c);
    // Kissat calls mark_added_literals over the old size before installing the
    // new size, so the stale tail is re-published too.
    std::vector<Lit> stale_added_tail;
    if (!redundant)
      for (uint32_t i = static_cast<uint32_t>(lits.size()); i < c->size; ++i)
        stale_added_tail.push_back(c->lits[i]);
    kernel_replace_large_clause_in_place(
        k, cref, id, lits.data(), static_cast<uint32_t>(lits.size()),
        redundant ? glue : 0, "vivify");
    for (const Lit lit : stale_added_tail) {
      kernel_touch_var(k, var_of(lit));
      kernel_mark_subsume_material(k, var_of(lit));
      kernel_mark_factor_literal(k, lit, "vivify_stale_tail");
    }
    c = clause_at(k.arena, cref);
    vivify_reestablish_watch_invariant(k, c->lits, c->size);
    return ApplyResult::Ok;
  }
  // A binary terminal retires the whole original record in Kissat, so retained
  // free literals get removal advice too.
  if (!redundant && lits.size() == 2)
    for (const Lit old : *c)
      if (!k.value(old) &&
          std::find(lits.begin(), lits.end(), old) != lits.end())
        kernel_mark_bve_retry(k, var_of(old), "vivify-learn-binary", c);
  // shared replacement tail; glue is a redundant-clause concept, 0 otherwise
  switch (kernel_replace_clause(k, c, id, lits.data(),
                                static_cast<uint32_t>(lits.size()), redundant,
                                redundant ? glue : 0, nullptr)) {
  case ReplaceOutcome::Refuted:
    return ApplyResult::Unsat;
  case ReplaceOutcome::Unit:
    // Kissat continues the schedule after a unit; later candidates recheck root
    // values and refs stay valid until the round-end collection.
    k.unit_ids[var_of(lits[0])] = id;
    kernel_assign_unit(k, lits[0]);
    if (propagate_service(k) == ApplyResult::Unsat)
      return ApplyResult::Unsat;
    return ApplyResult::Ok;
  case ReplaceOutcome::Binary:
    // Kissat's binary replacement via `new_clause` is a defragmentation
    // checkpoint.
    kernel_defrag_vectors(k);
    return ApplyResult::Ok;
  case ReplaceOutcome::Large:
    assert(false);
    return ApplyResult::Unsat;
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

// Kissat recomputes the glue of every redundant large reason Vivify analysis
// traverses and promotes it at once (without `used`).
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

// An irredundant candidate subsumed by a redundant reason promotes that reason
// (kissat vivify.c:1145-1163).
static void promote_irredundant(Kernel &k, Ref ref) {
  Clause *c = clause_at(k.arena, ref);
  assert(!c->garbage() && c->redundant());
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_clause_transition(k, "promote", c->lits, c->size,
                                      c->id());
#endif
  kernel_promote_large_irredundant(k, ref);
  c = clause_at(k.arena, ref);
  for (const Lit l : *c) {
    // Pinned Kissat promotes this clause through `mark_added_literals`, which
    // arms forward subsumption and Factor but deliberately not Eliminate.
    kernel_touch_var(k, var_of(l));
    kernel_mark_subsume_material(k, var_of(l));
    kernel_mark_factor_literal(k, l, "vivify_promote");
  }
  k.stats.vivify_promoted++;
}

// Analyze under the candidate assumptions: `analyzed` marks false literals by
// variable. The learned clause negates the decision frontier plus an optional
// implied literal; contained reasons are subsumers, redundant reasons are kept
// for the asymmetric-tautology guard.
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
      // Walk the semantic binary reason, as Kissat (vivify.c:525-542);
      // expanding jumped_over here could hide a contained binary reason.
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
      journal_reason("large(id=" + std::to_string(reason->id()) +
                     ";lits=" + vivify_journal_literals(k, *reason) + ')');
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

// Probe one candidate and run the terminal dispatch. With `reuse` the round
// owns the scope and trail; otherwise this opens and closes its own. Either
// way reset_to_root() precedes any mutation (design §3).
static ApplyResult vivify_one(Kernel &k, Ref cref, bool reuse) {
  Clause *c = clause_at(k.arena, cref);
  if (!is_candidate(k, c)) {
    // Delete a root-satisfied candidate (kissat/CaDiCaL); root, not current
    // value.
    if (!c->garbage() && c->size >= 3) {
      bool satisfied = false;
      for (const Lit l : *c)
        satisfied |= kernel_root_value(k, l) > 0;
      if (satisfied && in_scope(k, c)) {
        // Kissat marks a root-satisfied candidate garbage and keeps the prefix.
        kernel_delete_clause(k, c);
        k.stats.vivify_deleted++;
        return ApplyResult::Ok;
      }
    }
    return ApplyResult::NoOp;
  }
  k.stats.vivify_checked++;
  if (c->redundant() && c->glue() > k.tier2)
    k.stats.vivify_tier3++; // 0 here means the tier3 band is inert
  // Probe literals in the round's global order (design §2.2).
  std::vector<Lit> probes;
  probes.reserve(c->size);
  probe_list(k, c, vivify_order(), probes, reuse);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  if (vivify_journal_active)
    vivify_journal_facts.probes = vivify_journal_literals(k, probes);
#endif
  if (probes.size() <= 2)
    return ApplyResult::NoOp; // effectively binary: C2 territory
  if (reuse && k.level > 0) {
    // (a) The candidate may be the reason of a retained literal (kissat
    // vivify.c:995-1005).
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
    // (b) Trim assumptions that are not literals of this candidate (VP3, §3.4).
    if (k.level > probes.size())
      kernel_backtrack(k, static_cast<uint32_t>(probes.size()),
                       /*update_phases=*/false);
  }
  // Persistent deduction scratch, restored in O(touched). Probe-literal marks
  // (kissat vivify_mark_sorted_literals) find an implied literal anywhere.
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
  // Speculative churn is handed back when the scope closes; in reuse mode the
  // round owns the scope.
  if (!reuse)
    kernel_probing_begin(k, /*rollback_pose=*/false);
  uint32_t reused = 0, made = 0;
  uint32_t assumed = 0;
  Lit implied = INVALID_LIT;
  for (uint32_t i = 0; i < probes.size(); i++) {
    const Lit li = probes[i];
    const uint32_t lvl = assumed + 1;
    if (lvl <= k.level) {
      // Reuse test on the frame decision, not on the value (VP3).
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
    kernel_assign_decision(k, negate(li));
    assumed++;
    made++;
    if (kernel_propagate_probing(k, cref)) {
      break;
    }
    // Eager implied literal (kissat vivify.c:1078-1090) gives (probes[0..i],
    // implied); `better_implied` is completed after the loop
    // (vivify.c:1101-1117). Start at before + 1: trail[before] is the
    // assumption.
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
  if (vivify_journal_active && conflict)
    vivify_journal_facts.conflict_clause =
        k.conflict_ref == INVALID_REF
            ? vivify_journal_literals(k, k.conflict_bin)
            : vivify_journal_literals(k, *clause_at(k.arena, k.conflict_ref));
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
    vivify_journal_terminal("subsumed");
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
      vivify_journal_terminal("shrunken");
    // Units and binaries go to the root; a large learned clause keeps the
    // prefix, trimmed to new_size-2 without an implied literal before choosing
    // watches (vivify.c: vivify_learn_large).
    std::vector<Lit> replacement = d.learned;
    if (replacement.size() <= 2) {
      reset_to_root(k);
    } else {
      vivify_trim_replacement_prefix(
          k, implied, static_cast<uint32_t>(replacement.size()));
      vivify_pose_replacement_watches(k, replacement);
      vivify_journal_replacement(k, replacement);
    }
    res = rewrite(k, cref, replacement);
    k.stats.vivify_shrunken++;
    } else if (implied != INVALID_LIT && c->redundant()) {
      vivify_journal_terminal("implied-delete");
    kernel_delete_clause(k, clause_at(k.arena, cref));
    k.stats.vivify_implied++;
    res = ApplyResult::Ok;
    } else if ((conflict || implied != INVALID_LIT) && !c->redundant() &&
             !d.used_redundant) {
      vivify_journal_terminal("asymmetric-delete");
    kernel_delete_clause(k, clause_at(k.arena, cref));
    k.stats.vivify_asymmetric++;
    res = ApplyResult::Ok;
    } else if (implied != INVALID_LIT) {
      vivify_journal_terminal("implied-decline");
    k.stats.vivify_noop++;
    } else if (implied == INVALID_LIT && assumed == probes.size()) {
    // No deduction: reverse the last assumption (kissat vivify.c:1180-1200); a
    // conflict proves the shortened clause by RUP.
    const Lit remove = probes.back();
    kernel_backtrack(k, k.level - 1, /*update_phases=*/false);
    // Kissat's conflict is a local return value.  The first deduction has
    // consumed it; the reverse-last-literal transaction starts clean.
    k.conflict_valid = false;
    k.conflict_ref = INVALID_REF;
    kernel_assign_decision(k, remove);
    made++;
    if (kernel_propagate_probing(k, cref)) {
      vivify_journal_terminal("instantiated");
      // Kissat strengthens a large instantiation in the current physical order,
      // then backtracks two levels before choosing watches
      // (vivify.c:large_strengthen_after_instantiation); reconstruct that
      // result.
      std::vector<Lit> shorter;
      c = clause_at(k.arena, cref);
      shorter.reserve(c->size - 1);
      for (const Lit l : *c)
        if (l != remove && kernel_root_value(k, l) >= 0)
          shorter.push_back(l);
      // Kissat vivify_strengthen_after_instantiation: a binary result returns
      // to the root, a large one keeps level-2 and re-poses the watches. Branch
      // on the root-filtered `shorter`, not the level.
      if (shorter.size() > 2) {
        const uint32_t retained = k.level > 1 ? k.level - 2 : 0;
        kernel_backtrack(k, retained, /*update_phases=*/false);
        vivify_pose_replacement_watches(k, shorter);
      } else {
        reset_to_root(k);
      }
      vivify_journal_replacement(k, shorter);
      res = rewrite(k, cref, shorter, true);
      k.stats.vivify_instantiated++;
    } else {
      vivify_journal_terminal("instantiation-decline");
      const uint32_t keep = k.level > 1 ? k.level - 2 : 0;
      kernel_backtrack(k, keep, /*update_phases=*/false);
      k.stats.vivify_noop++;
    }
    } else {
      vivify_journal_terminal("no-deduction");
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
    // If the terminal path kept the whole conflicting stack, drop its last
    // frame.
    if (conflict && k.level == level_after_assumptions && k.level)
      kernel_backtrack(k, k.level - 1, /*update_phases=*/false);
    c = clause_at(k.arena, cref);
    if (!c->garbage())
      vivify_reestablish_watch_invariant(k, c->lits, c->size);
    k.conflict_valid = false;
    k.conflict_ref = INVALID_REF;
  }
  return res;
}

// canonical instance: one full round over the tier-aware schedule
ApplyResult vivify_apply(Kernel &k) {
  assert(vivify_guard(k));
  // The caller poses Kissat's cumulative ceiling; scheduling and rewatching use
  // the same allowance.
  const uint64_t tick_start = k.stats.ticks;
  // D-LEVEL guard purification: complete the root closure U (no-op at a
  // fixpoint); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.vivify_rounds++;
  // Do not inherit a recording scope from an earlier rule.
  kernel_probing_end(k);
  // Kissat enters Vivify through `kissat_flush_large_watches`: large watches
  // disconnect and duplicate binaries retire before scheduling.
  if (k.large_watches_connected)
    kernel_flush_large_watches(k);
  std::vector<Ref> candidates;
  VivifySchedule sched;
  vivify_schedule(k, candidates, &sched);
  if (sched.refuted)
    return ApplyResult::Unsat;
  // Reconnect after committing the literal order; the watch pose decides
  // reasons.
  event_trace_slot_boundary(k, "vivify-dense-resume", "entry");
  // Reproduce Kissat's arena-order rewatch from source-position keys.
  kernel_rebuild_large_watches(k);
  event_trace_slot_boundary(k, "vivify-dense-resume", "resume");
  if (sched.root_unit) {
    if (inprocess_reach_root(k))
      return ApplyResult::Unsat;
    return ApplyResult::Ok;
  }
  k.stats.vivify_prioritized += sched.prioritized;
  k.stats.vivify_reprioritized += sched.rearmed;
  k.stats.vivify_flushed += sched.flushed;
  uint64_t changed = sched.flushed + sched.root_changed;
  const bool reuse = k.vivify_reuse;
  // k.work_budget is the cumulative Vivify ceiling in propagation ticks.
  // Zero is unbounded only when work_budget_bounded is false.
  uint64_t ordinal = 0;
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
    // Poll termination on every candidate (kissat vivify.c:1291).
    k.stats.vivify_polls++;
    if (kernel_terminating(k)) {
      k.stats.vivify_terminated++;
      break;
    }
    // Level-zero facts left by a closed scope reach closure before the next
    // one.
    if ((!reuse || !k.probing.recording) && inprocess_reach_root(k))
      return ApplyResult::Unsat;
    // Reopen only a closed scope; test `recording`, not `level == 0`.
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
    if (journal) {
      vivify_journal_facts = VivifyJournalFacts{};
      vivify_journal_active = true;
      journal_pre = vivify_journal_snapshot(k, journal_clause);
      journal_digest = rule_journal_digest(k);
      journal_formula_epoch = k.arena_generation;
      journal_root_epoch = k.touch_fixed_head;
    }
#endif
    const ApplyResult r = vivify_one(k, cref, reuse);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_vivify_candidate(
        k, "vivify_candidate_end", ordinal, cref,
        k.work_budget_offset + (k.stats.ticks - tick_start), k.work_budget);
#endif
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    if (journal) {
      vivify_journal_emit_candidate(
          k, journal_candidate, journal_pre, journal_digest,
          journal_formula_epoch, journal_root_epoch, r);
      vivify_journal_active = false;
    }
#endif
    ordinal++;
    // Clear after trying, so a cut leaves the untried tail armed (kissat
    // vivify.c:1311).
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

ApplyResult vivify_try(Kernel &k, Ref candidate) {
  assert(vivify_guard(k));
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  // Standalone: its own root-to-root probing scope, no retained trail — the N1
  // surface must not depend on a round having run first.
  return vivify_one(k, candidate, /*reuse=*/false);
}

// Semantic surface: every live large clause is an instance; root-fixed literals
// and the band are handled at application.
static uint32_t vivify_semantic_instances(Kernel &k, uint64_t *out,
                                          uint32_t cap) {
  uint32_t count = 0;
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (count == cap)
      break;
    if (!c->garbage() && c->size >= 3)
      out[count++] = ref;
  }
  return count;
}

static ApplyResult vivify_try_semantic(Kernel &k, Ref candidate) {
  const Clause *c = clause_at(k.arena, candidate);
  if (c->garbage() || c->size < 3)
    return ApplyResult::NoOp;

  // Scope is a policy scheduling choice.  Pose it as unscoped for this one
  // explicit application, then restore the caller's online configuration.
  const uint8_t saved_scope = k.vivify_scope;
  k.vivify_scope = 0;
  const ApplyResult result = vivify_try(k, candidate);
  k.vivify_scope = saved_scope;
  return result;
}

TACTIX_REGISTER_RULE(Vivify, RuleTier::Inprocess, vivify_guard,
                     vivify_apply, arena_list_instances<vivify_candidates>,
                     (apply_root_arena_instance<vivify_apply, vivify_try>),
                     vivify_semantic_instances,
                     (apply_root_arena_instance<vivify_apply,
                                                vivify_try_semantic>));

} // namespace tactix
