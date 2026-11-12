#include "frontend/parse.hpp"

#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/rule_parameters.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <unordered_map>

namespace tactix {

namespace {

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

// N2 input identity: the checker numbers raw file clauses by position and
// deletes by literal multiset, so publish the exact narrowed clause and retire
// its raw predecessor immediately (reports/2026-08-05-input-clause-normalization-n2.md §2).
static void restate_input(Kernel &k, uint64_t id, const std::vector<Lit> &lits,
                          const std::vector<Lit> &original) {
  // The count is the reachability probe; the restatement needs a proof.
  k.stats.input_restated++;
  if (!k.proof)
    return;
  assert(!original.empty());
  k.proof->add_rup(id, lits.data(), static_cast<uint32_t>(lits.size()));
  k.proof->del(k.next_clause_id, id, original.data(),
               static_cast<uint32_t>(original.size()));
}

// Kissat propagates every new input unit immediately (`kissat_search_propagate`,
// internal.c:363), charging both `ticks` and `search_ticks`.
static void propagate_input_unit(Kernel &k) {
  const bool conflict = kernel_propagate(k);
  if (!conflict)
    return;
  if (k.proof)
    k.proof->add_empty(0);
  k.inconsistent = true;
}

// Activation, kissat's `tumble` order (heuristics_seed_activation_order), of
// the normalized clause's unassigned literals in clause order.
template <class Lits>
[[gnu::always_inline]] static inline void
activate_input_literals(Kernel &k, const Lits &lits) {
  if (k.input_activated.size() < k.nvars)
    k.input_activated.resize(k.nvars, 0);
  for (const Lit lit : lits) {
    const Var v = var_of(lit);
    assert(k.values[make_lit(v, false)] == 0);
    if (!k.input_activated[v]) {
      k.input_activated[v] = 1;
      if (k.tumble)
        k.input_activation_order.push_back(v);
    }
  }
}

// Store the normalized input binary (a, b) unless already live; returns
// whether stored. A duplicate consumes its input id only; `raw` is its checker
// copy. A no-false-negative filter is confirmed against the mirrored watches.
[[gnu::always_inline]] static inline bool
ingest_input_binary(Kernel &k, uint64_t id, Lit a, Lit b,
                    const std::vector<Lit> &raw,
                    RawInputObservation &observation) {
  const uint64_t key = kernel_binary_key(a, b);
  const bool duplicate = k.binary_ids.tracks_keys()
                             ? k.binary_ids.count(key)
                             : kernel_binary_live(k, a, b);
  if (duplicate) {
    observation.note(RawDropped | RawDuplicate);
    // Each raw duplicate still exists in the checker. Keep exactly the
    // one copy represented by the kernel so a later deletion removes it.
    if (k.proof)
      k.proof->del(k.next_clause_id, id, raw.data(),
                   static_cast<uint32_t>(raw.size()));
    return false;
  }
  if (!k.binary_ids.tracks_keys() && k.binary_liveness &&
      k.binary_liveness->needs_growth(k.binary_ids.size() + 1))
    grow_input_binary_liveness(k, k.binary_ids.size() + 1);
  kernel_add_new_binary_clause(k, a, b, id);
  observation.note(RawAdmitted);
  if (k.input_binary_order_captured)
    k.input_binary_order.push_back(key);
  // Kissat's original-clause constructor checks vector defragmentation only
  // after the complete clause has joined the watch database.
  k.watches.defrag_if_needed();
  return true;
}

[[gnu::noinline]] static void ingest_clause_generic(Kernel &k,
                                                    std::vector<Lit> &scratch) {
  // Every input clause consumes one proof id in input order, including
  // tautologies and duplicates that are never stored.
  const uint64_t id = ++k.next_clause_id;
  // Headerless C API input: grow the identity map before recording the
  // original clause (the proof shadow translates through it).
  Var maxv = 0;
  for (const Lit l : scratch)
    maxv = std::max(maxv, var_of(l));
  if (!scratch.empty()) {
    // A literal beyond the header ends the fixed-universe bitmap's scope:
    // rebuild the ordinary filter from the exact live watches.
    if (maxv >= k.nvars && k.binary_liveness && k.binary_liveness->exact())
      grow_input_binary_liveness(k, k.binary_ids.size());
    kernel_reserve_vars(k, maxv + 1);
  }
  kernel_sync_population_observer(k);
  RawInputObservation observation(k, id, scratch);
  capture_raw_input_shape(k, scratch);
  // Seed the diagnostic proof shadow from the checker's actual population
  // (every file clause, tautologies and duplicates included).
  if (k.proof)
    k.proof->seed_population(id, scratch.data(),
                             static_cast<uint32_t>(scratch.size()));

  // Kissat `tumble=1` imports variables by first occurrence and activates
  // surviving clauses' literals in file order (internal.c `kissat_add` ->
  // flags.c `kissat_activate_literals`); record imports over raw literals.
  const size_t file_size = scratch.size();
  // Capture only clauses that actually change, before the first in-place
  // rewrite. Unchanged input carries no extra allocation or literal copy.
  std::vector<Lit> original;
  const auto retain_original = [&] {
    if (k.proof && original.empty())
      original = scratch;
  };
  const auto retire_unstored = [&] {
    if (!k.proof)
      return;
    retain_original();
    const auto &raw = original.empty() ? scratch : original;
    k.proof->del(k.next_clause_id, id, raw.data(),
                 static_cast<uint32_t>(raw.size()));
  };
  for (const Lit lit : scratch)
    kernel_record_import(k, var_of(lit));

  // Set semantics: dedupe in place in first-occurrence file order (as Kissat
  // stores it); tautologies contribute nothing.
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
  // Kissat internal.c:kissat_add normalizes against units fixed while
  // parsing; new units propagate immediately.
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
    } else
      *out++ = lit;
  }
  scratch.erase(out, scratch.end());
  observation.normalized_available();

  // N2: a shrunk clause may not keep the checker's `id`; restate the exact
  // stored clause, only when it is stored.
  const bool shrunk = scratch.size() < file_size;

  if (k.inconsistent) {
    observation.note(RawDropped | RawAlreadyInconsistent);
    return; // already refuted; further clauses don't matter
  }

  // Activation in kissat's `tumble` order (heuristics_seed_activation_order),
  // after the tautology return as in `kissat_add`; input_activated skips
  // repeats and earlier-activated variables.
  activate_input_literals(k, scratch);

  if (scratch.empty()) { // empty input clause: immediate refutation
    observation.note(RawAdmitted | RawEmpty |
                     (file_size ? RawRootShortened : RawNone));
    k.inconsistent = true;
    k.input_refuted = true;
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
      return;
    }
    k.unit_ids[var_of(u)] = id;
    observation.note(RawAdmitted);
    if (shrunk)
      restate_input(k, id, scratch, original);
    kernel_assign_unit(k, u);
    propagate_input_unit(k);
  } else if (scratch.size() == 2) {
    observation.note(RawBinary);
    if (ingest_input_binary(k, id, scratch[0], scratch[1],
                            original.empty() ? scratch : original,
                            observation) &&
        shrunk)
      restate_input(k, id, scratch, original);
  } else {
    observation.note(RawAdmitted | RawLarge);
    kernel_add_large_clause(k, scratch.data(),
                            static_cast<uint32_t>(scratch.size()), false, 0, id);
    k.watches.defrag_if_needed();
    if (shrunk)
      restate_input(k, id, scratch, original);
  }
}

void declare_input(Kernel &k, uint32_t variables, uint64_t clauses) {
  kernel_reserve_vars(k, variables);
  k.input_header = {true, variables, clauses};
  // The direct finite-domain startup rule keeps the original unique-binary
  // order inside its bounded admission envelope.
  k.input_binary_order_captured =
      variables <= strategy::OnehotModelParameters::MAXIMUM_VARIABLES_DEFAULT &&
      clauses <= strategy::OnehotModelParameters::MAXIMUM_CONSTRAINTS_DEFAULT;
  if (k.input_binary_order_captured)
    k.input_binary_order.reserve(static_cast<size_t>(clauses));
  k.input_clause_tail.reserve(Kernel::INPUT_CLAUSE_TAIL_CAPACITY);
}

void ingest_clause(Kernel &k, std::vector<Lit> &scratch) {
  // A small frame for the ordinary normalized binary path when both variables
  // are reserved and neither literal has a root value.
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
      const Lit pair[2] = {a, b};
      activate_input_literals(k, pair);
      ingest_input_binary(k, id, a, b, scratch, observation);
      return;
    }
  }
  ingest_clause_generic(k, scratch);
}

void finish_input(Kernel &k) {
  kernel_sync_population_observer(k);
  // Reserve trail and frames once, so assignment never grows a vector.
  k.trail.reserve(k.nvars);
  k.frames.reserve(static_cast<size_t>(k.nvars) + 1);
  // Input ids are settled. An unassigned variable absent from every surviving
  // clause is outside Kissat's active population: retire it without a witness
  // (either value extends a model), keeping its import rank.
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
  // Binaries are named by literals after import: release the key archive;
  // size_ remains the monotone watermark.
  k.binary_ids.discard_storage();
  if (k.proof && k.proof->population_shadow_available())
    k.proof->arm_population_shadow();
  // Publish the activation population to `H` now, or keep it until
  // ScopedHeuristics attaches; seeding combines it with first-import ranks
  // (Kissat samples compact `VARS`).
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
    if (k.proof)
      k.proof->add_empty(++k.next_clause_id);
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

void ingest_dimacs(Kernel &k, const std::vector<int32_t> &literals) {
  std::vector<Lit> clause;
  for (const int32_t external : literals) {
    if (external) {
      clause.push_back(internalize(external));
    } else {
      ingest_clause(k, clause);
      clause.clear();
    }
  }
  assert(clause.empty());
  finish_input(k);
}

void attach_replayed_input_proof(Kernel &current, Proof &proof) {
  assert(!current.proof && current.startup_certificate_open);
  // Replay the checker's exact input, not the clauses left after eager proofless
  // unit propagation.
  Kernel replay;
  replay.startup_certificate_open = false; // current holds the raw input
  replay.proof = &proof;
  replay.population_observer = current.population_observer;
  // The replay reconstructs checker identities without a second raw stream
  // or solver population.
  replay.population_observe_raw_input = false;
  replay.population_observe_solver = false;
  proof.bind_variable_map(&replay.internal_to_external);
  proof.bind_external_variable_map(&replay.external_to_internal);
  kernel_sync_population_observer(replay);
  ingest_dimacs(replay, current.startup_input);
  // N2: the root units the replay wrote are the ones a proof attached before the
  // input would have recorded, so the live kernel's root view must hold them too.
  current.dsr_root_values = std::move(replay.dsr_root_values);
  current.dsr_root_proof = replay.dsr_root_proof;
  current.dsr_root_stream_identity = replay.dsr_root_stream_identity;
  if (!current.dsr_root_values.empty())
    current.dsr_root_values.resize(current.nvars, 0);
  current.next_clause_id = std::max(current.next_clause_id,
                                    replay.next_clause_id);
  if (replay.inconsistent) {
    // The replay already published the complete input refutation.  Do not
    // let the ordinary finalizer publish it a second time with stale ids.
    current.inconsistent = true;
    current.input_refuted = false;
  }
  // Replayed root units take fresh ids, so a live id may name another
  // constraint: delete by specification.
  proof.disable_pbp_id_deletions();
  current.proof = &proof;
  proof.bind_variable_map(&current.internal_to_external);
  proof.bind_external_variable_map(&current.external_to_internal);
  kernel_sync_population_observer(current);
  if (!proof.startup_pb_available())
    current.close_startup_certificate();
}

const char *parse_dimacs_with_block(FILE *f, Kernel &k, size_t block_bytes) {
  assert(block_bytes > 0);
  std::vector<Lit> scratch;
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
  // Scan one block at a time with locals for cursor and limit; every scanning
  // loop refills before concluding that the input ended.
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
  // Accumulates the digit run at p into n, checking the 2^28 bound per digit
  // when bounded (false reports it); leaves p at the first non-digit.
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
          return "variable count out of range";
        for (;;) {
          while (p != e && (*p == ' ' || *p == '\t' || *p == '\r'))
            ++p;
          if (p != e || !refill())
            break;
        }
        uint64_t clauses = 0;
        read_digits(clauses, false);
        declare_input(k, static_cast<uint32_t>(n), clauses);
        // Reserve for binaries up to the declared clause count, capped at two
        // per declared variable; the table still grows when exceeded.
        const uint64_t predicted_binaries =
            std::min<uint64_t>(clauses, 2 * n);
        // Import uses a compact no-false-negative filter backed by exact
        // mirrored-watch confirmation; PBP deletes binaries by specification.
        k.binary_ids.discard_storage();
        input_liveness.arm(static_cast<size_t>(predicted_binaries),
                           2 * k.nvars, clauses);
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
        return "expected literal";
    }
    if (!is_digit(*p))
      return "expected literal";
    uint64_t n = 0;
    if (!read_digits(n, true))
      return "literal out of range";
    if (p != e && !is_space(*p))
      return "unexpected character after literal";
    if (n == 0) {
      ingest_clause(k, scratch);
      scratch.clear();
    } else if (neg) {
      scratch.push_back(internalize(-static_cast<int>(n)));
    } else {
      scratch.push_back(internalize(static_cast<int>(n)));
    }
  }
  if (!scratch.empty()) // tolerate a missing final 0
    ingest_clause(k, scratch);
  finish_input(k);
  return nullptr;
}

const char *parse_dimacs(FILE *f, Kernel &k) {
  return parse_dimacs_with_block(f, k, size_t{1} << 20);
}

} // namespace tactix
