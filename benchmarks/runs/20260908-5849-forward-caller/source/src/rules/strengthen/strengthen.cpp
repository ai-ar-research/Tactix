// See strengthen.hpp for the rule's contract.
#include "rules/strengthen/strengthen.hpp"

#include "diagnostics/event_trace.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/bve/bve.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace tactix {

#ifdef TACTIX_ENABLE_EVENT_TRACE
static uint64_t selected_strengthen_clause_id() {
  const char *value =
      std::getenv("TACTIX_EVENT_TRACE_STRENGTHEN_CLAUSE_ID");
  return value ? std::strtoull(value, nullptr, 10) : 0;
}

static bool selected_strengthen_clause(const Clause *clause) {
  const uint64_t selected = selected_strengthen_clause_id();
  return selected && clause->id() == selected;
}

static bool selected_strengthen_pair(int *first, int *second) {
  const char *value = std::getenv("TACTIX_EVENT_TRACE_STRENGTHEN_PAIR");
  return value && std::sscanf(value, "%d,%d", first, second) == 2;
}

static int selected_strengthen_external() {
  const char *value =
      std::getenv("TACTIX_EVENT_TRACE_STRENGTHEN_EXTERNAL");
  return value ? std::abs(std::atoi(value)) : 0;
}

static bool selected_strengthen_external_in(const Kernel &k,
                                            const Clause *clause) {
  const int selected = selected_strengthen_external();
  if (!selected)
    return false;
  for (const Lit lit : *clause)
    if (std::abs(kernel_externalize(k, lit)) == selected)
      return true;
  return false;
}

static bool selected_forward_clause(const Kernel &k, const Clause *clause) {
  const char *spec = std::getenv("TACTIX_EVENT_TRACE_FORWARD_CLAUSE");
  if (!spec || !*spec)
    return false;
  std::vector<int> selected;
  while (*spec) {
    char *end = nullptr;
    const long value = std::strtol(spec, &end, 10);
    if (end == spec)
      return false;
    selected.push_back(static_cast<int>(value));
    spec = end;
    if (*spec == ',')
      ++spec;
    else if (*spec)
      return false;
  }
  if (selected.size() != clause->size)
    return false;
  std::vector<int> actual;
  actual.reserve(clause->size);
  for (const Lit lit : *clause)
    actual.push_back(kernel_externalize(k, lit));
  std::sort(selected.begin(), selected.end());
  std::sort(actual.begin(), actual.end());
  return selected == actual;
}
#endif

RuleEligibility strengthen_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct: a pure F-rewrite is sound
  // regardless of a recorded conflict (D-SPLIT-3, `docs/calculus.md` §7).
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0);
}

// Victim criteria, shared with C1 (duplicated deliberately — modules stay
// self-contained per §5.3). Redundancy is not one of them: deleting a redundant
// clause is S2 Forget, and strengthening one emits an add-RUP that stands
// against the whole formula at emit time.
static bool is_victim(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->size > STRENGTHEN_CLSLIM)
    return false;
  for (const Lit l : *c)
    if (k.value(l) > 0)
      return false; // root-satisfied
  return true;
}

// Does the canonical round work on `c`? Irredundant always; a redundant clause
// only inside the tier band, and only with the band on
// (`Kernel::touch_redundant`). cadical's scope test (subsume.cpp:388 filters on
// `likely_to_be_kept_clause`), evaluated LIVE, so band membership is read now
// rather than at creation. Ledger row D-C1C2-VICTIM.
static bool in_round_scope(const Kernel &k, const Clause *c) {
  if (!is_victim(k, c))
    return false;
  return !c->redundant() ||
         (k.touch_redundant &&
          kernel_likely_kept(k, true, c->glue(), c->size));
}

static bool touched_on_two_variables(const Kernel &k, const Clause *c,
                                     uint64_t watermark) {
  uint32_t touched = 0;
  for (const Lit lit : *c)
    if ((kernel_touched_since(k, var_of(lit), watermark) ||
         (c->redundant() &&
          kernel_kept_since(k, var_of(lit), watermark))) &&
        ++touched == 2)
      return true;
  return false;
}

void strengthen_candidates(const Kernel &k, std::vector<Ref> &out,
                           bool complete) {
  out.clear();
  struct Cand {
    Ref ref;
    uint64_t position;
    uint32_t size;
    bool redundant;
  };
  std::vector<Cand> cands;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (complete ? is_victim(k, c) : in_round_scope(k, c))
      cands.push_back(
          {ref, kernel_clause_position(k, ref), c->size, c->redundant()});
    ref += clause_actual_words(c);
  }
  // Irredundant first, then size ascending. The band is the larger population
  // by orders of magnitude and its clauses are small, so a plain size-ascending
  // order would let them crowd the round's budget and starve the irredundant
  // work; ordering the band behind makes it a strict addition at the tail.
  std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
    if (a.redundant != b.redundant)
      return !a.redundant;
    if (a.size != b.size)
      return a.size < b.size;
    // Kissat's stable size radix retains physical arena order. Packed Tactix
    // replacements live at a fresh Ref but carry the source record's current
    // logical position until collection materializes it. Use that position,
    // not the append-only host Ref, for the equal-size tie.
    if (a.position != b.position)
      return a.position < b.position;
    return a.ref < b.ref; // defensive total order for aliased positions
  });
  out.reserve(cands.size());
  for (const Cand &c : cands)
    out.push_back(c.ref);
}

// outcome of checking one partner clause against the marked candidate
struct CheckResult {
  enum { NONE, SUBSUMED, STRENGTHEN } kind = NONE;
  Lit flipped = INVALID_LIT; // partner literal in opposite phase; the
                             // candidate loses negate(flipped)
};

// cadical subsume.cpp subsume_check, transliterated: every partner literal must
// be marked in the same phase, root-falsified (absorbed by units), or — at most
// once — marked in the opposite phase, which is the strengthen case.
static CheckResult check_marked(const Kernel &k,
                                const std::vector<uint8_t> &marks,
                                const Clause *d, bool *satisfied = nullptr) {
  CheckResult r;
  for (const Lit l : *d) {
    if (marks[l])
      continue;
    const int8_t v = k.value(l);
    if (v > 0) {
      if (satisfied)
        *satisfied = true;
      return {}; // satisfied partner proves nothing
    }
    if (v < 0)
      continue; // absorbed
    if (marks[negate(l)]) {
      if (r.flipped != INVALID_LIT)
        return {}; // two flips: plain resolution, not self-subsuming
      r.flipped = l;
      continue;
    }
    return {}; // free literal outside the candidate
  }
  r.kind = r.flipped == INVALID_LIT ? CheckResult::SUBSUMED
                                    : CheckResult::STRENGTHEN;
  return r;
}

static void mark_candidate(const Kernel &k, std::vector<uint8_t> &marks,
                           const Clause *c, uint8_t on) {
  for (const Lit l : *c)
    if (k.value(l) == 0)
      marks[l] = on;
}

static bool watch_population_within(const Kernel &k, Lit lit, bool binary,
                                    uint32_t limit) {
  if (binary)
    return k.watches.binary_size(lit) <= limit;
  uint32_t count = 0;
  // Large entries still require the complete entry-aware walk: unlike the
  // exact binary population, lazy garbage remains part of this physical cap.
  for (const WatchEntry w : k.watches.entries(lit))
    if (!w.binary() && ++count > limit)
      return false;
  return true;
}

// partner found by a scan: NONE / SUBSUMED / STRENGTHEN with the flipped
// literal plus what the LRAT chain needs (partner id + its absorbed units)
struct Partner {
  CheckResult res;
  uint64_t id = 0;
  std::vector<uint64_t> absorbed; // unit ids of root-falsified partner lits
#ifdef TACTIX_ENABLE_EVENT_TRACE
  Ref trace_ref = INVALID_REF;
  Lit trace_a = INVALID_LIT, trace_b = INVALID_LIT;
#endif
};

// Kissat's forward-subsumption index is one mixed watch stream per literal.
// Dense-mode entry first flushes every large reference, leaving the binary
// prefix.  Survivors are then connected in candidate order and binaries made
// by an earlier strengthening are appended at that exact point.  Keeping this
// local image avoids hoisting such late binaries ahead of already-connected
// large partners, which changes the last winning strengthener.
struct ForwardWatch {
  uint32_t payload = INVALID_REF;
  bool binary = false;

  static ForwardWatch binary_entry(Lit other) {
    ForwardWatch result;
    result.payload = other;
    result.binary = true;
    return result;
  }

  static ForwardWatch large_entry(Ref ref) {
    ForwardWatch result;
    result.payload = ref;
    return result;
  }

  Lit other() const {
    assert(binary);
    return payload;
  }

  Ref ref() const {
    assert(!binary);
    return payload;
  }
};
static_assert(sizeof(ForwardWatch) == 8);

#ifdef TACTIX_ENABLE_EVENT_TRACE
static void trace_forward_result(const Kernel &k, const Clause *candidate,
                                 const Partner &partner, size_t index,
                                 const std::vector<std::vector<ForwardWatch>>
                                     &watches) {
  const char *path = std::getenv("TACTIX_FORWARD_CLAUSE_TRACE");
  const char *conflict =
      std::getenv("SAT_EVENT_TRACE_FORWARD_CLAUSE_CONFLICT");
  if (!path || !*path || !conflict || !*conflict ||
      std::strtoull(conflict, nullptr, 10) != k.stats.conflicts ||
      !selected_forward_clause(k, candidate))
    return;
  static std::FILE *out = nullptr;
  if (!out) {
    out = std::fopen(path, "w");
    if (!out)
      std::exit(2);
    std::fputs("# tactix-forward-clause\t1\n", out);
  }
  int scan_external = 0, other_external = 0;
  if (selected_strengthen_pair(&scan_external, &other_external)) {
    Lit scan = INVALID_LIT, other = INVALID_LIT;
    for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
      const int external = kernel_externalize(k, lit);
      if (external == scan_external)
        scan = lit;
      if (external == other_external)
        other = lit;
    }
    if (scan != INVALID_LIT && other != INVALID_LIT) {
      uint32_t stream_matches = 0;
      for (const ForwardWatch &watch : watches[scan])
        stream_matches += watch.binary && watch.other() == other;
      std::fprintf(out,
                   "pair\tscan=%d\tother=%d\tkernel_copies=%u\t"
                   "kernel_mirror_copies=%u\tstream_size=%zu\t"
                   "stream_matches=%u\n",
                   scan_external, other_external,
                   kernel_binary_copies(k, scan, other),
                   kernel_binary_copies(k, other, scan), watches[scan].size(),
                   stream_matches);
    }
  }
  std::fprintf(out,
               "result\tindex=%zu\tid=%llu\taction=%d\tflipped=%d\t"
               "partner_id=%llu\tpartner=",
               index, (unsigned long long)candidate->id(), partner.res.kind,
               partner.res.flipped == INVALID_LIT
                   ? 0
                   : kernel_externalize(k, partner.res.flipped),
               (unsigned long long)partner.id);
  if (partner.trace_ref != INVALID_REF) {
    const Clause *source = clause_at(k.arena, partner.trace_ref);
    std::fputs("large:", out);
    for (const Lit lit : *source)
      std::fprintf(out, "%d,", kernel_externalize(k, lit));
  } else if (partner.trace_a != INVALID_LIT) {
    std::fprintf(out, "binary:%d,%d", kernel_externalize(k, partner.trace_a),
                 kernel_externalize(k, partner.trace_b));
  } else {
    std::fputc('-', out);
  }
  std::fputc('\n', out);
  std::fclose(out);
  out = nullptr;
  if (std::getenv("TACTIX_FORWARD_CLAUSE_TRACE_STOP"))
    std::exit(0);
}
#endif

// Kissat's forward pass charges one step per candidate, then for every
// non-empty, non-saturated watch stream charges one plus the number of assumed
// 128-byte cache lines occupied by its 4-byte watches, and one more per large
// watch visited.  It publishes that local scalar when the scan returns.  Keep
// the same representation and commit it through this scope guard on every
// exit.  The physical ForwardWatch is wider, but the reference currency is
// deliberately based on Kissat's packed 4-byte watch: ceil(size / 32).
struct ForwardStepCounter {
  uint64_t &published;
  uint64_t value;
  explicit ForwardStepCounter(uint64_t &counter)
      : published(counter), value(counter) {}
  ~ForwardStepCounter() { published = value; }
};

static uint64_t forward_cache_lines(size_t watches) {
  return (watches + 31u) / 32u;
}

// Root facts must survive deletion of a connected reason. Native dense
// assignment emits the unit immediately and retains UNIT_REASON. Reuse the
// established LRAT chain builder, before any later satisfied-clause cleanup.
static void forward_assign_root(Kernel &k, Lit unit, uint32_t reason,
                                bool binary) {
  const size_t from = k.trail.size();
  kernel_assign(k, unit, 0, reason, binary);
  if (k.proof) {
    if (k.proof->lrat())
      kernel_emit_root_units(k, from);
    else
      k.proof->add_rup(0, &unit, 1, nullptr, 0);
  }
  Assigned &assigned = k.assigned[var_of(unit)];
  assigned.reason = UNIT_REASON;
  assigned.binary = false;
  assigned.jumped = false;
}

static void forward_refute(Kernel &k, const Clause *clause,
                           Lit first = INVALID_LIT, Lit second = INVALID_LIT) {
  if (k.proof) {
    if (k.proof->lrat()) {
      k.lrat_entries.clear();
      const auto add = [&](Lit lit) {
        assert(k.value(lit) < 0);
        const Var v = var_of(lit);
        k.lrat_entries.emplace_back(k.assigned[v].trail, k.unit_ids[v]);
      };
      if (clause)
        for (const Lit lit : *clause)
          add(lit);
      else {
        add(first);
        add(second);
      }
      kernel_lrat_chain(k, clause ? clause->id()
                                 : kernel_binary_id(k, first, second));
      k.proof->add_empty(++k.next_clause_id, k.lrat_chain.data(),
                         static_cast<uint32_t>(k.lrat_chain.size()));
    } else
      k.proof->add_empty(0, nullptr, 0);
  }
  k.inconsistent = true;
  k.conflict_valid = false;
}

// The same dense assignment/proof semantics apply to the private partial
// forward image and to the caller's complete irredundant image. The caller
// chooses the image; do not replace the partial scan during candidate work.
template <class Streams>
static bool forward_propagate_units(Kernel &k, const Streams &watches,
                                    uint64_t &changed) {
  while (k.propagate_head < k.trail.size()) {
    const Lit false_lit = negate(k.trail[k.propagate_head++]);
    k.stats.propagations++;
    const auto &stream = watches[false_lit];
    uint64_t ticks = 1 + forward_cache_lines(stream.size());
    for (const auto watch : stream) {
      if (watch.binary) {
        const Lit other = watch.payload;
        if (k.value(other) > 0)
          continue;
        if (k.value(other) < 0) {
          forward_refute(k, nullptr, false_lit, other);
          return false; // native does not charge the conflicting scan
        }
        if (!kernel_var_retired(k, var_of(other)))
          forward_assign_root(k, other, false_lit, true);
        continue;
      }
      ++ticks;
      Clause *clause = clause_at(k.arena, watch.payload);
      if (clause->garbage())
        continue;
      Lit unit = INVALID_LIT;
      unsigned non_false = 0;
      bool satisfied = false;
      for (const Lit lit : *clause) {
        if (lit == false_lit || k.value(lit) < 0)
          continue;
        if (k.value(lit) > 0) {
          satisfied = true;
          kernel_delete_clause(k, clause);
          ++changed;
          break;
        }
        unit = lit;
        if (++non_false == 2)
          break;
      }
      if (satisfied)
        continue;
      if (!non_false) {
        forward_refute(k, clause);
        return false;
      }
      if (non_false == 1)
        forward_assign_root(k, unit, watch.payload, false);
    }
    const bool saved_inprocessing = k.inprocessing;
    k.inprocessing = true; // dense ticks never increase search_ticks
    kernel_charge_ticks(k, ticks);
    k.inprocessing = saved_inprocessing;
  }
  return true;
}

// Compact the private forward image only after closing its reachable units.
// Sparse public callers retain full replay debt. Eliminate's caller separately
// reconnects all irredundant clauses before replaying the forward suffix.
static bool forward_flush_units(
    Kernel &k, std::vector<std::vector<ForwardWatch>> &watches,
    uint64_t &changed) {
  const uint32_t from = k.propagate_head;
  if (from == k.trail.size())
    return true;
  if (k.large_watches_connected)
    k.binary_only_head = std::min(k.binary_only_head, from);
  if (!forward_propagate_units(k, watches, changed))
    return false;
  for (uint32_t i = from; i < k.propagate_head; ++i) {
    auto &stream = watches[k.trail[i]];
    size_t kept = 0;
    for (const ForwardWatch watch : stream) {
      if (watch.binary) {
        stream[kept++] = watch;
        // Normally no BVE heap exists during the forward pass. Preserve the
        // shared removal notification if a public caller has installed one.
        if (k.value(watch.other()) == 0)
          kernel_notify_bve_score(k, var_of(watch.other()), true);
        continue;
      }
      Clause *clause = clause_at(k.arena, watch.ref());
      if (!clause->garbage()) {
        kernel_delete_clause(k, clause);
        ++changed;
      }
    }
    stream.resize(kept);
  }
  return true;
}

ApplyResult strengthen_reconnect_and_flush(Kernel &k, uint32_t trail_from) {
  assert(k.level == 0 && k.dense && k.dense->track_order);
  assert(!k.large_watches_connected && trail_from <= k.trail.size());
  if (k.inconsistent)
    return ApplyResult::Unsat;
  // eliminate.c rebuilds full connections after every forward call, even a
  // quota cut. Reuse the existing arena-order constructor, without allocating
  // another formula-wide copy of the mixed occurrence image.
  kernel_rebuild_dense_occurrences(k);
  k.propagate_head = trail_from;
  const auto propagate = [&] {
    uint64_t changed = 0;
    if (!forward_propagate_units(k, k.dense->ordered, changed))
      return ApplyResult::Unsat;
    bve_flush_units_while_connected(k, trail_from);
    return ApplyResult::Ok;
  };
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  // Rearm before journal eligibility is evaluated: the partial forward scan
  // may have consumed the old cursor without covering this complete image.
  return journal_direct_rule(k, "Propagate", "post-forward-connected",
                             propagate, ApplyResult::Ok);
#else
  return propagate();
#endif
}

static Partner find_forward_partner(
    Kernel &k, const std::vector<uint8_t> &marks, const Clause *c,
    std::vector<std::vector<ForwardWatch>> &watches, uint64_t &steps,
    uint64_t &changed) {
  Partner p;
  const bool lrat = k.proof && k.proof->lrat();
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    for (int phase = 0; phase < 2; ++phase) {
      const Lit probe = phase ? negate(l) : l;
      std::vector<ForwardWatch> &stream = watches[probe];
#ifdef TACTIX_ENABLE_EVENT_TRACE
      const bool selected = selected_strengthen_clause(c);
      if (selected) {
        event_trace_detail(k, "strengthen_selected_scan",
                           kernel_externalize(k, probe));
        event_trace_detail(k, "strengthen_selected_scan_phase", phase);
        event_trace_detail(k, "strengthen_selected_stream_size",
                           stream.size());
      }
#endif
      if (stream.size() > STRENGTHEN_OCCLIM)
        continue;
      if (stream.empty())
        continue;
      steps += 1 + forward_cache_lines(stream.size());
      size_t read = 0, write = 0;
      bool stop_stream = false;
      while (read < stream.size()) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
        const size_t wi = read;
#endif
        const ForwardWatch watch = stream[read++];
        stream[write++] = watch;
#ifdef TACTIX_ENABLE_EVENT_TRACE
        if (selected) {
          event_trace_detail(k, "strengthen_selected_entry", wi);
          event_trace_detail(k, "strengthen_selected_entry_binary",
                             watch.binary);
          if (watch.binary) {
            event_trace_detail(k, "strengthen_selected_entry_other",
                               kernel_externalize(k, watch.other()));
          } else {
            const Clause *entry = clause_at(k.arena, watch.ref());
            event_trace_detail(k, "strengthen_selected_entry_ref",
                               watch.ref());
            event_trace_detail(k, "strengthen_selected_entry_garbage",
                               entry->garbage());
            if (!entry->garbage()) {
              event_trace_detail(k, "strengthen_selected_entry_id",
                                 entry->id());
              event_trace_detail(k, "strengthen_selected_entry_order",
                                 kernel_clause_position(k, watch.ref()));
              event_trace_detail(k, "strengthen_selected_entry_size",
                                 entry->size);
              for (const Lit member : *entry)
                event_trace_detail(k, "strengthen_selected_entry_lit",
                                   kernel_externalize(k, member));
            }
          }
        }
#endif
        if (watch.binary) {
          if (phase)
            continue;
          const Lit other = watch.other();
          if (marks[other]) {
            p.res.kind = CheckResult::SUBSUMED;
#ifdef TACTIX_ENABLE_EVENT_TRACE
            p.trace_ref = INVALID_REF;
            p.trace_a = probe;
            p.trace_b = other;
#endif
#ifdef TACTIX_ENABLE_EVENT_TRACE
            if (selected) {
              event_trace_detail(k, "strengthen_selected_match_kind",
                                 p.res.kind);
              event_trace_detail(k, "strengthen_selected_match_flipped", 0);
            }
#endif
            if (lrat)
              p.id = kernel_binary_id(k, probe, other);
            stop_stream = true;
            break;
          }
          if (marks[negate(other)]) {
            p.res.kind = CheckResult::STRENGTHEN;
            p.res.flipped = other;
            p.absorbed.clear();
#ifdef TACTIX_ENABLE_EVENT_TRACE
            p.trace_ref = INVALID_REF;
            p.trace_a = probe;
            p.trace_b = other;
#endif
#ifdef TACTIX_ENABLE_EVENT_TRACE
            if (selected) {
              event_trace_detail(k, "strengthen_selected_match_kind",
                                 p.res.kind);
              event_trace_detail(k, "strengthen_selected_match_flipped",
                                 kernel_externalize(k, p.res.flipped));
            }
#endif
            if (lrat)
              p.id = kernel_binary_id(k, probe, other);
            // A binary strengthener terminates this physical watch walk.
            // The opposite-phase walk for the same candidate literal still
            // follows, just as the second forward_literal call in Kissat.
            stop_stream = true;
            break;
          }
          continue;
        }
        steps++;
        const Clause *d = clause_at(k.arena, watch.ref());
        if (d->garbage()) {
          write--;
          continue;
        }
        bool satisfied = false;
        const CheckResult r = check_marked(k, marks, d, &satisfied);
        if (satisfied) {
          kernel_delete_clause(k, clause_at(k.arena, watch.ref()));
          ++changed;
          --write;
          break; // native cleans this entry and retains the unscanned tail
        }
        if (r.kind == CheckResult::NONE)
          continue;
        p.res = r;
        p.absorbed.clear();
#ifdef TACTIX_ENABLE_EVENT_TRACE
        p.trace_ref = watch.ref();
        p.trace_a = p.trace_b = INVALID_LIT;
#endif
#ifdef TACTIX_ENABLE_EVENT_TRACE
        if (selected) {
          event_trace_detail(k, "strengthen_selected_match_kind", r.kind);
          event_trace_detail(
              k, "strengthen_selected_match_flipped",
              r.flipped == INVALID_LIT ? 0
                                       : kernel_externalize(k, r.flipped));
        }
#endif
        if (lrat) {
          p.id = d->id();
          for (const Lit m : *d)
            if (k.value(m) < 0)
              p.absorbed.push_back(k.unit_ids[var_of(m)]);
        }
        if (r.kind == CheckResult::SUBSUMED) {
          stop_stream = true;
          break;
        }
        // Large strengtheners do not stop the walk.  A later large or binary
        // entry is allowed to overwrite the literal to remove.
      }
      // `forward_literal` removes garbage encountered before its result while
      // preserving the unscanned tail byte-for-byte.  This matters both to the
      // next candidate's physical population cap and to its cache-line charge.
      while (read < stream.size())
        stream[write++] = stream[read++];
      stream.resize(write);
      if (stop_stream && p.res.kind == CheckResult::SUBSUMED)
        return p;
    }
  }
  return p;
}

// Pinned Kissat forward.c begins every pass by collapsing duplicate binaries
// on variables whose persistent `subsume` flag is armed. Tactix's equivalent
// flag image is the Strengthen touch watermark. The same scan detects
// (l∨a),(l∨¬a), whose hyper-unary resolvent is l, before compacting later
// copies. Returns the number of retired copies, or UINT64_MAX on refutation.
static uint64_t sweep_forward_binaries(Kernel &k, uint64_t since,
                                       bool filtered) {
  struct UnitWitness {
    Lit unit;
    uint64_t first, second;
  };
  std::vector<UnitWitness> units;
  std::vector<std::pair<Lit, Lit>> duplicates;
  std::vector<uint8_t> marks(2 * static_cast<size_t>(k.nvars), 0);
  std::vector<uint64_t> ids(2 * static_cast<size_t>(k.nvars), 0);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
    const Var v = var_of(lit);
    if (kernel_var_retired(k, v) || k.value(lit) != 0 ||
        (filtered && !kernel_touched_since(k, v, since)))
      continue;
    std::vector<Lit> touched;
    for (const WatchEntry w : k.watches.entries(lit)) {
      if (!w.binary())
        continue;
      const Lit other = w.other();
      const Var ov = var_of(other);
      if (kernel_var_retired(k, ov) || k.value(other) != 0 ||
          (filtered && !kernel_touched_since(k, ov, since)))
        continue;
      if (marks[other]) {
        // The current list itself is the exact multiplicity authority.  Record
        // the pair while it is already in cache instead of rescanning this
        // (potentially very high-degree) list once for every binary below.
        // Only one mirror owns the candidate; sort/unique still protects the
        // result from triples and later additions in the same physical list.
        if (lit < other)
          duplicates.emplace_back(lit, other);
        continue; // a later physical duplicate; compacted below
      }
      if (marks[negate(other)])
        units.push_back(
            {lit, ids[negate(other)], kernel_binary_id(k, lit, other)});
      marks[other] = 1;
      ids[other] = kernel_binary_id(k, lit, other);
      touched.push_back(other);
    }
    for (const Lit other : touched) {
      marks[other] = 0;
      ids[other] = 0;
    }
  }
  uint64_t removed = 0;
  std::sort(duplicates.begin(), duplicates.end());
  duplicates.erase(std::unique(duplicates.begin(), duplicates.end()),
                   duplicates.end());
  for (const auto &[a, b] : duplicates)
    removed += kernel_remove_duplicate_binary_copies(k, a, b);

  for (const UnitWitness &w : units) {
    const int8_t value = k.value(w.unit);
    if (value > 0)
      continue;
    const uint64_t id = ++k.next_clause_id;
    if (k.proof) {
      const uint64_t chain[2] = {w.first, w.second};
      k.proof->add_rup(k.proof->lrat() ? id : 0, &w.unit, 1,
                       k.proof->lrat() ? chain : nullptr,
                       k.proof->lrat() ? 2 : 0);
    }
    if (value < 0) {
      kernel_emit_empty_from_pair(k, id, k.unit_ids[var_of(w.unit)]);
      return UINT64_MAX;
    }
    k.unit_ids[var_of(w.unit)] = id;
    kernel_assign_unit(k, w.unit);
    removed++;
  }
  if (!units.empty() && inprocess_reach_root(k))
    return UINT64_MAX;
  return removed;
}

// scan the candidate's watch lists (both phases) for binary partners and the
// round's occurrence lists (both phases) for large ones. occs == nullptr
// selects the watch-list-only discovery the single-candidate instance surface
// uses. Free literals only; both scans are read-only.
static Partner find_partner(Kernel &k, const std::vector<uint8_t> &marks,
                            const Clause *c,
                            const std::vector<std::vector<Ref>> *occs,
                            Ref self, uint64_t &visits, uint64_t visit_start,
                            bool bounded, uint64_t visit_budget,
                            bool *budget_exhausted) {
  Partner p;
  const bool lrat = k.proof && k.proof->lrat();
  const auto budget_cut = [&]() {
    if (!budget_exhausted || !bounded ||
        visits - visit_start < visit_budget)
      return false;
    *budget_exhausted = true;
    return true;
  };
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    for (int phase = 0; phase < 2; phase++) {
      const Lit probe = phase ? negate(l) : l;
      // Kissat scans binaries only in `forward_literal(lit, true)`.  Its
      // opposite-phase `forward_literal(NOT(lit), false)` walk considers large
      // occurrences only.  Opposite-phase binary discovery is logically
      // redundant (the same binary is reachable from its same-phase literal)
      // but changes which of several valid strengtheners wins first.
      if (phase == 0 &&
          watch_population_within(k, probe, true, STRENGTHEN_BINLIM)) {
        const bool stop_at_large = k.watches.heads[probe].binaries_first();
        bool strengthened_by_binary = false;
        for (const WatchEntry w : k.watches.entries(probe)) {
          if (!w.binary()) {
            if (stop_at_large)
              break;
            continue;
          }
          if (budget_cut())
            return p;
          visits++; // a binary partner compared
          const Lit o = w.other();
          if (phase == 0) {
            // no redundancy check on the binary partner: the binary-subsumer
            // argument in subsume_forward.hpp covers this
            if (marks[o]) { // (l ∨ o), both in candidate: subsumes
              p.res.kind = CheckResult::SUBSUMED;
              if (lrat)
                p.id = kernel_binary_id(k, probe, o);
              return p;
            }
            if (marks[negate(o)]) { // (l ∨ o), candidate has ¬o: drop ¬o
              p.res.kind = CheckResult::STRENGTHEN;
              p.res.flipped = o;
              p.absorbed.clear();
              if (lrat)
                p.id = kernel_binary_id(k, probe, o);
              // Kissat breaks this one `forward_literal` watch walk, but
              // carries `remove` into the remaining candidate literals where
              // a later strengthener may overwrite it.
              strengthened_by_binary = true;
              break;
            }
          }
        }
        if (strengthened_by_binary)
          continue;
      }
      if (!occs) // instance surface: large partners via watch lists below
        continue;
      const std::vector<Ref> &os = (*occs)[probe];
      if (os.size() > STRENGTHEN_OCCLIM)
        continue;
      for (const Ref dref : os) {
        if (budget_cut())
          return p;
        visits++;
        const Clause *d = clause_at(k.arena, dref);
        const CheckResult r = check_marked(k, marks, d);
        if (r.kind == CheckResult::NONE)
          continue;
        p.res = r;
        p.absorbed.clear();
        if (lrat) {
          p.id = d->id();
          for (const Lit m : *d)
            if (k.value(m) < 0)
              p.absorbed.push_back(k.unit_ids[var_of(m)]);
        }
        if (r.kind == CheckResult::SUBSUMED)
          return p;
        // Large strengtheners do not break `forward_literal`; the last valid
        // one in the remaining physical occurrence scans wins.
      }
    }
  }
  if (occs)
    return p;
  // instance surface: large partners live in the watch lists of the candidate's
  // own free literals — a partner watches two of its own literals, each a
  // candidate literal or its negation — so rerun the phase loop for them
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    for (int phase = 0; phase < 2; phase++) {
      const Lit probe = phase ? negate(l) : l;
      if (!watch_population_within(k, probe, false, STRENGTHEN_OCCLIM))
        continue;
      for (const WatchEntry w : k.watches.entries(probe)) {
        if (w.binary())
          continue;
        const Ref dref = w.ref();
        if (dref == self)
          continue;
        if (budget_cut())
          return p;
        visits++;
        const Clause *d = clause_at(k.arena, dref);
        if (d->garbage() || d->redundant() || d->size > c->size)
          continue;
        const CheckResult r = check_marked(k, marks, d);
        if (r.kind == CheckResult::NONE)
          continue;
        p.res = r;
        p.absorbed.clear();
        if (lrat) {
          p.id = d->id();
          for (const Lit m : *d)
            if (k.value(m) < 0)
              p.absorbed.push_back(k.unit_ids[var_of(m)]);
        }
        if (r.kind == CheckResult::SUBSUMED)
          return p;
      }
    }
  }
  return p;
}

// act() on a found partner. Returns: 0 = candidate unchanged (NONE),
// 1 = candidate died (subsumed), 2 = strengthened into *ref (rescan),
// 3 = strengthened to binary (done with candidate), 4 = unit asserted
// (the canonical caller propagates and continues), 5 = empty clause (UNSAT)
static int act(Kernel &k, Ref *ref, const Partner &p, uint64_t *changed,
               std::pair<Lit, Lit> *new_binary = nullptr) {
  if (p.res.kind == CheckResult::NONE)
    return 0;
  Clause *c = clause_at(k.arena, *ref);
  if (p.res.kind == CheckResult::SUBSUMED) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (selected_strengthen_external_in(k, c)) {
      event_trace_detail(k, "strengthen_target_action", CheckResult::SUBSUMED);
      event_trace_detail(k, "strengthen_target_clause_id", c->id());
      event_trace_detail(k, "strengthen_target_removed", 1);
    }
#endif
    // C1 shadowed this clause and C2 has now deleted it on its own.
    if (kernel_shadow_retire(k, c->id()))
      k.stats.c1_shadow_deleted++;
    kernel_delete_clause(k, c);
    k.stats.strengthen_subsumed++;
    (*changed)++;
    return 1;
  }
  // Strengthen: rebuild from free literals minus the resolved one. A shadowed
  // clause rewritten away is also handled, but that is a different disposal
  // from a deletion, so it gets its own counter.
  if (kernel_shadow_retire(k, c->id()))
    k.stats.c1_shadow_rewritten++;
  const Lit gone = negate(p.res.flipped);
  const bool lrat = k.proof && k.proof->lrat();
  std::vector<Lit> lits;
  std::vector<uint64_t> chain;
  lits.reserve(c->size);
  for (const Lit l : *c) {
    if (l == gone)
      continue;
    if (k.value(l) < 0) { // stripped; its unit absorbs it for the checker
      if (lrat)
        chain.push_back(k.unit_ids[var_of(l)]);
      continue;
    }
    lits.push_back(l);
  }
#ifdef TACTIX_ENABLE_EVENT_TRACE
  if (selected_strengthen_external_in(k, c)) {
    const int selected = selected_strengthen_external();
    bool retained = false;
    for (const Lit lit : lits)
      retained |= std::abs(kernel_externalize(k, lit)) == selected;
    event_trace_detail(k, "strengthen_target_action", CheckResult::STRENGTHEN);
    event_trace_detail(k, "strengthen_target_clause_id", c->id());
    event_trace_detail(k, "strengthen_target_removed", !retained);
    event_trace_detail(k, "strengthen_target_flipped",
                       kernel_externalize(k, p.res.flipped));
  }
#endif
  const uint64_t id = ++k.next_clause_id;
  if (k.proof) {
    if (lrat) {
      chain.insert(chain.end(), p.absorbed.begin(), p.absorbed.end());
      chain.push_back(p.id);    // partner propagates the flipped literal
      chain.push_back(c->id()); // then the original conflicts
      k.proof->add_rup(id, lits.data(), static_cast<uint32_t>(lits.size()),
                       chain.data(), static_cast<uint32_t>(chain.size()));
    } else {
      k.proof->add_rup(0, lits.data(), static_cast<uint32_t>(lits.size()),
                       nullptr, 0);
    }
  }
  k.stats.strengthened++;
  (*changed)++;
  // Rewrites preserve redundancy; read before the replacement garbages `c`.
  // kissat's fused forward pass (forward.c) shrinks the record in place and
  // leaves glue untouched -- only search-time OTFS (strengthen.c), Sweep, and
  // Vivify clamp to size-1. Preserving the old glue here keeps tier and
  // Reduce retention on the reference trajectory.
  const bool redundant = c->redundant();
  const uint32_t glue = c->glue();
  if (lits.size() >= 3) {
    // Kissat shrinks this record in place.  Preserve its arena/dense-occurrence
    // position instead of letting Tactix's packed append representation turn a
    // rewrite into new BVE material and a different later occurrence order.
    kernel_replace_large_clause_in_place(
        k, *ref, id, lits.data(), static_cast<uint32_t>(lits.size()),
        redundant ? glue : 0, "strengthen");
    return 2;
  }
  switch (kernel_replace_clause(k, c, id, lits.data(),
                                static_cast<uint32_t>(lits.size()), redundant,
                                redundant ? glue : 0, ref)) {
  case ReplaceOutcome::Refuted:
    return 5; // corpus 002: the empty clause just emitted IS the refutation
  case ReplaceOutcome::Unit: // caller owns the appropriate propagation image
    k.unit_ids[var_of(lits[0])] = id;
    kernel_assign_unit(k, lits[0]);
    return 4;
  case ReplaceOutcome::Binary:
    if (new_binary)
      *new_binary = {lits[0], lits[1]};
    return 3;
  default:
    return 2;
  }
}

ApplyResult strengthen_apply(Kernel &k) {
  assert(strengthen_guard(k));
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.strengthen_rounds++;
  // Native forward admission consumes clause-addition marks, not root-fixed
  // notifications: Kissat's mark_fixed_literal does not set flags.subsume.
  // Keep actual addition marks even on fixed variables. C1/CCE retain their
  // separate root-notification consumer; public instances remain unfiltered.
  const bool filter = k.touch_filter;
  const uint64_t snapshot = k.touch_epoch;
  const uint64_t wm = k.strengthen_touch_mark;
  const uint64_t binary_removed = sweep_forward_binaries(k, wm, filter);
  if (binary_removed == UINT64_MAX)
    return ApplyResult::Unsat;
  uint64_t changed = binary_removed;
  // §6 termination polling before and inside the candidate loop. Like
  // Kissat's `forward_subsume_all_clauses`, one candidate gets at most one
  // subsume-or-strengthen action in a round, so there is no same-candidate
  // cascade to poll. Every site aborts between candidates, never mid-rewrite,
  // so an interrupted round is exactly an incomplete round.
  // No tick cadence at any of them (see subsume_forward.cpp).
  bool interrupted = false;
  const auto poll = [&]() {
    if (!kernel_rule_interrupt(k, k.stats.strengthen_interrupted))
      return false;
    interrupted = true;
    return true;
  };
  // Pinned forward.c does not run sparse root collection here. Candidate
  // discovery retires root-satisfied irredundant clauses, while root-false
  // literals remain physically present unless that particular clause is later
  // strengthened. A former whole-arena root flush converted every effective
  // binary into a physical binary before the pass and changed the next BVE
  // schedule. Keep startup/Reduce sparse collection as the only broad
  // root-false stripping owners.
  interrupted = poll();
  if (!interrupted)
    for (Ref ref = 0; ref < k.arena.size();) {
      Clause *c = clause_at(k.arena, ref);
      const uint32_t words = clause_actual_words(c);
      if (!c->garbage() && !c->redundant() &&
          c->size <= STRENGTHEN_CLSLIM) {
        bool satisfied = false;
        for (const Lit lit : *c)
          satisfied |= k.value(lit) > 0;
        if (satisfied) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
          if (selected_strengthen_external_in(k, c)) {
            event_trace_detail(k, "strengthen_target_action", 3);
            event_trace_detail(k, "strengthen_target_clause_id", c->id());
            event_trace_detail(k, "strengthen_target_removed", 1);
          }
#endif
          kernel_delete_clause(k, c);
          k.stats.flushed++;
          changed++;
        }
      }
      ref += words;
    }
  std::vector<Ref> candidates;
  if (!interrupted) {
    strengthen_candidates(k, candidates);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    for (const Ref ref : candidates) {
      const Clause *c = clause_at(k.arena, ref);
      if (!selected_strengthen_clause(c))
        continue;
      uint32_t touched = 0;
      for (const Lit lit : *c)
        touched += kernel_touched_since(k, var_of(lit), wm);
      event_trace_detail(k, "strengthen_selected_pre_filter", 1);
      event_trace_detail(k, "strengthen_selected_touched", touched);
      event_trace_detail(k, "strengthen_selected_size", c->size);
      break;
    }
#endif
    if (filter) {
      size_t kept = 0;
      for (const Ref ref : candidates) {
        const Clause *c = clause_at(k.arena, ref);
        if (touched_on_two_variables(k, c, wm)) {
          candidates[kept++] = ref;
          continue;
        }
        k.stats.strengthen_mark_skipped++;
        k.stats.strengthen_red_skipped += c->redundant();
      }
      candidates.resize(kept);
    }
#ifdef TACTIX_ENABLE_EVENT_TRACE
    bool retained = false;
    for (const Ref ref : candidates)
      retained |= selected_strengthen_clause(clause_at(k.arena, ref));
    if (selected_strengthen_clause_id())
      event_trace_detail(k, "strengthen_selected_retained", retained);
#endif
  }
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  std::vector<std::vector<ForwardWatch>> forward_watches(2 * k.nvars);
  // `kissat_enter_dense_mode` flushes every large search watch before the
  // first forward round, and each preceding elimination round flushes its
  // dense large occurrences before the next one.  Initialize the exact
  // surviving binary prefix; large partners are appended by `connect` below.
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary())
        forward_watches[lit].push_back(
            ForwardWatch::binary_entry(watch.other()));
  // Kissat's `SET_EFFORT_LIMIT(forward_steps)` is tested strictly between
  // candidates.  A candidate is indivisible even if its scans overrun the
  // allowance; the next candidate is admitted when spent == budget and
  // stopped only when spent > budget.  Survivors stay candidates, so the cut
  // is resumable.  Direct rule calls remain unbounded unless explicitly given
  // a budget; DefaultPolicy enables this uniformly for its fused forward pass.
  ForwardStepCounter steps(k.stats.strengthen_visits);
  const uint64_t check_start = steps.value;
  const bool step_bounded = k.midround_budget && k.work_budget;
  const uint64_t step_budget = k.work_budget;
  // Connect a scheduled survivor at its least-occurring free literal in the
  // current mixed forward stream.  Under the canonical filter every active
  // variable must be armed, exactly as Kissat's `connect_subsuming` requires;
  // clauses that failed the two-variable scheduling predicate never enter the
  // stream.
  //
  // Redundant survivors are NOT indexed: `occs` is the partner index, a partner
  // justifies deleting or rewriting another clause, so it must not be
  // forgettable (the header's Forget argument). Widening the victim set
  // therefore leaves the partner set where it was, which is also what keeps the
  // band's cost bounded — band clauses are checked, never chased.
  const auto connect = [&](Ref cr) {
    const Clause *c = clause_at(k.arena, cr);
    if (c->redundant())
      return;
    if (filter)
      for (const Lit l : *c) {
        if (k.value(l) != 0)
          continue;
        if (!kernel_touched_since(k, var_of(l), wm))
          return;
      }
    Lit min_lit = INVALID_LIT;
    size_t min_occs = 0;
    for (const Lit l : *c) {
      if (k.value(l) != 0)
        continue;
      const size_t n = forward_watches[l].size();
      // `connect_subsuming` updates the winner on equality, hence the last
      // minimum literal in physical clause order wins.
      if (min_lit == INVALID_LIT || n <= min_occs) {
        min_lit = l;
        min_occs = n;
      }
    }
    // under-derived roots (corpus 002) can leave a candidate with no free
    // literals; nothing to connect then
    if (min_lit != INVALID_LIT && min_occs <= STRENGTHEN_OCCLIM)
      forward_watches[min_lit].push_back(ForwardWatch::large_entry(cr));
  };
  size_t stopped_at = candidates.size(); // index of the first unreached one
  std::vector<Ref> rewritten_survivors;
  bool made_binary = false;
  for (size_t ci = 0; ci < candidates.size(); ci++) {
    Ref cref = candidates[ci];
    if (step_bounded && steps.value - check_start > step_budget) {
      k.stats.budget_stops_c2++;
      stopped_at = ci;
      break;
    }
    if (poll()) {
      stopped_at = ci;
      break;
    }
    const bool red = clause_at(k.arena, cref)->redundant();
    k.stats.strengthen_red_checked += red;
    const uint64_t red_sub_before = k.stats.strengthen_subsumed;
    const uint64_t red_str_before = k.stats.strengthened;
    Clause *c = clause_at(k.arena, cref);
    if (c->garbage())
      continue;
    // Earlier candidates can change roots. Re-evaluate this scheduled clause
    // before marking it; the native candidate step is charged only when at
    // least two free literals remain and partner discovery actually runs.
    unsigned non_false = 0;
    Lit remaining = INVALID_LIT;
    bool satisfied = false;
    for (const Lit lit : *c) {
      if (k.value(lit) > 0) {
        satisfied = true;
        break;
      }
      if (k.value(lit) == 0) {
        ++non_false;
        remaining = lit;
        marks[lit] = 1;
      }
    }
    k.stats.strengthen_checked++;
    if (satisfied) {
      mark_candidate(k, marks, c, 0);
      kernel_delete_clause(k, c);
      ++k.stats.flushed;
      ++changed;
      continue;
    }
    if (!non_false) {
      forward_refute(k, c);
      return ApplyResult::Unsat;
    }
    if (non_false == 1) {
      marks[remaining] = 0;
      forward_assign_root(k, remaining, cref, false);
      kernel_delete_clause(k, c);
      ++changed;
      if (!forward_flush_units(k, forward_watches, changed))
        return ApplyResult::Unsat;
      continue;
    }
#ifdef TACTIX_ENABLE_EVENT_TRACE
    const bool selected = selected_strengthen_clause(c);
    if (selected) {
      event_trace_detail(k, "strengthen_selected_index", ci);
      Lit scan = INVALID_LIT, positive = INVALID_LIT;
      int scan_external = 0, other_external = 0;
      if (selected_strengthen_pair(&scan_external, &other_external)) {
        for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
          const int external = kernel_externalize(k, lit);
          if (external == scan_external)
            scan = lit;
          else if (external == other_external)
            positive = lit;
        }
      }
      if (scan != INVALID_LIT && positive != INVALID_LIT) {
        const Lit other = positive;
        uint32_t stream_matches = 0;
        for (const ForwardWatch &watch : forward_watches[scan])
          stream_matches += watch.binary && watch.other() == other;
        event_trace_detail(k, "strengthen_selected_binary_copies",
                           kernel_binary_copies(k, scan, other));
        event_trace_detail(k, "strengthen_selected_stream_size",
                           forward_watches[scan].size());
        event_trace_detail(k, "strengthen_selected_stream_matches",
                           stream_matches);
      }
    }
#endif
    steps.value++; // Kissat `INC(forward_steps)` in forward_marked_clause.
    const Partner p =
        find_forward_partner(k, marks, c, forward_watches, steps.value, changed);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    trace_forward_result(k, c, p, ci, forward_watches);
#endif
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (selected) {
      event_trace_detail(k, "strengthen_selected_result", p.res.kind);
      event_trace_detail(
          k, "strengthen_selected_flipped",
          p.res.flipped == INVALID_LIT
              ? 0
              : kernel_externalize(k, p.res.flipped));
    }
#endif
    mark_candidate(k, marks, clause_at(k.arena, cref), 0);
    std::pair<Lit, Lit> new_binary{INVALID_LIT, INVALID_LIT};
    const int r = act(k, &cref, p, &changed, &new_binary);
    if (r == 5)
      return ApplyResult::Unsat;
    // Kissat connects both an unchanged survivor and the once-strengthened
    // candidate for use as a partner by later candidates.  It does not
    // rescan the strengthened candidate until a later forward round.
    if (r == 0 || r == 2)
      connect(cref);
    if (r == 2)
      rewritten_survivors.push_back(cref);
    if (r == 3) {
      made_binary = true;
      const Lit a = new_binary.first, b = new_binary.second;
      assert(a != INVALID_LIT && b != INVALID_LIT);
      forward_watches[a].push_back(ForwardWatch::binary_entry(b));
      forward_watches[b].push_back(ForwardWatch::binary_entry(a));
    }
    if (r == 4 && !forward_flush_units(k, forward_watches, changed))
      return ApplyResult::Unsat;
    if (red) { // the band's YIELD, read against `strengthen_red_checked`
      k.stats.strengthen_red_subsumed +=
          k.stats.strengthen_subsumed - red_sub_before;
      k.stats.strengthen_red_strengthened +=
          k.stats.strengthened - red_str_before;
    }
    if (interrupted) {
      stopped_at = ci + 1; // this candidate was checked; the rest were not
      break;
    }
  }
  // Resume (see the twin comment in subsume_forward.cpp). A re-arm writes the
  // array the candidate's own creation would have written — `touched` for an
  // irredundant clause, `touched_kept` for a band one — because re-arming a
  // band candidate through `touched` would leak the band into
  // BCE/X2/X4/Factor's filters and break `Kernel::touched_kept`'s isolation.
  for (size_t ci = interrupted ? candidates.size()
                               : (filter ? stopped_at : candidates.size());
       ci < candidates.size(); ci++) {
    const Clause *c = clause_at(k.arena, candidates[ci]);
    if (c->garbage())
      continue;
    const bool red = c->redundant();
    for (const Lit l : *c)
      red ? kernel_touch_kept_var(k, var_of(l)) : kernel_touch_var(k, var_of(l));
    k.stats.strengthen_rearmed++;
  }
  // An interrupted round does not advance the watermark at all: the re-arm
  // loop above is what makes advancing safe, and an entry abort leaves
  // `candidates` empty, so advancing would retire material this round never
  // looked at.
  if (filter && !interrupted) {
    k.strengthen_touch_mark = snapshot;
    k.stats.strengthen_mark_advances++;
  }
  // A unit-only rewrite which later leaves no live strengthened candidate
  // does not itself re-arm native forward subsumption. Live large rewrites
  // and newly created binaries do; so do unreached candidates or interruption.
  const bool live_rewrite = std::any_of(
      rewritten_survivors.begin(), rewritten_survivors.end(),
      [&](Ref ref) { return !clause_at(k.arena, ref)->garbage(); });
  k.strengthen_complete = !interrupted && stopped_at == candidates.size() &&
                          !made_binary && !live_rewrite;
  if (!changed)
    return ApplyResult::NoOp;
  // Tactix still uses a packed variable-width arena. Until the remaining
  // collapsed-source lifecycle is represented in place, this collection is
  // the production physical boundary that prevents stale source allocation
  // from changing later sparse watch order. The isolated lazy-forward arm is
  // retained only in its frozen campaign, not as a default mechanism.
  kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

// Single-candidate instance surface (payload = arena Ref, like C1); applies at
// most one subsume-or-strengthen step, no cascade.
ApplyResult strengthen_try(Kernel &k, Ref candidate) {
  assert(strengthen_guard(k));
  // `work_budget` is a round device and does not reach this path, so
  // termination is checked here at entry, before the per-clause flush, where
  // nothing is part-rewritten.
  if (kernel_rule_interrupt(k, k.stats.strengthen_interrupted))
    return ApplyResult::NoOp;
  Clause *c = clause_at(k.arena, candidate);
  if (c->garbage())
    return ApplyResult::NoOp;
  // The victim may be redundant on this path (N1: the instance surface is
  // complete over everything the round could accept, and more); the partner may
  // not — find_partner's `d->redundant()` skip is the load-bearing check.
  if (!is_victim(k, c))
    return ApplyResult::NoOp;
  k.stats.strengthen_checked++;
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  mark_candidate(k, marks, c, 1);
  uint64_t visits = k.stats.strengthen_visits;
  const Partner p = find_partner(k, marks, c, nullptr, candidate, visits,
                                 visits, false, 0, nullptr);
  k.stats.strengthen_visits = visits;
  mark_candidate(k, marks, clause_at(k.arena, candidate), 0);
  uint64_t changed = 0;
  Ref ref = candidate;
  const int r = act(k, &ref, p, &changed);
  if (r == 5)
    return ApplyResult::Unsat;
  if (!changed)
    return ApplyResult::NoOp;
  kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

// Complete over victims, band or no band, so `instances()` never advertises
// less than `apply_instance` accepts.
static uint32_t strengthen_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Ref> candidates;
  strengthen_candidates(k, candidates, /*complete=*/true);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(candidates.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = candidates[i];
  return n;
}

static ApplyResult strengthen_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return strengthen_apply(k);
  for (Ref ref = 0; ref < k.arena.size();) {
    if (ref == inst)
      return strengthen_try(k, ref);
    if (ref > inst)
      break;
    ref += clause_actual_words(clause_at(k.arena, ref));
  }
  return ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(Strengthen, RuleTier::Inprocess, strengthen_guard,
                     strengthen_apply, strengthen_instances,
                     strengthen_apply_instance);

} // namespace tactix
