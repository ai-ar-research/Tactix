#include "frontend/parse.hpp"

#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/rule_parameters.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <sys/resource.h>

namespace tactix {

namespace {

struct ParseMemoryTrace {
  FILE *out = nullptr;
  uint64_t next_clause = 5000000;

  ParseMemoryTrace() {
    const char *path = std::getenv("TACTIX_PARSE_MEMORY_TRACE");
    if (path && *path)
      out = std::fopen(path, "w");
  }

  ~ParseMemoryTrace() {
    if (out)
      std::fclose(out);
  }

  void emit(const Kernel &k, const char *stage) {
    if (!out)
      return;
    struct rusage usage {};
    getrusage(RUSAGE_SELF, &usage);
    std::fprintf(
        out,
        "%s\tclauses=%llu\tvars=%u\trss_kib=%ld\tbinary_count=%zu\t"
        "watch_heads=%zu\twatch_words=%zu\twatch_capacity=%zu\t"
        "watch_holes=%llu\tarena_words=%zu\ttrail_capacity=%zu\tframes_capacity=%zu\t"
        "activation_capacity=%zu\n",
        stage, (unsigned long long)k.next_clause_id, k.nvars,
        usage.ru_maxrss, k.binary_ids.size(), k.watches.heads.size(),
        k.watches.arena.size(), k.watches.arena.capacity(),
        (unsigned long long)k.watches.holes, k.arena.size(),
        k.trail.capacity(), k.frames.capacity(),
        k.input_activation_order.capacity());
    std::fflush(out);
  }

  void maybe(const Kernel &k) {
    if (!out || k.next_clause_id < next_clause)
      return;
    emit(k, "import");
    next_clause = ((k.next_clause_id / 5000000) + 1) * 5000000;
  }
};

class RawInputObservation {
public:
  RawInputObservation(Kernel &kernel, uint64_t raw_input_id,
                      const std::vector<Lit> &clause) noexcept
      : kernel_(kernel), id_(raw_input_id), normalized_(clause) {
    if (!kernel_.population_observer ||
        !kernel_.population_observe_raw_input)
      return;
    RawInputPopulationEvent event;
    event.kind = RawInputEventKind::Clause;
    event.raw_input_id = id_;
    event.raw = kernel_population_clause(
        kernel_, clause.data(), static_cast<uint32_t>(clause.size()));
    kernel_.population_observer->observe(event);
  }

  ~RawInputObservation() {
    if (!kernel_.population_observer ||
        !kernel_.population_observe_raw_input)
      return;
    RawInputPopulationEvent event;
    event.kind = RawInputEventKind::Outcome;
    event.raw_input_id = id_;
    event.disposition = disposition_;
    event.normalized_available = normalized_available_;
    event.source_classification =
        normalized_available_ && !normalized_.empty() &&
                (disposition_ & RawAdmitted)
            ? PopulationSourceClassification::Eligible
            : PopulationSourceClassification::Ineligible;
    if (normalized_available_)
      event.normalized = kernel_population_clause(
          kernel_, normalized_.data(),
          static_cast<uint32_t>(normalized_.size()));
    kernel_.population_observer->observe(event);
  }

  void note(uint32_t flags) noexcept { disposition_ |= flags; }
  void normalized_available() noexcept { normalized_available_ = true; }

private:
  Kernel &kernel_;
  uint64_t id_;
  const std::vector<Lit> &normalized_;
  uint32_t disposition_ = RawNone;
  bool normalized_available_ = false;
};

} // namespace

static void grow_input_binary_liveness(Kernel &k, size_t entries) {
  assert(k.binary_liveness);
  k.binary_liveness->reserve(entries);
  for (Lit lit = 0; lit < 2 * k.nvars; lit++)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary() && lit < watch.other())
        k.binary_liveness->add(kernel_binary_key(lit, watch.other()));
}

static void capture_raw_input_shape(Kernel &k,
                                    const std::vector<Lit> &scratch) {
  if (k.startup_certificate_open) {
    for (const Lit literal : scratch)
      k.startup_input.push_back(kernel_externalize(k, literal));
    k.startup_input.push_back(0);
  }
  if (scratch.size() == 1) {
    ++k.input_unit_clause_count;
  } else {
    ++k.input_nonunit_clause_count;
    const uint32_t width = static_cast<uint32_t>(scratch.size());
    if (k.input_nonunit_clause_count == 1)
      k.input_uniform_nonunit_width = width;
    else if (width != k.input_uniform_nonunit_width)
      k.input_nonunit_uniform = false;
  }
  std::array<int32_t, 2> tail{0, 0};
  if (scratch.size() == 2) {
    ++k.input_binary_clause_count;
    tail[0] = kernel_externalize(k, scratch[0]);
    tail[1] = kernel_externalize(k, scratch[1]);
    if (tail[0] > 0 && tail[1] > 0)
      ++k.input_positive_binary_count;
  }
  if (scratch.size() == 3)
    ++k.input_ternary_clause_count;
  if (k.input_clause_tail.size() < Kernel::INPUT_CLAUSE_TAIL_CAPACITY) {
    k.input_clause_tail.push_back(tail);
  } else {
    k.input_clause_tail[k.input_clause_tail_next] = tail;
    k.input_clause_tail_next =
        (k.input_clause_tail_next + 1) % Kernel::INPUT_CLAUSE_TAIL_CAPACITY;
  }

  if (scratch.size() <= 10)
    return;
  if (k.input_long_clause_count != UINT32_MAX)
    ++k.input_long_clause_count;
  if (!k.input_fermat_long_shape || k.input_long_clause_count > 2) {
    k.input_fermat_long_shape = false;
    return;
  }
  // The recovered constant has k-1 bits and the owner is a 64-bit strategy.
  if (scratch.size() > 64) {
    k.input_fermat_long_shape = false;
    return;
  }
  const uint32_t width = static_cast<uint32_t>(scratch.size());
  if (k.input_long_clause_count == 1)
    k.input_fermat_width = width;
  else if (width != k.input_fermat_width) {
    k.input_fermat_long_shape = false;
    return;
  }
  const uint32_t first = k.input_long_clause_count == 1
                             ? 1
                             : k.input_fermat_width + 1;
  for (uint32_t i = 0; i < width; ++i)
    if (kernel_externalize(k, scratch[i]) !=
        -static_cast<int>(first + i)) {
      k.input_fermat_long_shape = false;
      return;
    }
}

// N2 input identity: the checker numbers raw file clauses by position, so any
// normalization can diverge from the stored clause. Add each exact narrowed
// clause after input: LRAT cites root units then the file id; DRAT searches for
// the same RUP justification. This includes duplicate removal because a later
// DRAT deletion matches a literal multiset, not set semantics. Mid-input
// refutation is likewise deferred so LRAT's input namespace is never crossed.
// Cases and checker evidence:
// benchmarks/reports/2026-08-05-input-clause-normalization-n2.md §2.
static void restate_input(Kernel &k, uint64_t id, const std::vector<Lit> &lits,
                          const std::vector<Lit> &units, Ref ref,
                          const std::vector<Lit> &original) {
  // Record that the checker's clause `id` is not the clause just stored. The
  // count is unconditional, being the reachability probe for this path; the
  // restatement matters only with a proof, since without one no checker
  // population can diverge from the normalized solver population.
  k.stats.input_restated++;
  if (!k.proof)
    return;
  // DSR and DRAT both delete by literals. A retained raw clause can make a
  // later deletion retire the wrong copy or change a RAT/SR obligation. Both
  // formats can publish the exact replacement and retire its raw predecessor
  // immediately; only LRAT must wait for the complete input id namespace.
  if (!k.proof->lrat()) {
    assert(!original.empty());
    k.proof->add_rup(id, lits.data(), static_cast<uint32_t>(lits.size()),
                     nullptr, 0);
    k.proof->del(k.next_clause_id, id, original.data(),
                 static_cast<uint32_t>(original.size()));
    return;
  }
  Kernel::InputRestatement r;
  r.id = id;
  r.begin = static_cast<uint32_t>(k.input_restate_lits.size());
  r.size = static_cast<uint32_t>(lits.size());
  r.unit_begin = static_cast<uint32_t>(k.input_restate_units.size());
  r.unit_size = static_cast<uint32_t>(units.size());
  r.ref = ref;
  k.input_restate_lits.insert(k.input_restate_lits.end(), lits.begin(),
                              lits.end());
  k.input_restate_units.insert(k.input_restate_units.end(), units.begin(),
                               units.end());
  k.input_restatements.push_back(r);
}

// Kissat closes every new input unit immediately through
// `kissat_search_propagate` (internal.c:363), so later clauses are added
// against the resulting root assignment and the scan contributes to both
// `ticks` and `search_ticks`. Do the same for the proof-less and DRAT paths.
// Native LRAT must defer this closure until all original input ids have been
// consumed: propagation-derived units need fresh ids, and a derived id may not
// precede a later original id in the LRAT namespace.
static void propagate_input_unit(Kernel &k) {
  if (k.proof && k.proof->lrat())
    return;
  const bool conflict = kernel_propagate(k);
  if (!conflict)
    return;
  if (k.proof)
    k.proof->add_empty(0, nullptr, 0); // DRAT needs no antecedent ids
  k.inconsistent = true;
}

[[gnu::noinline]] static void ingest_clause_generic(Kernel &k,
                                                    std::vector<Lit> &scratch) {
  // Every input clause consumes one proof id in input order, including
  // tautologies and duplicates that are never stored (LRAT input numbering).
  const uint64_t id = ++k.next_clause_id;
  // Headerless C API input has no earlier identity-map reservation. Diagnostic
  // proof shadowing translates raw literals through that map, so grow it before
  // recording the original clause (still before normalization/import reorder).
  Var maxv = 0;
  for (const Lit l : scratch)
    maxv = std::max(maxv, var_of(l));
  if (!scratch.empty()) {
    // DIMACS counts are advisory. An input literal beyond the header ends
    // the fixed-universe bitmap's scope before any lookup can address it.
    // Rebuild the ordinary filter from the exact live watches, retaining all
    // previous pairs so later duplicates still consume only their input id.
    if (maxv >= k.nvars && k.binary_liveness && k.binary_liveness->exact())
      grow_input_binary_liveness(k, k.binary_ids.size());
    kernel_reserve_vars(k, maxv + 1);
  }
  kernel_sync_population_observer(k);
  RawInputObservation observation(k, id, scratch);
  capture_raw_input_shape(k, scratch);
  // Seed the diagnostic proof shadow from the checker's ACTUAL starting
  // population, before normalization changes or drops this clause. The checker
  // reads every file clause, including tautologies and duplicate copies; a
  // snapshot of only the solver's later live formula cannot model DRAT
  // deletion-by-literal-multiset.
  if (k.proof)
    k.proof->seed_population(id, scratch.data(),
                             static_cast<uint32_t>(scratch.size()));

  // Kissat's default `tumble=1` imports variables by first occurrence and
  // activates the literals of each surviving clause in that same file order
  // (internal.c `kissat_add` -> flags.c `kissat_activate_literals`).  Record
  // imports over the raw file literals now; activation follows below, after
  // the set-semantics pass, which keeps first-occurrence file order.
  const size_t file_size = scratch.size();
  // Capture only clauses that actually change, before the first in-place
  // rewrite. Unchanged input carries no extra allocation or literal copy.
  std::vector<Lit> original;
  const auto retain_original = [&] {
    if (k.proof && !k.proof->lrat() && original.empty())
      original = scratch;
  };
  const auto retire_unstored = [&] {
    if (!k.proof || k.proof->lrat())
      return;
    retain_original();
    const auto &raw = original.empty() ? scratch : original;
    k.proof->del(k.next_clause_id, id, raw.data(),
                 static_cast<uint32_t>(raw.size()));
  };
  for (const Lit lit : scratch)
    kernel_record_import(k, var_of(lit));

  // Set semantics: dedupe; tautologies contribute nothing.  Marks decide
  // membership, so the clause is rewritten in place in stable first-occurrence
  // file order, as Kissat stores it while all literals are unassigned;
  // sorting storage here would change the initial watched pair and the order
  // of every affected watch list.
  if (file_size == 2) {
    if (scratch[0] == negate(scratch[1])) {
      observation.note(RawDropped | RawTautology | RawBinary);
      retire_unstored();
      return;
    }
    if (scratch[0] == scratch[1]) {
      observation.note(RawRepeatedLiteral);
      retain_original();
      scratch.resize(1);
    }
  } else if (file_size > 2) {
    if (k.input_marks.size() < 2 * k.nvars)
      k.input_marks.resize(2 * k.nvars, 0);
    size_t kept = 0;
    bool tautology = false;
    for (size_t i = 0; i < file_size; i++) {
      const Lit lit = scratch[i];
      if (k.input_marks[lit]) {
        observation.note(RawRepeatedLiteral);
        retain_original();
        continue; // repeated literal: the first occurrence stands
      }
      if (k.input_marks[negate(lit)]) {
        tautology = true;
        break;
      }
      k.input_marks[lit] = 1;
      scratch[kept++] = lit;
    }
    for (size_t i = 0; i < kept; i++)
      k.input_marks[scratch[i]] = 0;
    if (tautology) {
      observation.note(RawDropped | RawTautology);
      retire_unstored();
      return;
    }
    scratch.resize(kept);
  }
  // Kissat internal.c:kissat_add normalizes every input clause against units
  // already fixed while parsing.  A root-true literal satisfies the clause;
  // a root-false literal is removed, with its true complement retained as the
  // LRAT antecedent for the deferred restatement below.  The normal and DRAT
  // paths propagate new units immediately, matching Kissat; native LRAT alone
  // defers derived units because their ids cannot precede later input ids.
  const bool native_lrat = k.proof && k.proof->lrat();
  std::vector<Lit> root_units;
  auto out = scratch.begin();
  for (const Lit lit : scratch) {
    const int8_t value = k.values[lit];
    if (value > 0) {
      observation.note(RawDropped | RawRootSatisfied);
      retire_unstored();
      return; // satisfied input clause: consume its id, activate/store nothing
    }
    if (value < 0) {
      observation.note(RawRootShortened);
      retain_original();
      if (native_lrat)
        root_units.push_back(negate(lit));
    }
    else
      *out++ = lit;
  }
  scratch.erase(out, scratch.end());
  observation.normalized_available();

  // N2: the checker holds the file clause under `id`, repeats and all, so a
  // shrunk clause may not keep it. Add the exact stored clause at finish_input,
  // but only when it is actually stored — a dropped clause is never deleted or
  // used as the solver-side premise of a later inference.
  const bool shrunk = scratch.size() < file_size;

  if (k.inconsistent) {
    observation.note(RawDropped | RawAlreadyInconsistent);
    return; // already refuted; further clauses don't matter
  }

  // Activation, kissat's `tumble` order (heuristics_seed_activation_order).
  // Placed after the tautology return and before the store so it matches
  // kissat's per-clause activation in `kissat_add`: a tautology activates
  // nothing, and first activation follows file-literal order rather than the
  // canonical sorted storage order. input_activated removes repeated
  // literals in one clause and variables activated by an earlier clause; it
  // also identifies variables in no surviving clause at input finalization.
  // The normalized clause holds the unassigned distinct literals in
  // first-occurrence file order, which is exactly the population and order
  // the raw file literals would activate: repeats are skipped by
  // input_activated and root-assigned variables are never activated.
  if (k.input_activated.size() < k.nvars)
    k.input_activated.resize(k.nvars, 0);
  for (const Lit lit : scratch) {
    const Var v = var_of(lit);
    assert(k.values[make_lit(v, false)] == 0);
    if (!k.input_activated[v]) {
      k.input_activated[v] = 1;
      if (k.tumble)
        k.input_activation_order.push_back(v);
    }
  }

  if (scratch.empty()) { // empty input clause: immediate refutation
    observation.note(RawAdmitted | RawEmpty |
                     (file_size ? RawRootShortened : RawNone));
    k.inconsistent = true;
    k.input_refuted = true; // the input empty clause is the whole chain
    k.input_refutation_id = id;
    k.input_refutation_units = root_units;
  } else if (scratch.size() == 1) {
    observation.note(RawUnit);
    const Lit u = scratch[0];
    if (k.values[u] > 0) {
      observation.note(RawDropped | RawDuplicate);
      retire_unstored();
      return; // duplicate unit (its id is consumed, never referenced)
    }
    if (k.values[u] < 0) { // contradicting unit: refuted at the root
      observation.note(RawAdmitted | RawContradiction);
      k.inconsistent = true;
      k.input_refuted = true;
      k.input_refutation_id = id;
      k.input_refutation_units = {negate(u)};
      return;
    }
    k.unit_ids[var_of(u)] = id;
    observation.note(RawAdmitted);
    if (shrunk)
      restate_input(k, id, scratch, root_units, 0, original);
    kernel_assign_unit(k, u);
    propagate_input_unit(k);
  } else if (scratch.size() == 2) {
    observation.note(RawBinary);
    // A duplicate binary consumes its input id but is not stored or proved.
    // No deletion occurs during import. Native LRAT's BinaryIdMap is therefore
    // the exact live-set image. DRAT/proofless import first asks its compact
    // no-false-negative filter, then confirms possible hits against mirrored
    // watches, which remain the exact identity authority after input.
    // Evidence: benchmarks/reports/2026-08-27-sat-factor-marks-search-localization.md.
    const uint64_t key = kernel_binary_key(scratch[0], scratch[1]);
    const bool duplicate = k.binary_ids.tracks_keys()
                               ? k.binary_ids.count(key)
                               : kernel_binary_live(k, scratch[0], scratch[1]);
    if (duplicate) {
      observation.note(RawDropped | RawDuplicate);
      // Each raw duplicate still exists in the checker. Keep exactly the
      // one copy represented by the kernel so a later deletion removes it.
      if (k.proof && !k.proof->lrat()) {
        const auto &raw = original.empty() ? scratch : original;
        k.proof->del(k.next_clause_id, id, raw.data(),
                     static_cast<uint32_t>(raw.size()));
      }
      return;
    }
    if (!k.binary_ids.tracks_keys() && k.binary_liveness &&
        k.binary_liveness->needs_growth(k.binary_ids.size() + 1))
      grow_input_binary_liveness(k, k.binary_ids.size() + 1);
    kernel_add_new_binary_clause(k, scratch[0], scratch[1], id);
    observation.note(RawAdmitted);
    if (k.input_binary_order_captured)
      k.input_binary_order.push_back(key);
    // Kissat's original-clause constructor checks vector defragmentation only
    // after the complete clause has joined the watch database.
    k.watches.defrag_if_needed();
    if (shrunk)
      restate_input(k, id, scratch, root_units, 0, original);
  } else {
    observation.note(RawAdmitted | RawLarge);
    const Ref ref = kernel_add_large_clause(
        k, scratch.data(), static_cast<uint32_t>(scratch.size()), false, 0, id);
    k.watches.defrag_if_needed();
    if (shrunk)
      restate_input(k, id, scratch, root_units, ref, original);
  }
}

void ingest_clause(Kernel &k, std::vector<Lit> &scratch) {
  // Keep the ordinary normalized binary path in a small frame of its own.
  // The generic importer owns proof-restatement and root-unit vectors needed
  // by the exceptional cases; constructing its frame for every binary input
  // clause is avoidable when the DIMACS header has already reserved both
  // variables and neither literal has a root value.
  if (scratch.size() == 2) {
    const Lit a = scratch[0], b = scratch[1];
    if (a != b && a != negate(b) && var_of(a) < k.nvars &&
        var_of(b) < k.nvars && !k.inconsistent && k.values[a] == 0 &&
        k.values[b] == 0) {
      const uint64_t id = ++k.next_clause_id;
      kernel_sync_population_observer(k);
      RawInputObservation observation(k, id, scratch);
      observation.normalized_available();
      observation.note(RawBinary);
      capture_raw_input_shape(k, scratch);
      if (k.proof)
        k.proof->seed_population(id, scratch.data(), 2);
      kernel_record_import(k, var_of(a));
      kernel_record_import(k, var_of(b));

      if (k.input_activated.size() < k.nvars)
        k.input_activated.resize(k.nvars, 0);
      for (const Lit lit : {a, b}) {
        const Var v = var_of(lit);
        if (!k.input_activated[v]) {
          k.input_activated[v] = 1;
          if (k.tumble)
            k.input_activation_order.push_back(v);
        }
      }

      const uint64_t key = kernel_binary_key(a, b);
      const bool duplicate = k.binary_ids.tracks_keys()
                                 ? k.binary_ids.count(key)
                                 : kernel_binary_live(k, a, b);
      if (duplicate) {
        observation.note(RawDropped | RawDuplicate);
        if (k.proof && !k.proof->lrat())
          k.proof->del(k.next_clause_id, id, scratch.data(), 2);
        return;
      }
      if (!k.binary_ids.tracks_keys() && k.binary_liveness &&
          k.binary_liveness->needs_growth(k.binary_ids.size() + 1))
        grow_input_binary_liveness(k, k.binary_ids.size() + 1);
      kernel_add_new_binary_clause(k, a, b, id);
      observation.note(RawAdmitted);
      if (k.input_binary_order_captured)
        k.input_binary_order.push_back(key);
      k.watches.defrag_if_needed();
      return;
    }
  }
  ingest_clause_generic(k, scratch);
}

void finish_input(Kernel &k) {
  kernel_sync_population_observer(k);
  // Search never has more than one assignment per variable, while its level
  // stack cannot exceed the same population.  Reserve both once, after the
  // DIMACS header/import stream has fixed the population, so the propagation
  // hot path does not carry vector growth and relocation into assignment.
  k.trail.reserve(k.nvars);
  k.frames.reserve(static_cast<size_t>(k.nvars) + 1);
  // Input ids are now settled: next_clause_id equals the number of input
  // clauses, exactly the checker's numbering, because ingest_clause consumes
  // one id per clause and nothing else consumed any. Derived ids start here.
  // Restatements are non-empty only when a proof was attached during import.
  // The C API's later attachment first replays the exact raw input into a
  // proof-bearing kernel, then remaps the existing session's live identities.
  assert(k.input_restatements.empty() || k.proof);
  // An unassigned variable absent from every surviving normalized clause is
  // outside Kissat's active/search population, even when a tautological or
  // root-satisfied raw clause mentioned it. Retire that kernel identity
  // without a witness: it occurs in no live constraint, so either value
  // extends a model. Explicit Decide instances still retain the variable
  // through the policy-neutral unassigned surface. Keep its import rank too:
  // that order remains the stable API/debug identity image.
  for (Var v = 0; v < k.nvars; v++) {
    const bool activated = v < k.input_activated.size() && k.input_activated[v];
    if (!activated && k.values[make_lit(v, false)] == 0) {
      if (k.var_status.size() < k.nvars)
        k.var_status.resize(k.nvars, VarStatus::Active);
      if (k.var_status[v] == VarStatus::Active) {
        assert(k.values[make_lit(v, false)] == 0);
        assert(k.unassigned_active > 0);
        k.unassigned_active--;
        kernel_affine_note_namespace_change(k);
        k.var_status[v] = VarStatus::Eliminated;
        if (k.h)
          heuristics_deactivate_var(k, v);
        else
          k.retired_events.push_back(v);
      }
    }
    kernel_record_import(k, v);
  }
  std::vector<uint64_t> chain;
  for (const Kernel::InputRestatement &r : k.input_restatements) {
    const Lit *lits = k.input_restate_lits.data() + r.begin;
    // DRAT additions carry no ids, so retain the solver's input id and do not
    // perturb its proofless trajectory. LRAT must allocate and publish the
    // fresh checker id used by every later hint and deletion.
    const bool native_lrat = k.proof->lrat();
    const uint64_t nid = native_lrat ? ++k.next_clause_id : r.id;
    chain.clear();
    if (native_lrat) {
      for (uint32_t i = 0; i < r.unit_size; i++) {
        const Lit unit = k.input_restate_units[r.unit_begin + i];
        assert(k.values[unit] > 0);
        const uint64_t uid = k.unit_ids[var_of(unit)];
        assert(uid);
        chain.push_back(uid);
      }
      // Root units first remove every root-false literal; the original file
      // clause then conflicts under the negated normalized clause. Duplicate-
      // only normalization has no units and retains the old one-hint proof.
      chain.push_back(r.id);
    }
    k.proof->add_rup(nid, lits, r.size,
                     chain.empty() ? nullptr : chain.data(),
                     static_cast<uint32_t>(chain.size()));
    if (!native_lrat)
      continue;
    k.proof->observe_checker_id_rebind(r.id, nid, lits, r.size);
    if (r.size == 1) {
      assert(k.unit_ids[var_of(lits[0])] == r.id);
      k.unit_ids[var_of(lits[0])] = nid;
    } else if (r.size == 2) {
      const uint64_t key = kernel_binary_key(lits[0], lits[1]);
      assert(k.binary_ids.at(key) == r.id);
      k.binary_ids.insert_or_assign_known(key, nid, false);
    } else {
      Clause *c = clause_at(k.arena, r.ref);
      assert(c->id() == r.id);
      // LRAT restatement changes only the proof identity. The clause stays in
      // its existing arena record, so no packed-replacement position alias is
      // needed.
      c->set_id(nid);
    }
  }
  k.input_restatements.clear();
  k.input_restate_lits.clear();
  k.input_restate_units.clear();
  // Native LRAT and streaming PBP need an exact live proof id for every
  // binary pair. DRAT names clauses by literals and proofless solving has no
  // clause identities, so their key archive has no consumer after import
  // deduplication. Release it for every such input, independent of formula
  // size or shape; size_ remains the monotone material watermark.
  if ((!k.proof || !k.proof->needs_clause_ids()) && !k.replay_lrat_input)
    k.binary_ids.discard_storage();
  if (k.proof && k.proof->population_shadow_available())
    k.proof->arm_population_shadow();
  // The decision order is `H` state.  If `H` is already attached, publish the
  // activation population now. Otherwise retain it until ScopedHeuristics
  // attaches at solve entry. The seeding routine combines it with permanent
  // first-import ranks: Kissat's random brancher samples compact `VARS`, not
  // activation order or every variable admitted by the DIMACS header.
  heuristics_seed_activation_order(k, k.input_activation_order);
  if (k.h) {
    k.input_activation_order.clear();
    k.input_activation_order.shrink_to_fit();
  }
  k.input_activated.clear();
  k.input_activated.shrink_to_fit();
  k.input_marks.clear();
  k.input_marks.shrink_to_fit();
  if (k.input_refuted) {
    k.input_refuted = false;
    if (k.proof) {
      chain.clear();
      if (k.proof->lrat()) {
        for (const Lit unit : k.input_refutation_units) {
          assert(k.values[unit] > 0);
          const uint64_t uid = k.unit_ids[var_of(unit)];
          assert(uid);
          chain.push_back(uid);
        }
        chain.push_back(k.input_refutation_id);
      }
      k.proof->add_empty(++k.next_clause_id,
                         chain.empty() ? nullptr : chain.data(),
                         static_cast<uint32_t>(chain.size()));
    }
    k.input_refutation_units.clear();
  }
  if (k.proof)
    k.proof->finish_input_transaction();
  if (k.population_observer && k.population_observe_raw_input) {
    RawInputPopulationEvent event;
    event.kind = RawInputEventKind::Finished;
    event.source_classification =
        PopulationSourceClassification::Ineligible;
    event.raw_input_id =
        k.input_unit_clause_count + k.input_nonunit_clause_count;
    k.population_observer->observe(event);
  }
}

void attach_replayed_input_proof(Kernel &current, Proof &proof) {
  assert(!current.proof && current.startup_certificate_open);
  // A proof attached after proofless input cannot reconstruct the discarded
  // binary-ID archive. Streaming PBP remains sound by falling back to
  // deletion-by-specification when it encounters that sentinel.
  assert(!proof.lrat() || current.binary_ids.tracks_values());
  // Replay the checker's exact input, not the clauses left after eager
  // proofless unit propagation. The C API may keep adding original clauses
  // after proof selection, so LRAT-derived ids must follow all input ids.
  Kernel replay;
  replay.proof = &proof;
  replay.population_observer = current.population_observer;
  // The replay reconstructs checker identities for input already observed by
  // the live kernel.  It must not claim a second raw stream or a second solver
  // population; Proof events and the explicit rebinds below remain visible.
  replay.population_observe_raw_input = false;
  replay.population_observe_solver = false;
  proof.bind_variable_map(&replay.internal_to_external);
  proof.bind_external_variable_map(&replay.external_to_internal);
  kernel_sync_population_observer(replay);
  std::vector<Lit> clause;
  for (const int32_t external : current.startup_input) {
    if (external) {
      clause.push_back(internalize(external));
    } else {
      ingest_clause(replay, clause);
      clause.clear();
    }
  }
  assert(clause.empty());
  finish_input(replay);
  if (proof.lrat() && !replay.inconsistent)
    propagate_service(replay);

  if (proof.lrat() && !replay.inconsistent) {
    // The eager import may have stored a smaller image than native LRAT's
    // deferred import.  Original clause ids remain in the checker, so prove
    // each live image from its raw clause and the replay's named root units.
    // Re-point only proof identities; watches, assignments, reasons and H
    // remain those of the existing solver session.
    struct Live {
      uint64_t key;
      Ref ref;
      std::vector<Lit> lits;
    };
    std::unordered_map<uint64_t, Live> live;
    live.reserve(current.binary_ids.size() +
                 current.live_irredundant_large);
    current.binary_ids.for_each([&](uint64_t key, uint64_t id) {
      live.emplace(id, Live{key, INVALID_REF,
                            {static_cast<Lit>(key >> 32),
                             static_cast<Lit>(key)}});
    });
    for (Ref ref = 0; ref < current.arena.size();) {
      Clause *c = clause_at(current.arena, ref);
      if (!c->garbage())
        live.emplace(c->id(), Live{0, ref,
                                   std::vector<Lit>(c->begin(), c->end())});
      ref += clause_actual_words(c);
    }
    uint64_t raw_id = 0;
    std::vector<Lit> raw;
    std::vector<uint64_t> chain;
    for (const int32_t external : current.startup_input) {
      if (external) {
        raw.push_back(internalize(external));
        continue;
      }
      const auto found = live.find(++raw_id);
      if (found != live.end()) {
        Live &entry = found->second;
        auto normalized = entry.lits;
        auto original = raw;
        std::sort(normalized.begin(), normalized.end());
        std::sort(original.begin(), original.end());
        if (normalized != original) {
          chain.clear();
          for (const Lit lit : raw) {
            if (std::find(entry.lits.begin(), entry.lits.end(), lit) !=
                entry.lits.end())
              continue;
            const Lit unit = negate(lit);
            assert(replay.values[unit] > 0);
            const uint64_t uid = replay.unit_ids[var_of(unit)];
            assert(uid);
            if (std::find(chain.begin(), chain.end(), uid) == chain.end())
              chain.push_back(uid);
          }
          chain.push_back(raw_id);
          const uint64_t replacement = ++replay.next_clause_id;
          proof.add_rup(replacement, entry.lits.data(),
                        static_cast<uint32_t>(entry.lits.size()), chain.data(),
                        static_cast<uint32_t>(chain.size()));
          proof.observe_checker_id_rebind(raw_id, replacement,
                                          entry.lits.data(),
                                          static_cast<uint32_t>(
                                              entry.lits.size()));
          if (entry.ref != INVALID_REF)
            clause_at(current.arena, entry.ref)->set_id(replacement);
          else
            current.binary_ids.insert_or_assign_known(entry.key,
                                                       replacement, false);
        }
      }
      raw.clear();
    }
    assert(raw.empty() && raw_id == current.next_clause_id);
    for (Var v = 0; v < current.nvars; v++) {
      if (current.values[make_lit(v, false)] == 0)
        continue;
      assert(replay.values[make_lit(v, false)] ==
             current.values[make_lit(v, false)]);
      const uint64_t predecessor = current.unit_ids[v];
      current.unit_ids[v] = replay.unit_ids[v];
      assert(current.unit_ids[v]);
      if (predecessor != current.unit_ids[v]) {
        const Lit unit = make_lit(v, current.values[make_lit(v, false)] < 0);
        proof.observe_checker_id_rebind(predecessor, current.unit_ids[v],
                                        &unit, 1);
      }
    }
  } else if (proof.dsr()) {
    current.dsr_root_values = replay.dsr_root_values;
  }
  current.next_clause_id = std::max(current.next_clause_id,
                                    replay.next_clause_id);
  if (replay.inconsistent) {
    // The replay already published the complete input refutation.  Do not
    // let the ordinary finalizer publish it a second time with stale ids.
    current.inconsistent = true;
    current.input_refuted = false;
    current.input_refutation_units.clear();
  }
  current.proof = &proof;
  proof.bind_variable_map(&current.internal_to_external);
  proof.bind_external_variable_map(&current.external_to_internal);
  kernel_sync_population_observer(current);
  if (!proof.startup_pb_available())
    current.close_startup_certificate();
  current.replay_lrat_input = false;
}

const char *parse_dimacs_with_block(FILE *f, Kernel &k, size_t block_bytes) {
  assert(block_bytes > 0);
  Proof *deferred_lrat = k.proof && k.proof->lrat() ? k.proof : nullptr;
  if (deferred_lrat) {
    assert(k.next_clause_id == 0 && k.startup_input.empty());
    k.proof = nullptr;
    k.replay_lrat_input = true;
    k.startup_certificate_open = true;
  }
  const auto parse_error = [&](const char *reason) {
    if (deferred_lrat) {
      k.proof = deferred_lrat;
      k.replay_lrat_input = false;
    }
    return reason;
  };
  std::vector<Lit> scratch;
  ParseMemoryTrace memory_trace;
  memory_trace.emit(k, "begin");
  struct ScopedInputBinaryLiveness {
    Kernel &kernel;
    std::unique_ptr<BinaryLivenessIndex> index;
    explicit ScopedInputBinaryLiveness(Kernel &k) : kernel(k) {}
    ~ScopedInputBinaryLiveness() {
      if (index) {
        assert(kernel.binary_liveness == index.get());
        kernel.binary_liveness = nullptr;
      }
    }
    void arm(size_t entries, uint32_t literals, uint64_t declared_clauses) {
      if (index) {
        assert(kernel.binary_liveness == index.get());
        // A repeated advisory header may have enlarged the literal universe.
        // Keep the owner and rebuild its filter, not an empty replacement.
        grow_input_binary_liveness(kernel,
                                  std::max(entries, kernel.binary_ids.size()));
        return;
      }
      assert(!kernel.binary_liveness);
      index = std::make_unique<BinaryLivenessIndex>();
      index->reserve_bounded_exact(entries, literals, declared_clauses);
      kernel.binary_liveness = index.get();
      // Headerless clauses may precede the first advisory header.
      if (kernel.binary_ids.size())
        grow_input_binary_liveness(kernel,
                                  std::max(entries, kernel.binary_ids.size()));
    }
  } input_liveness(k);
  // Parsing is single-threaded and owns `f`. Scan one block at a time with the
  // cursor and limit held in locals, so the whitespace and digit loops stay
  // register-resident and stdio is entered once per block, never per byte.
  // A token may straddle a block boundary: every scanning loop refills before
  // it concludes that the input ended.
  std::unique_ptr<unsigned char[]> block(new unsigned char[block_bytes]);
  const unsigned char *p = block.get();
  const unsigned char *e = p;
  const auto refill = [&]() -> bool {
    const size_t got = std::fread(block.get(), 1, block_bytes, f);
    p = block.get();
    e = p + got;
    return got != 0;
  };
  const auto is_space = [](unsigned char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
  };
  const auto is_digit = [](unsigned char ch) { return ch >= '0' && ch <= '9'; };
  // Leaves p at the first non-blank byte; false means the input ended.
  const auto skip_space = [&]() -> bool {
    for (;;) {
      while (p != e && is_space(*p))
        ++p;
      if (p != e)
        return true;
      if (!refill())
        return false;
    }
  };
  // Leaves p at the next '\n' (unconsumed) or at the end of input.
  const auto skip_line = [&]() {
    for (;;) {
      while (p != e && *p != '\n')
        ++p;
      if (p != e || !refill())
        return;
    }
  };
  // Accumulates the digit run at p into n, checking the 2^28 bound after every
  // digit when bounded; false reports that bound. Leaves p at the first
  // non-digit or at the end of input.
  const auto read_digits = [&](uint64_t &n, bool bounded) -> bool {
    for (;;) {
      while (p != e && is_digit(*p)) {
        n = n * 10 + static_cast<uint64_t>(*p - '0');
        if (bounded && n > (1u << 28))
          return false;
        ++p;
      }
      if (p != e || !refill())
        return true;
    }
  };
  for (;;) {
    if (!skip_space())
      break;
    const unsigned char c = *p;
    if (c == 'c') { // comment line
      skip_line();
      continue;
    }
    if (c == 'p') { // header: "p cnf <vars> <clauses>" (counts advisory)
      ++p;
      // The first digit run on the line is the declared variable count.
      for (;;) {
        while (p != e && *p != '\n' && !is_digit(*p))
          ++p;
        if (p != e || !refill())
          break;
      }
      if (p != e && is_digit(*p)) {
        uint64_t n = 0;
        if (!read_digits(n, true))
          return parse_error("variable count out of range");
        kernel_reserve_vars(k, static_cast<uint32_t>(n));
        for (;;) {
          while (p != e && (*p == ' ' || *p == '\t' || *p == '\r'))
            ++p;
          if (p != e || !refill())
            break;
        }
        uint64_t clauses = 0;
        read_digits(clauses, false);
        // Avoid repeated multi-gigabyte rehashes on binary-dominated inputs.
        // Binary encodings commonly contain almost nothing else.  Reserve up
        // to the declared clause count, capped by twice the variable
        // population so a huge long-clause input does not reserve a giant
        // binary table it will never use.  Two binaries per declared
        // variable is a deliberately non-binding prediction: the table
        // still grows exactly when an input exceeds it, while the former
        // three-times estimate needlessly crossed a power-of-two boundary on
        // large circuit encodings.
        const uint64_t predicted_binaries =
            std::min<uint64_t>(clauses, 2 * n);
        // The direct finite-domain startup rule needs the original order of
        // unique binary constraints to reproduce its owning algorithm's
        // deterministic seed. Keep that order only inside the rule's bounded
        // admission envelope; the first startup opportunity releases it.
        k.input_binary_order_captured =
            n <= strategy::OnehotModelParameters::MAXIMUM_VARIABLES_DEFAULT &&
            clauses <=
                strategy::OnehotModelParameters::MAXIMUM_CONSTRAINTS_DEFAULT;
        if (k.input_binary_order_captured)
          k.input_binary_order.reserve(static_cast<size_t>(clauses));
        k.input_clause_tail.reserve(Kernel::INPUT_CLAUSE_TAIL_CAPACITY);
        // Native LRAT retains exact binary proof identities. Streaming PBP,
        // DRAT and proofless solving instead use a compact no-false-negative
        // filter backed by exact mirrored-watch confirmation during input.
        // PBP deletes binaries by specification and retains exact identities
        // only for non-binary clauses. The filter grows by load, never by
        // formula identity or a size gate.
        if ((!k.proof || !k.proof->needs_clause_ids()) &&
            !k.replay_lrat_input) {
          k.binary_ids.discard_storage();
          input_liveness.arm(static_cast<size_t>(predicted_binaries),
                             2 * k.nvars, clauses);
        } else {
          k.binary_ids.reserve(static_cast<size_t>(predicted_binaries));
        }
        memory_trace.emit(k, "header");
        skip_line();
      }
      continue;
    }
    // a literal
    bool neg = false;
    if (c == '-') {
      neg = true;
      ++p;
      if (p == e && !refill())
        return parse_error("expected literal");
    }
    if (!is_digit(*p))
      return parse_error("expected literal");
    uint64_t n = 0;
    if (!read_digits(n, true))
      return parse_error("literal out of range");
    if (p != e && !is_space(*p))
      return parse_error("unexpected character after literal");
    if (n == 0) {
      ingest_clause(k, scratch);
      scratch.clear();
      memory_trace.maybe(k);
    } else if (neg) {
      scratch.push_back(internalize(-static_cast<int>(n)));
    } else {
      scratch.push_back(internalize(static_cast<int>(n)));
    }
  }
  if (!scratch.empty()) // tolerate a missing final 0
    ingest_clause(k, scratch);
  memory_trace.emit(k, "finish-begin");
  if (k.proof && k.proof->streaming_pbp() &&
      k.binary_ids.tracks_values())
    k.proof->declare_exact_binary_ids();
  finish_input(k);
  if (deferred_lrat)
    attach_replayed_input_proof(k, *deferred_lrat);
  memory_trace.emit(k, "finish-end");
  return nullptr;
}

const char *parse_dimacs(FILE *f, Kernel &k) {
  return parse_dimacs_with_block(f, k, size_t{1} << 20);
}

} // namespace tactix
