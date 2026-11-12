#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include "rules/substitution_redundancy/clause_multiset.hpp"
#include "rules/substitution_redundancy/cut_orbit_index.hpp"
#include "rules/substitution_redundancy/detection_support.hpp"
#include "rules/substitution_redundancy/literal_permutation.hpp"

#include "symmetry_backend/dejavu_backend.hpp"
#include "symmetry_backend/sparse_permutation.hpp"
#include "diagnostics/symmetry_trace.hpp"
#include "diagnostics/formula_fingerprint.hpp"
#include "diagnostics/graph_capture.hpp"
#include "kernel/splitmix64.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <vector>

namespace tactix {
namespace {

using sr_detail::backend_stop;
using sr_detail::fail;
using sr_detail::finish;
using sr_detail::LiteralGraph;
using sr_detail::saturating_add;
using sr_detail::stop_requested;
using sr_detail::StopContext;

struct FormulaSnapshot {
  SymmetryClauseMultiset clause_index;
  std::vector<uint32_t> occurrences;
};

uint64_t saturating_twice(uint64_t value) {
  return value > UINT64_MAX / 2 ? UINT64_MAX : 2 * value;
}

// Upper bound on one sparse generator's exact check: clause count plus literal
// domain plus twice the literal occurrences. Discovery admits only generators
// it can validate and reserves one such quantum for group planning.
uint64_t generator_validation_upper_bound(const FormulaSnapshot &snapshot,
                                          uint64_t literal_count) {
  uint64_t support = 0;
  for (const auto &clause : snapshot.clause_index.clauses())
    support = saturating_add(support, clause.size());
  return saturating_add(
      saturating_add(snapshot.clause_index.clauses().size(), literal_count),
      saturating_twice(support));
}

template <class T>
void apply_order(std::vector<T> &values, SymmetryTraversalOrder order,
                 uint64_t seed) {
  if (order == SymmetryTraversalOrder::Reverse) {
    std::reverse(values.begin(), values.end());
    return;
  }
  if (order != SymmetryTraversalOrder::Seeded)
    return;
  for (size_t remaining = values.size(); remaining > 1; --remaining) {
    const size_t selected = splitmix64_next(seed) % remaining;
    std::swap(values[remaining - 1], values[selected]);
  }
}

struct EligibleClause {
  std::vector<Lit> literals;
  bool amo = false;
};

struct GeneratorFixingStats {
  uint64_t negation_units = 0;
  uint64_t binary_units = 0;
  bool visitor_stopped = false;
};

// The kernel, stop flag and result of one pass, charged against `parameters`.
struct SymmetryPass {
  const Kernel &kernel;
  const SubstitutionRedundancyParameters &parameters;
  volatile sig_atomic_t *stop;
  SymmetryEnumerationResult &result;

  bool charge(uint64_t amount) const;
  bool snapshot_formula(FormulaSnapshot &snapshot) const;
  bool build_symmetry_graph(const FormulaSnapshot &snapshot,
                            LiteralGraph &storage) const;
  bool exact_automorphism(const FormulaSnapshot &snapshot,
                          const std::vector<Lit> &mapping) const;
  bool exact_sparse_automorphism(const FormulaSnapshot &snapshot,
                                 const SparseLiteralPermutation &mapping,
                                 SymmetryClauseVisitWorkspace &workspace,
                                 bool generator) const;
  bool eligible_clauses(const FormulaSnapshot &snapshot,
                        std::vector<EligibleClause> &clauses) const;
  bool generator_fixing_scans(
      const FormulaSnapshot &snapshot,
      const std::vector<SparseLiteralPermutation> &generators,
      const OrbitopalInstanceVisitor &visitor,
      GeneratorFixingStats &stats) const;
};

bool SymmetryPass::charge(uint64_t amount) const {
  if (stop_requested(kernel, stop))
    return fail(result, SymmetryEnumerationStatus::Interrupted);
  if (result.ticks >= parameters.maximum_detection_ticks ||
      amount > parameters.maximum_detection_ticks - result.ticks) {
    result.ticks = parameters.maximum_detection_ticks;
    return fail(result, SymmetryEnumerationStatus::LimitReached);
  }
  result.ticks += amount;
  return true;
}

bool SymmetryPass::snapshot_formula(FormulaSnapshot &snapshot) const {
  const uint64_t literal_vertices = 2ull * kernel.nvars;
  if (literal_vertices > static_cast<uint64_t>(std::numeric_limits<int>::max()))
    return fail(result, SymmetryEnumerationStatus::LimitReached);
  std::vector<std::vector<Lit>> clauses;
  snapshot.occurrences.assign(literal_vertices, 0);
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    for (const WatchEntry watch : kernel.watches.entries(literal)) {
      if (!charge(1))
        return false;
      if (watch.binary() && literal < watch.other())
        clauses.push_back({literal, watch.other()});
    }
  for (const auto [ref, clause] : arena_records(kernel.arena)) {
    if (!clause->garbage()) {
      if (!charge(clause->size))
        return false;
      clauses.emplace_back(clause->begin(), clause->end());
    }
  }
  for (Var var = 0; var < kernel.nvars; ++var) {
    if (!charge(1))
      return false;
    const Lit positive = make_lit(var, false);
    if (kernel.value(positive)) {
      clauses.push_back(
          {kernel.value(positive) > 0 ? positive : negate(positive)});
    }
  }
  for (const auto &clause : clauses) {
    for (const Lit literal : clause)
      ++snapshot.occurrences[literal];
  }
  snapshot.clause_index.build(std::move(clauses));
  return true;
}

bool SymmetryPass::build_symmetry_graph(const FormulaSnapshot &snapshot,
                                        LiteralGraph &storage) const {
  const GraphSymmetryParameters &settings = parameters.graph_symmetry;
  // A uniquely colored assigned literal fixes its unit clause, so that leaf is
  // omitted; the full snapshot stays the exact-check oracle.
  const auto fixed_unit = [&](const std::vector<Lit> &clause) {
    return settings.pointwise_root_stabilizer && clause.size() == 1 &&
           kernel.value(clause.front());
  };
  uint64_t fixed_units = 0;
  uint64_t binaries = 0, nonbinary = 0, support = 0,
           nonbinary_support = 0;
  for (const auto &clause : snapshot.clause_index.clauses()) {
    if (fixed_unit(clause)) {
      ++fixed_units;
      continue;
    }
    if (support > UINT64_MAX - clause.size())
      return fail(result, SymmetryEnumerationStatus::ResourceExhausted);
    support += clause.size();
    if (clause.size() == 2)
      ++binaries;
    else {
      ++nonbinary;
      nonbinary_support += clause.size();
    }
  }
  bool direct_binary_edges = false;
  if (settings.binary_graph_mode == SymmetryBinaryGraphMode::DirectEdges) {
    direct_binary_edges = true;
  } else if (settings.binary_graph_mode ==
             SymmetryBinaryGraphMode::Automatic) {
    const uint64_t threshold =
        static_cast<uint64_t>(kernel.nvars) *
        settings.binary_graph_density_permille;
    const uint64_t required =
        threshold / 1000 + static_cast<uint64_t>(threshold % 1000 != 0);
    direct_binary_edges = binaries >= required;
  }

  const uint64_t literal_vertices = 2ull * kernel.nvars;
  const auto over_size_limits = [&](uint64_t vertices, uint64_t edges) {
    const uint64_t hard_edge_limit = std::min(
        {settings.maximum_graph_edges,
         static_cast<uint64_t>(std::numeric_limits<int>::max()),
         static_cast<uint64_t>(SIZE_MAX / (2 * sizeof(uint32_t)))});
    return vertices > settings.maximum_graph_vertices ||
           vertices > static_cast<uint64_t>(std::numeric_limits<int>::max()) ||
           edges > hard_edge_limit;
  };

  // The backend graph must be simple: a repeated binary becomes a degree-two
  // clause vertex. The repeat-free size is admitted before the classification
  // scan, which charges one tick per binary clause.
  std::vector<bool> repeated_binary;
  uint64_t repeated_binaries = 0;
  if (direct_binary_edges) {
    const uint64_t minimum_vertices =
        3ull * kernel.nvars + nonbinary;
    const uint64_t minimum_edges =
        2ull * kernel.nvars + binaries + nonbinary_support;
    if (over_size_limits(minimum_vertices, minimum_edges))
      return fail(result, SymmetryEnumerationStatus::LimitReached);
    if (!charge(binaries))
      return false;
    const auto &clauses = snapshot.clause_index.clauses();
    repeated_binary.assign(clauses.size(), false);
    std::unordered_set<uint64_t> direct;
    direct.reserve(static_cast<size_t>(binaries));
    uint64_t scanned = 0;
    for (size_t i = 0; i < clauses.size(); ++i) {
      if (clauses[i].size() != 2)
        continue;
      if (!(++scanned & 1023u) && !charge(0))
        return false;
      const Lit first = std::min(clauses[i][0], clauses[i][1]);
      const Lit second = std::max(clauses[i][0], clauses[i][1]);
      if (!direct.insert((static_cast<uint64_t>(first) << 32) | second)
               .second) {
        repeated_binary[i] = true;
        ++repeated_binaries;
      }
    }
  }

  const uint64_t negation_vertices =
      direct_binary_edges ? kernel.nvars : 0;
  const uint64_t clause_vertices =
      direct_binary_edges
          ? nonbinary + repeated_binaries
          : snapshot.clause_index.clauses().size() - fixed_units;
  if (literal_vertices > UINT64_MAX - negation_vertices ||
      literal_vertices + negation_vertices > UINT64_MAX - clause_vertices)
    return fail(result, SymmetryEnumerationStatus::ResourceExhausted);
  const uint64_t vertex_count =
      literal_vertices + negation_vertices + clause_vertices;
  const uint64_t negation_edges =
      direct_binary_edges ? 2ull * kernel.nvars : kernel.nvars;
  const uint64_t clause_edges =
      direct_binary_edges
          ? binaries + repeated_binaries + nonbinary_support
          : support;
  if (negation_edges > UINT64_MAX - clause_edges)
    return fail(result, SymmetryEnumerationStatus::ResourceExhausted);
  const uint64_t edge_count = negation_edges + clause_edges;
  if (over_size_limits(vertex_count, edge_count) ||
      !charge(edge_count)) {
    if (result.status == SymmetryEnumerationStatus::Complete)
      result.status = SymmetryEnumerationStatus::LimitReached;
    return false;
  }

  storage.reset(kernel, settings.pointwise_root_stabilizer,
                static_cast<uint32_t>(vertex_count), edge_count,
                static_cast<uint32_t>(literal_vertices));
  uint32_t next_vertex = static_cast<uint32_t>(literal_vertices);
  if (direct_binary_edges) {
    for (Var var = 0; var < kernel.nvars; ++var) {
      if (!(var & 1023u) && !charge(0))
        return false;
      storage.add_negation_gadget(var, next_vertex++);
    }
    const auto &clauses = snapshot.clause_index.clauses();
    for (size_t i = 0; i < clauses.size(); ++i) {
      const auto &clause = clauses[i];
      if (fixed_unit(clause))
        continue;
      if (!(storage.edges.size() & 2047u) && !charge(0))
        return false;
      if (clause.size() == 2 && !repeated_binary[i]) {
        storage.add_edge(clause[0], clause[1]);
        continue;
      }
      const uint32_t vertex = next_vertex++;
      for (const Lit literal : clause)
        storage.add_edge(literal, vertex);
    }
  } else {
    for (Var var = 0; var < kernel.nvars; ++var)
      storage.add_edge(make_lit(var, false), make_lit(var, true));
    for (const auto &clause : snapshot.clause_index.clauses()) {
      if (fixed_unit(clause))
        continue;
      if (!(storage.edges.size() & 2047u) && !charge(0))
        return false;
      const uint32_t vertex = next_vertex++;
      for (const Lit literal : clause)
        storage.add_edge(literal, vertex);
    }
  }
  if (next_vertex != storage.vertex_count ||
      storage.edges.size() != 2 * storage.edge_count)
    return fail(result, SymmetryEnumerationStatus::Failed);
  for (const uint32_t degree : storage.degrees)
    if (degree > static_cast<uint32_t>(std::numeric_limits<int>::max()))
      return fail(result, SymmetryEnumerationStatus::ResourceExhausted);
  return true;
}

bool signed_literal_permutation(const Kernel &kernel,
                                const std::vector<Lit> &image) {
  if (image.size() != 2ull * kernel.nvars)
    return false;
  std::vector<uint8_t> seen(image.size(), 0);
  for (Lit literal = 0; literal < image.size(); ++literal) {
    if (image[literal] >= image.size() || seen[image[literal]])
      return false;
    seen[image[literal]] = 1;
    if (image[negate(literal)] != negate(image[literal]))
      return false;
    if (kernel_var_retired(kernel, var_of(literal)) &&
        image[literal] != literal)
      return false;
  }
  return true;
}

// Run one exact snapshot check. A detailed trace records its clause counts,
// prefixed by the moved support of a sparse transversal check.
template <class Check>
bool traced_exact_check(const SymmetryEnumerationResult &result,
                        const size_t *support, Check &&check) {
  const SymmetryTrace trace;
  SymmetryClauseCheckStats stats;
  const bool valid = check(trace.detailed() ? &stats : nullptr);
  if (!trace.detailed())
    return valid;
  if (support)
    trace.event("symmetry_exact_sparse", "support\t%zu\tfixed\t%llu\tmapped\t%llu\tvalid\t%u\tstatus\t%u",
        *support, static_cast<unsigned long long>(stats.fixed_clauses),
        static_cast<unsigned long long>(stats.mapped_clauses), static_cast<unsigned>(valid),
        static_cast<unsigned>(result.status));
  else
    trace.event("symmetry_exact_index",
                "fixed\t%llu\tmapped\t%llu\tvalid\t%u\tstatus\t%u",
                static_cast<unsigned long long>(stats.fixed_clauses),
                static_cast<unsigned long long>(stats.mapped_clauses),
                static_cast<unsigned>(valid), static_cast<unsigned>(result.status));
  return valid;
}

bool SymmetryPass::exact_automorphism(const FormulaSnapshot &snapshot,
                                      const std::vector<Lit> &mapping) const {
  if (!signed_literal_permutation(kernel, mapping))
    return false;
  return traced_exact_check(result, nullptr, [&](SymmetryClauseCheckStats *stats) {
    return snapshot.clause_index.preserves_set_support(
        mapping, [&](uint64_t amount) { return charge(amount); }, stats);
  });
}

// A detector generator keeps an uncharged signed-bijection check and the
// reference accounting; transversal validation charges differently.
bool SymmetryPass::exact_sparse_automorphism(
    const FormulaSnapshot &snapshot, const SparseLiteralPermutation &mapping,
    SymmetryClauseVisitWorkspace &workspace, bool generator) const {
  const auto retired = [&](Var variable) { return kernel_var_retired(kernel, variable); };
  const auto charged = [&](uint64_t amount) {
    return charge(amount);
  };
  if (mapping.domain != 2ull * kernel.nvars ||
      !(generator ? mapping.validate(retired, [](uint64_t) { return true; })
                  : mapping.validate(retired, charged)))
    return false;
  const size_t support = mapping.moved.size();
  return traced_exact_check(result, generator ? nullptr : &support,
                            [&](SymmetryClauseCheckStats *stats) {
    return generator ? snapshot.clause_index.preserves_set_moved_reference(
                           mapping, mapping.moved, workspace, charged, stats)
                     : snapshot.clause_index.preserves_set_moved(
                           mapping, mapping.moved, workspace, charged, stats);
  });
}

bool SymmetryPass::eligible_clauses(const FormulaSnapshot &snapshot,
                                    std::vector<EligibleClause> &clauses) const {
  const GraphSymmetryParameters &settings = parameters.graph_symmetry;
  std::unordered_set<uint64_t> binaries;
  binaries.reserve(snapshot.clause_index.clauses().size());
  for (const auto &clause : snapshot.clause_index.clauses())
    if (clause.size() == 2)
      binaries.insert(kernel_binary_key(clause[0], clause[1]));
  for (auto clause : snapshot.clause_index.clauses()) {
    if (clause.size() < 2)
      continue;
    if (settings.clause_scope == SymmetryClauseScope::UniqueLiteralsOnly &&
        std::any_of(clause.begin(), clause.end(), [&](Lit literal) {
          return snapshot.occurrences[literal] != 1;
        }))
      continue;
    bool amo = true;
    for (size_t first = 0; first < clause.size() && amo; ++first)
      for (size_t second = first + 1; second < clause.size(); ++second) {
        if (!charge(1))
          return false;
        if (!binaries.count(kernel_binary_key(negate(clause[first]),
                                               negate(clause[second])))) {
          amo = false;
          break;
        }
      }
    clauses.push_back({std::move(clause), amo});
  }
  apply_order(clauses, settings.clause_order,
              static_cast<uint64_t>(settings.seed) ^ 0x434c41555345534full);
  if (settings.clause_selection != SymmetryClauseSelection::InputOrder)
    std::stable_sort(
        clauses.begin(), clauses.end(), [&](const auto &first, const auto &second) {
          if (settings.clause_selection ==
                  SymmetryClauseSelection::AmoThenShortest &&
              first.amo != second.amo)
            return first.amo > second.amo;
          return first.literals.size() < second.literals.size();
        });
  uint64_t literal_seed =
      static_cast<uint64_t>(settings.seed) ^ 0x4c49544552414c53ull;
  for (auto &clause : clauses) {
    apply_order(clause.literals, settings.literal_order, literal_seed);
    splitmix64_next(literal_seed);
  }
  return true;
}

// Append the moved points of a dense literal image in source order.
template <class Image>
void append_moved_points(SparseLiteralPermutation &sparse, const Image &image,
                         uint32_t literal_count) {
  for (Lit source = 0; source < literal_count; ++source)
    if (const Lit target = image[source]; source != target)
      sparse.moved.emplace_back(source, target);
}

SubstitutionRedundancyInstance negative_fixing_instance(
    Lit fixed_false, Lit witness_true, const SparseLiteralPermutation &image) {
  SubstitutionRedundancyInstance instance;
  instance.clause = {negate(fixed_false)};
  instance.witness = {negate(fixed_false), witness_true};
  image.positive_moves([&](Lit source, Lit target) {
    if (var_of(source) != var_of(fixed_false) && var_of(source) != var_of(witness_true))
      instance.substitution.emplace_back(source, target);
  });
  return instance;
}

// A binary chain member (pivot ∨ second) witnessed by the inverse transversal
// mapping the representative to `other`; pivot order is semantic in DSR.
// Clausal fixing is (representative ∨ ¬other), an orbit cut is
// (¬representative ∨ other); either witness satisfies every earlier member
// of the level.
SubstitutionRedundancyInstance chain_binary_instance(
    Lit pivot, Lit second, const SparseLiteralPermutation &image) {
  SubstitutionRedundancyInstance instance;
  instance.clause = {pivot, second};
  instance.symmetry_chain_certificate = true;
  image.positive_moves([&](Lit source, Lit target) {
    instance.substitution.emplace_back(source, target);
  });
  return instance;
}

SubstitutionRedundancyInstance symmetry_generator_unit_instance(
    Lit representative, const SparseLiteralPermutation &image) {
  SubstitutionRedundancyInstance instance;
  instance.clause = {representative};
  instance.symmetry_chain_certificate = true;
  // DSR maps the unit pivot to true implicitly; the rest is the witness of an
  // automorphism mapping the representative to its complement.
  image.positive_moves([&](Lit source, Lit target) {
    if (var_of(source) == var_of(representative)) return;
    instance.substitution.emplace_back(source, target);
  });
  return instance;
}

// Large domains avoid a Schreier chain (Satsuma): order exact generators by
// the rarest point of their support, take complement moves first and retire
// every generator touching a new unit, then repeat for binary clauses.
bool SymmetryPass::generator_fixing_scans(
    const FormulaSnapshot &snapshot,
    const std::vector<SparseLiteralPermutation> &generators,
    const OrbitopalInstanceVisitor &visitor,
    GeneratorFixingStats &stats) const {
  const size_t literal_count = 2ull * kernel.nvars;
  std::vector<uint8_t> active(generators.size(), 1);
  std::vector<uint8_t> propagated(literal_count, 0);
  std::vector<uint64_t> incidence;
  std::vector<size_t> order;

  const auto rank_active = [&] {
    incidence.assign(literal_count, 0);
    order.clear();
    for (size_t id = 0; id < generators.size(); ++id) {
      if (!active[id])
        continue;
      order.push_back(id);
      for (const auto &[source, target] : generators[id].moved) {
        (void)target;
        if (!charge(1))
          return false;
        ++incidence[source];
      }
    }
    std::vector<uint64_t> rarity(generators.size(), UINT64_MAX);
    for (const size_t id : order)
      for (const auto &[source, target] : generators[id].moved) {
        (void)target;
        rarity[id] = std::min(rarity[id], incidence[source]);
      }
    // Score-only ordering, as in Satsuma; no detector-order tie-break.
    std::sort(order.begin(), order.end(), [&](size_t first, size_t second) {
      return rarity[first] < rarity[second];
    });
    return true;
  };
  const auto at_limit = [&] {
    return stats.negation_units + stats.binary_units ==
           parameters.graph_symmetry.maximum_fixing_instances;
  };
  // Negation fixing, then binary fixing, each over a fresh ranking of the
  // active generators.
  std::unordered_set<uint64_t> binaries;
  for (const bool binary_phase : {false, true}) {
    if (binary_phase) {
      // Binary fixing always follows (satsuma.h), over the generators that
      // stabilize every negation unit.
      for (size_t id = 0; id < generators.size(); ++id) {
        if (!active[id])
          continue;
        for (const auto &[source, target] : generators[id].moved) {
          (void)target;
          if (!charge(1))
            return false;
          if (propagated[source]) {
            active[id] = 0;
            break;
          }
        }
      }
      std::fill(propagated.begin(), propagated.end(), 0);

      binaries.reserve(snapshot.clause_index.clauses().size());
      for (const auto &clause : snapshot.clause_index.clauses()) {
        if (clause.size() != 2)
          continue;
        if (!charge(1))
          return false;
        binaries.insert(kernel_binary_key(clause[0], clause[1]));
      }
    }
    if (!rank_active())
      return false;
    for (const size_t id : order) {
      if (at_limit())
        return true;
      const auto &image = generators[id];
      Lit applicable = INVALID_LIT;
      bool touches_fixed = false;
      for (const auto &[source, target] : image.moved) {
        if (!charge(1))
          return false;
        if (propagated[source]) {
          touches_fixed = true;
          break;
        }
        if (binary_phase) {
          if (applicable == INVALID_LIT && source != target &&
              source != negate(target) && !kernel.value(source) &&
              !kernel.value(target) &&
              binaries.count(kernel_binary_key(source, target)))
            applicable = target;
        } else if (source == negate(target) && image[target] == source &&
                   !kernel.value(source) &&
                   (applicable == INVALID_LIT ||
                    incidence[source] < incidence[applicable]))
          applicable = source;
      }
      if (touches_fixed || applicable == INVALID_LIT) {
        if (touches_fixed && !binary_phase)
          active[id] = 0;
        continue;
      }
      if (!binary_phase)
        active[id] = 0;
      propagated[applicable] = 1;
      ++(binary_phase ? stats.binary_units : stats.negation_units);
      ++result.instances;
      // A binary unit's witness is the generator: the image of (source v
      // applicable) is true under the negated unit.
      if (!visitor(symmetry_generator_unit_instance(applicable, image))) {
        stats.visitor_stopped = true;
        return true;
      }
    }
  }
  return true;
}

SubstitutionRedundancyInstance representative_unit_instance(
    Lit representative) {
  SubstitutionRedundancyInstance instance;
  instance.clause = {representative};
  instance.symmetry_chain_certificate = true;
  return instance;
}

SymmetryEnumerationStatus convert(symmetry_backend::Status status) {
  using Backend = symmetry_backend::Status;
  using Pass = SymmetryEnumerationStatus;
  switch (status) {
  case Backend::Complete: return Pass::Complete;
  case Backend::Incomplete: return Pass::LimitReached;
  case Backend::Interrupted: return Pass::Interrupted;
  case Backend::Invalid: return Pass::Invalid;
  case Backend::ResourceExhausted: return Pass::ResourceExhausted;
  case Backend::Failed: return Pass::Failed;
  }
  return Pass::Failed;
}

} // namespace

SymmetryEnumerationResult substitution_redundancy_enumerate_symmetry(
    const Kernel &kernel,
    const SubstitutionRedundancyParameters &requested_parameters,
    const OrbitopalInstanceVisitor &visitor, volatile sig_atomic_t *stop,
    const SymmetryDiscoveryHints *hints,
    SymmetryEnumerationScheduling scheduling) {
  SymmetryEnumerationResult result;
  SubstitutionRedundancyParameters parameters = requested_parameters;
  const SymmetryPass pass{kernel, parameters, stop, result};
  const SymmetryTrace trace;
  trace.event("symmetry_begin", "variables\t%u", kernel.nvars);
  if (!parameters.valid() ||
      parameters.producer !=
          SubstitutionRedundancyProducer::GraphSymmetry)
    return result;
  if (!parameters.maximum_detection_ticks ||
      !parameters.graph_symmetry.maximum_graph_vertices ||
      !parameters.graph_symmetry.maximum_graph_edges ||
      !parameters.graph_symmetry.maximum_generators) {
    // Zero disables the producer: a completed empty pass.
    return result;
  }

  // Every graph has both literal vertices: reject an impossible allowance
  // before the snapshot and structured discovery.
  if (2ull * kernel.nvars > parameters.graph_symmetry.maximum_graph_vertices)
    return finish(result, SymmetryEnumerationStatus::LimitReached);

  FormulaSnapshot snapshot;
  if (!trace.measure("symmetry_snapshot", [&] {
        return pass.snapshot_formula(snapshot);
      }))
    return result;
  const uint32_t literal_count = 2 * kernel.nvars;
  if (trace.enabled()) {
    FormulaFingerprint fingerprint;
    std::vector<uint32_t> external;
    for (const auto &clause : snapshot.clause_index.clauses()) {
      if (clause.size() < 2)
        continue;
      external.clear();
      for (const Lit literal : clause)
        external.push_back(kernel_external_lit(kernel, literal));
      fingerprint.add(external);
    }
    trace.event("residual_fingerprint",
                "clauses\t%llu\tliterals\t%llu\tsum\t%llu\tsquared_sum\t%llu\tunits\t%zu",
                static_cast<unsigned long long>(fingerprint.clauses),
                static_cast<unsigned long long>(fingerprint.literals),
                static_cast<unsigned long long>(fingerprint.sum),
                static_cast<unsigned long long>(fingerprint.squared_sum),
                snapshot.clause_index.clauses().size() - fingerprint.clauses);
    trace.event("residual_multiplicity", "physical\t%zu\tdistinct\t%zu",
                snapshot.clause_index.clauses().size(),
                snapshot.clause_index.distinct_clause_count());
  }
  const GraphSymmetryParameters &configured = parameters.graph_symmetry;
  const bool large_domain_generator_fixing =
      configured.fixing_mode != SymmetryFixingMode::GeneratorsOnly &&
      kernel.nvars > configured.schreier_domain_limit;
  SymmetryDiscoveryHints structured;
  bool structured_incomplete = false;
  if (!large_domain_generator_fixing &&
      (configured.use_structured_generators ||
       configured.stabilize_structured_variables)) {
    auto discovery_parameters = parameters;
    discovery_parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
    discovery_parameters.maximum_detection_ticks -= result.ticks;
    if (hints) {
      if (hints->generators.size() > configured.maximum_generators)
        return finish(result, SymmetryEnumerationStatus::LimitReached);
      discovery_parameters.graph_symmetry.maximum_generators -=
          hints->generators.size();
    }
    const auto discovery = trace.measure("symmetry_structured_discovery", [&] {
      return substitution_redundancy_discover_orbitopal_generators(
          kernel, discovery_parameters, structured, stop);
    });
    trace.event("symmetry_structured_result", "status\t%u\tmatrices\t%llu\tticks\t%llu",
                static_cast<unsigned>(discovery.status),
                static_cast<unsigned long long>(discovery.matrices),
                static_cast<unsigned long long>(discovery.ticks));
    result.ticks += discovery.ticks;
    if (discovery.status == OrbitopalEnumerationStatus::Interrupted)
      return finish(result, SymmetryEnumerationStatus::Interrupted);
    structured_incomplete =
        discovery.status == OrbitopalEnumerationStatus::LimitReached;
    if (!configured.use_structured_generators)
      structured.generators.clear();
    if (!configured.stabilize_structured_variables)
      structured.fixed_variables.clear();
    // Structured discovery shares the allowance; if it spent the last credit,
    // no later step could publish, so end here.
    if (result.ticks >= parameters.maximum_detection_ticks)
      return finish(result, SymmetryEnumerationStatus::LimitReached);
  }
  std::vector<SparseLiteralPermutation> generators;
  // Generator and transversal checks charge setup differently; separate
  // workspaces keep budgets independent of order.
  SymmetryClauseVisitWorkspace generator_workspace;
  std::vector<Var> fixed_variables;
  for (const auto *advice : {hints, static_cast<const SymmetryDiscoveryHints *>(&structured)}) {
    if (!advice) continue;
    if (advice->generators.size() > configured.maximum_generators - generators.size())
      return finish(result, SymmetryEnumerationStatus::LimitReached);
    for (const auto &image : advice->generators) {
      if (!pass.charge(image.size()))
        return result;
      if (!pass.exact_automorphism(snapshot, image)) {
        if (result.status == SymmetryEnumerationStatus::Complete)
          result.status = SymmetryEnumerationStatus::Invalid;
        return result;
      }
      SparseLiteralPermutation sparse{literal_count, {}};
      append_moved_points(sparse, image, literal_count);
      generators.push_back(std::move(sparse));
    }
    for (const Var variable : advice->fixed_variables) {
      if (!pass.charge(1)) return result;
      if (variable >= kernel.nvars)
        return finish(result, SymmetryEnumerationStatus::Invalid);
      fixed_variables.push_back(variable);
    }
  }
  structured.generators.clear();
  trace.event("symmetry_structured_handoff", "generators\t%zu\tfixed_variables\t%zu",
              generators.size(), fixed_variables.size());
  LiteralGraph graph_storage;
  if (!trace.measure("symmetry_graph", [&] {
        return pass.build_symmetry_graph(snapshot, graph_storage);
      }))
    return result;

  for (const Var variable : fixed_variables) {
    // Unique colors, disjoint from gadget and literal colors, fix both signs.
    graph_storage.colors[2 * variable] = 3 + 2 * variable;
    graph_storage.colors[2 * variable + 1] = 3 + 2 * variable + 1;
  }

  trace.event("symmetry_graph_ready", "clauses\t%zu\tvertices\t%u\tedges\t%llu",
              snapshot.clause_index.clauses().size(),
              graph_storage.vertex_count,
              static_cast<unsigned long long>(graph_storage.edge_count));
  diagnostics::capture_graph(
      graph_storage.vertex_count, graph_storage.edge_count,
      [&](uint64_t vertex) { return graph_storage.colors[vertex]; },
      [&](auto edge) {
        for (size_t i = 0; i < graph_storage.edges.size(); i += 2)
          edge(graph_storage.edges[i], graph_storage.edges[i + 1]);
      });
  symmetry_backend::DetectionSettings settings =
      sr_detail::detection_settings(configured, literal_count);
  settings.maximum_generators = configured.maximum_generators - generators.size();
  const uint64_t validation_quantum =
      generator_validation_upper_bound(snapshot, literal_count);
  const uint64_t remaining_detection_ticks =
      parameters.maximum_detection_ticks - result.ticks;
  const uint64_t validation_allowance =
      remaining_detection_ticks > validation_quantum
          ? remaining_detection_ticks - validation_quantum
          : 0;
  const uint64_t affordable_generators = validation_quantum
      ? validation_allowance / validation_quantum
      : 0;
  const uint64_t requested_generators = settings.maximum_generators;
  const uint64_t hard_detection_ticks =
      scheduling.progress_ticks_per_generator &&
              scheduling.hard_detection_ticks >
                  parameters.maximum_detection_ticks
          ? scheduling.hard_detection_ticks
          : parameters.maximum_detection_ticks;
  const uint64_t hard_remaining_detection_ticks =
      hard_detection_ticks > result.ticks
          ? hard_detection_ticks - result.ticks
          : 0;
  const uint64_t hard_validation_allowance =
      hard_remaining_detection_ticks > validation_quantum
          ? hard_remaining_detection_ticks - validation_quantum
          : 0;
  const uint64_t hard_affordable_generators = validation_quantum
      ? hard_validation_allowance / validation_quantum
      : 0;
  const bool detector_work_capped =
      hard_affordable_generators < requested_generators;
  settings.maximum_generators =
      std::min(requested_generators, hard_affordable_generators);
  // Under progress scheduling each exact generator earns its validation, so
  // entry admits one generator whenever the hard allowance affords it.
  const uint64_t entry_generators =
      scheduling.progress_ticks_per_generator && hard_affordable_generators ? 1
                                                                            : 0;
  settings.initial_generators =
      std::min(settings.maximum_generators,
               std::max(affordable_generators, entry_generators));
  settings.progress_generators_per_generator = validation_quantum
      ? scheduling.progress_ticks_per_generator / validation_quantum
      : 0;
  trace.event("symmetry_generator_admission",
              "remaining\t%llu\thard_remaining\t%llu\tvalidation_quantum\t%llu\t"
              "affordable\t%llu\tinitial_cap\t%llu\tcap\t%llu\tprogress\t%llu",
              static_cast<unsigned long long>(remaining_detection_ticks),
              static_cast<unsigned long long>(hard_remaining_detection_ticks),
              static_cast<unsigned long long>(validation_quantum),
              static_cast<unsigned long long>(affordable_generators),
              static_cast<unsigned long long>(settings.initial_generators),
              static_cast<unsigned long long>(settings.maximum_generators),
              static_cast<unsigned long long>(
                  settings.progress_generators_per_generator));
  const symmetry_backend::Graph graph = graph_storage.backend();
  StopContext context{&kernel, stop};
  symmetry_backend::Permutations proposed;
  const symmetry_backend::Status detection = trace.measure("symmetry_detect", [&] {
    if (!settings.initial_generators)
      return symmetry_backend::Status::Incomplete;
    return symmetry_backend::detect(graph, settings, backend_stop, &context,
                                    proposed);
  });
  trace.event("symmetry_detected", "generators\t%llu\tstatus\t%u",
              static_cast<unsigned long long>(proposed.count),
              static_cast<unsigned>(detection));
  const bool detection_incomplete =
      detection == symmetry_backend::Status::Incomplete || structured_incomplete;
  // A completed pass over an incomplete detection still reports its limit.
  const auto settled = [&] {
    if (result.status == SymmetryEnumerationStatus::Complete &&
        detection_incomplete)
      result.status = SymmetryEnumerationStatus::LimitReached;
    return result;
  };
  result.status = convert(detection);
  // A complete subgroup proves the full set-automorphism group only without
  // fixed variables, a root stabilizer or duplicate clauses.
  result.complete_group = proposed.complete_group && fixed_variables.empty() &&
      !configured.pointwise_root_stabilizer &&
      snapshot.clause_index.clauses().size() ==
          snapshot.clause_index.distinct_clause_count();
  result.generators_proposed = proposed.count + generators.size();
  if (scheduling.progress_ticks_per_generator && proposed.count) {
    result.discovery_progress_units = proposed.count;
    const uint64_t credit =
        proposed.count >
                UINT64_MAX / scheduling.progress_ticks_per_generator
            ? UINT64_MAX
            : proposed.count * scheduling.progress_ticks_per_generator;
    const uint64_t renewed =
        credit > UINT64_MAX - parameters.maximum_detection_ticks
            ? UINT64_MAX
            : parameters.maximum_detection_ticks + credit;
    parameters.maximum_detection_ticks =
        std::min(renewed, hard_detection_ticks);
  }
  if (result.status == SymmetryEnumerationStatus::Interrupted ||
      result.status == SymmetryEnumerationStatus::Invalid ||
      result.status == SymmetryEnumerationStatus::ResourceExhausted ||
      result.status == SymmetryEnumerationStatus::Failed) {
    symmetry_backend::release(proposed);
    return result;
  }
  // Generator fixing uses each validated generator alone, so only a chain
  // planned over a work-capped prefix is discarded.
  if (detector_work_capped && !large_domain_generator_fixing &&
      detection == symmetry_backend::Status::Incomplete) {
    if (!scheduling.progress_ticks_per_generator && proposed.count &&
        proposed.count == settings.maximum_generators)
      result.discovery_progress_units = proposed.count;
    // The allowance cannot validate the detector stream and still plan the
    // group; an arbitrary callback-order prefix is discarded, keeping the
    // structured and hinted generators.
    symmetry_backend::release(proposed);
    result.complete_group = false;
    if (generators.empty()) {
      // As when charge() declines: the allowance is saturated so the shared
      // scope does not retry on the remainder.
      result.ticks = parameters.maximum_detection_ticks;
      return finish(result, SymmetryEnumerationStatus::LimitReached);
    }
  }
  result.status = SymmetryEnumerationStatus::Complete;

  trace.event("symmetry_generator_validation", "boundary\tbegin");
  generators.reserve(generators.size() + proposed.count);
  for (uint64_t row = 0; row < proposed.count; ++row) {
    SparseLiteralPermutation image{literal_count, {}};
    bool literal_image = proposed.domain_size == literal_count;
    if (literal_image && proposed.image_offsets) {
      const uint64_t begin = proposed.image_offsets[row], end = proposed.image_offsets[row + 1];
      literal_image = !proposed.image_offsets[0] &&
          proposed.image_offsets[proposed.count] == proposed.image_entries &&
          begin <= end && end <= proposed.image_entries &&
          (!end || (proposed.image_points && proposed.images));
      if (literal_image)
        for (uint64_t at = begin; at < end; ++at)
          image.moved.emplace_back(proposed.image_points[at], proposed.images[at]);
    } else if (literal_image) {
      literal_image = proposed.images &&
          (!literal_count || proposed.count <= UINT64_MAX / literal_count);
      if (literal_image)
        append_moved_points(image, proposed.images + row * literal_count,
                            literal_count);
    }
    if (literal_image &&
        pass.exact_sparse_automorphism(snapshot, image, generator_workspace,
                                       /*generator=*/true))
      generators.push_back(std::move(image));
    if (result.status == SymmetryEnumerationStatus::Interrupted ||
        result.status == SymmetryEnumerationStatus::LimitReached)
      break;
  }
  symmetry_backend::release(proposed);
  if (!configured.preserve_detector_order) {
    std::sort(generators.begin(), generators.end(), [](const auto &a, const auto &b) {
      return symmetry_backend::detail::implicit_dense_less(a.moved, b.moved);
    });
    generators.erase(std::unique(generators.begin(), generators.end(),
                                 [](const auto &a, const auto &b) { return a.moved == b.moved; }),
                     generators.end());
  }
  generators.erase(
      std::remove_if(generators.begin(), generators.end(),
                     [](const auto &image) {
                       return image.moved.empty();
                     }),
      generators.end());
  apply_order(generators, configured.generator_order,
              static_cast<uint64_t>(configured.seed) ^
                  0x47454e455241544full);
  result.generators_validated = generators.size();
  trace.event("symmetry_generator_validation", "boundary\tend\taccepted\t%zu",
              generators.size());
  if (result.generators_validated != result.generators_proposed)
    result.complete_group = false;
  if (result.status == SymmetryEnumerationStatus::Interrupted ||
      result.status == SymmetryEnumerationStatus::LimitReached ||
      generators.empty())
    return settled();

  if (large_domain_generator_fixing) {
    GeneratorFixingStats fixing;
    // Each instance is a proof-checked root unit, never withheld.
    const bool completed = trace.measure("symmetry_generator_fixing", [&] {
      return pass.generator_fixing_scans(snapshot, generators, visitor, fixing);
    });
    trace.event("symmetry_generator_fixing_result",
                "negation_units\t%llu\tbinary_units\t%llu\tvisitor_stopped\t%u",
                static_cast<unsigned long long>(fixing.negation_units),
                static_cast<unsigned long long>(fixing.binary_units),
                static_cast<unsigned>(fixing.visitor_stopped));
    if (!completed)
      return result;
    return settled();
  }

  std::vector<EligibleClause> clauses;
  if (!trace.measure("symmetry_eligible_clauses", [&] {
        return pass.eligible_clauses(snapshot, clauses);
      }))
    return result;
  if (clauses.empty() &&
      configured.chain_mode == SymmetryChainMode::ClausalFixing)
    return result;
  std::vector<uint32_t> generator_images, generator_points;
  std::vector<uint64_t> generator_offsets{0};
  for (const auto &generator : generators) {
    for (const auto &[source, target] : generator.moved) {
      generator_points.push_back(source);
      generator_images.push_back(target);
    }
    generator_offsets.push_back(generator_images.size());
  }
  if (trace.enabled()) {
    std::vector<uint32_t> dense(literal_count);
    for (Lit literal = 0; literal < literal_count; ++literal) dense[literal] = literal;
    size_t fixed_only = 0, open = 0;
    for (const auto &generator : generators) {
      bool moved_fixed = false, moved_open = false;
      for (const auto &[source, target] : generator.moved) {
        (void)target;
        if (kernel.value(source)) moved_fixed = true;
        else moved_open = true;
      }
      open += moved_open;
      fixed_only += moved_fixed && !moved_open;
    }
    trace.event("permutation_support",
                "producer\tgeneric\tmaps\t%zu\tfixed_only\t%zu\topen\t%zu\tidentity\t0",
                generators.size(), fixed_only, open);
    for (size_t row = 0; row < generators.size(); ++row) {
      for (const auto &[source, target] : generators[row].moved) dense[source] = target;
      trace.permutation("group_generator", row, dense.data(), literal_count);
      for (const auto &[source, target] : generators[row].moved) {
        (void)target;
        dense[source] = source;
      }
    }
  }
  std::vector<uint64_t> offsets = {0};
  std::vector<uint32_t> literals;
  std::vector<uint8_t> clause_is_amo;
  for (const auto &clause : clauses) {
    literals.insert(literals.end(), clause.literals.begin(),
                    clause.literals.end());
    offsets.push_back(literals.size());
    clause_is_amo.push_back(clause.amo);
  }
  std::vector<int8_t> values(literal_count);
  for (Lit literal = 0; literal < literal_count; ++literal)
    values[literal] = static_cast<int8_t>(kernel.value(literal));

  uint64_t group_ticks = 0;
  uint64_t stabilizer_instances = 0;
  if (trace.enabled()) {
    trace.event("literal_identity", "variables\t%u\tdefault\tidentity",
                kernel.nvars);
    for (Var variable = 0; variable < kernel.nvars; ++variable)
      if (kernel.internal_to_external[variable] != variable)
        trace.event("literal_identity_override", "internal_var\t%u\texternal_var\t%u",
                    variable, kernel.internal_to_external[variable]);
  }
  if (configured.fixing_mode != SymmetryFixingMode::GeneratorsOnly &&
      configured.maximum_fixing_instances &&
      configured.maximum_group_ticks) {
    if (scheduling.finish_admitted_group &&
        result.ticks >= parameters.maximum_detection_ticks)
      return finish(result, SymmetryEnumerationStatus::LimitReached);
    const uint64_t planning_allowance =
        parameters.maximum_detection_ticks - result.ticks;
    SubstitutionRedundancyParameters finalization_parameters = parameters;
    if (scheduling.finish_admitted_group)
      finalization_parameters.maximum_detection_ticks = UINT64_MAX;
    const SymmetryPass group_pass{
        kernel,
        scheduling.finish_admitted_group ? finalization_parameters : parameters,
        stop, result};
    const symmetry_backend::GroupInput group{
        literal_count,
        generators.size(),
        generator_images.data(),
        clauses.size(),
        offsets.data(),
        literals.size(),
        literals.data(),
        values.data(),
        clause_is_amo.data(),
        generator_offsets.data(),
        generator_points.data(),
        generator_images.size(),
    };
    const symmetry_backend::GroupSettings group_settings{
        configured.seed, configured.error_bound,
        configured.maximum_fixing_instances, configured.maximum_group_ticks,
        static_cast<uint8_t>(configured.chain_mode),
        static_cast<uint8_t>(configured.base_selection),
        scheduling.finish_admitted_group
            ? std::min(configured.chain_planning_tick_limit
                           ? configured.chain_planning_tick_limit : UINT64_MAX,
                       planning_allowance)
            : configured.chain_planning_tick_limit,
        configured.factor_support_components,
        static_cast<uint8_t>(configured.cut_point),
        configured.negation_fixing_first,
        configured.first_literal_fix_anchor};
    symmetry_backend::Fixings fixings;
    const symmetry_backend::Status fixed = trace.measure("symmetry_fix_clauses", [&] {
      return symmetry_backend::fix_clauses(group, group_settings, backend_stop,
                                          &context, fixings);
    });
    const SymmetryEnumerationStatus fixing_status = convert(fixed);
    if (fixing_status == SymmetryEnumerationStatus::Interrupted ||
        fixing_status == SymmetryEnumerationStatus::Invalid ||
        fixing_status == SymmetryEnumerationStatus::ResourceExhausted ||
        fixing_status == SymmetryEnumerationStatus::Failed) {
      result.status = fixing_status;
      symmetry_backend::release(fixings);
      return result;
    }
    if (fixing_status == SymmetryEnumerationStatus::LimitReached)
      result.status = fixing_status;
    group_ticks = fixings.ticks;
    if (scheduling.finish_admitted_group) {
      if (!group_pass.charge(group_ticks)) {
        symmetry_backend::release(fixings);
        return result;
      }
      group_ticks = 0;
    }

    struct CertifiedRow {
      uint8_t kind = 0;
      uint32_t level = 0;
      Lit representative = INVALID_LIT;
      Lit other = INVALID_LIT;
      SparseLiteralPermutation image;
    };
    trace.event("symmetry_transversal_validation", "boundary\tbegin\tproposed\t%llu",
                static_cast<unsigned long long>(fixings.count));
    std::vector<CertifiedRow> rows;
    rows.reserve(fixings.count);
    SymmetryClauseVisitWorkspace visit_workspace;
    bool valid_chain = !fixings.count ||
        (fixings.domain_size == literal_count && fixings.levels && fixings.kinds &&
         fixings.representatives && fixings.others);
    for (uint64_t row = 0; row < fixings.count && valid_chain; ++row) {
      const uint8_t kind = fixings.kinds[row];
      const uint32_t level = fixings.levels[row];
      const Lit representative = fixings.representatives[row];
      const Lit other = fixings.others[row];
      if (kind > 3 || representative >= literal_count ||
          ((kind == 0 || kind == 2 || kind == 3) &&
           other >= literal_count) ||
          (kind == 3 && other != negate(representative))) {
        valid_chain = false;
        break;
      }
      SparseLiteralPermutation image;
      image.domain = literal_count;
      if (kind != 1) {
        if (fixings.image_offsets) {
          const auto begin = fixings.image_offsets[row], end = fixings.image_offsets[row + 1];
          if (fixings.image_offsets[0] || fixings.image_offsets[fixings.count] != fixings.image_entries ||
              begin > end || end > fixings.image_entries ||
              (end && (!fixings.image_points || !fixings.images))) {
            valid_chain = false;
            break;
          }
          if (!group_pass.charge(end - begin)) {
            valid_chain = false;
            break;
          }
          image.moved.reserve(end - begin);
          for (auto at = begin; at < end; ++at)
            image.moved.emplace_back(fixings.image_points[at], fixings.images[at]);
        } else {
          if (!fixings.images) { valid_chain = false; break; }
          if (!group_pass.charge(literal_count)) {
            valid_chain = false;
            break;
          }
          append_moved_points(image, fixings.images + row * literal_count,
                              literal_count);
        }
      }
      // An untrusted backend map must be an exact automorphism of the
      // snapshot mapping the base to the advertised orbit point.
      if (kind != 1 &&
          (!group_pass.exact_sparse_automorphism(snapshot, image, visit_workspace,
                                            /*generator=*/false) ||
           image[representative] != other)) {
        valid_chain = false;
        break;
      }
      rows.push_back({kind, level, representative, other, std::move(image)});
    }

    trace.event("symmetry_transversal_validation",
                "boundary\tend\taccepted\t%zu\tvalid\t%u",
                rows.size(), static_cast<unsigned>(valid_chain));
    trace.event("symmetry_chain_validation", "boundary\tbegin");

    // A later level's witness must preserve every earlier cut orbit, so each
    // earlier cut's image is a published cut or a tautology.
    std::vector<CutOrbitMember> cut_members;
    for (const auto &row : rows)
      if (row.kind == 2)
        cut_members.push_back({row.level, row.representative, row.other});
    const CutOrbitIndex cut_index(cut_members);
    valid_chain &= cut_index.valid();
    for (const auto &row : rows) {
      if (!valid_chain)
        break;
      if (row.kind == 1)
        continue;
      valid_chain = cut_index.preserves(row.image, row.level);
    }

    trace.event("symmetry_chain_validation", "boundary\tend\tvalid\t%u",
                static_cast<unsigned>(valid_chain));
    const auto root_forcing = [&](const CertifiedRow &row) {
      // Kinds 1 and 3 are units; a binary with exactly one root-false literal
      // is unit too and is never withheld as a nonforcing cut.
      if (row.kind == 1 || row.kind == 3)
        return true;
      const Lit first = row.kind == 0 ? row.representative
                                      : negate(row.representative);
      const Lit second = row.kind == 0 ? negate(row.other) : row.other;
      if (kernel.value(first) > 0 || kernel.value(second) > 0)
        return false;
      return (kernel.value(first) < 0) != (kernel.value(second) < 0);
    };
    const bool forcing_batch =
        std::any_of(rows.begin(), rows.end(), root_forcing);
    const bool decline_nonforcing =
        valid_chain && !rows.empty() && !forcing_batch &&
        (configured.nonforcing_cut_admission ==
             SymmetryNonforcingCutAdmission::RequireForcingMember ||
         (configured.nonforcing_cut_admission ==
              SymmetryNonforcingCutAdmission::RequireCompoundProgress &&
          rows.size() == 1));
    trace.event("symmetry_publish",
                "boundary\tbegin\tforcing\t%u\tdeclined_nonforcing\t%u"
                "\tdeclined_singleton\t%u",
                static_cast<unsigned>(forcing_batch),
                static_cast<unsigned>(decline_nonforcing),
                0u);
    if (!valid_chain) {
      result.complete_group = false;
      if (result.status == SymmetryEnumerationStatus::Complete)
        result.status = SymmetryEnumerationStatus::Failed;
    } else if (decline_nonforcing) {
      result.nonforcing_orbit_cuts_declined += rows.size();
      stabilizer_instances += rows.size();
    } else
      for (auto &row : rows) {
        const auto &image = row.image;
        ++result.instances;
        ++stabilizer_instances;
        const SubstitutionRedundancyInstance instance =
            row.kind == 0
                ? chain_binary_instance(row.representative, negate(row.other),
                                        image)
            : row.kind == 1
                ? representative_unit_instance(row.representative)
            : row.kind == 2
                ? chain_binary_instance(negate(row.representative), row.other,
                                        image)
                : symmetry_generator_unit_instance(row.representative, image);
        if (!visitor(instance))
          break;
      }
    trace.event("symmetry_publish",
                "boundary\tend\tinstances\t%llu\tnonforcing_declined\t%llu"
                "\tsingleton_declined\t%llu",
                static_cast<unsigned long long>(result.instances),
                static_cast<unsigned long long>(
                    result.nonforcing_orbit_cuts_declined),
                static_cast<unsigned long long>(
                    result.singleton_forcing_rows_declined));
    symmetry_backend::release(fixings);
    if (result.status == SymmetryEnumerationStatus::Failed ||
        result.status == SymmetryEnumerationStatus::Invalid ||
        result.status == SymmetryEnumerationStatus::Interrupted)
      return result;
  }

  // With an empty stabilizer batch, try direct generator involutions; later
  // proposals must stabilize every earlier unit.
  const bool generator_phase =
      result.status == SymmetryEnumerationStatus::Complete &&
      (configured.fixing_mode == SymmetryFixingMode::GeneratorsOnly ||
       (configured.fixing_mode ==
            SymmetryFixingMode::StabilizerThenGenerators &&
        !stabilizer_instances));
  if (generator_phase && configured.maximum_generator_instances_per_pass) {
    std::vector<uint8_t> locally_fixed(literal_count, 0);
    uint64_t generated = 0;
    bool visitor_stopped = false;
    for (const auto &image : generators) {
      if (visitor_stopped ||
          generated == configured.maximum_generator_instances_per_pass)
        break;
      bool involution = true, stabilizes_prefix = true;
      for (const auto &[source, target] : image.moved) {
        involution &= image[target] == source;
        if (locally_fixed[source]) stabilizes_prefix = false;
      }
      if ((configured.direct_generator_involutions_only && !involution) ||
          !stabilizes_prefix)
        continue;
      bool emitted = false;
      for (const auto &clause : clauses) {
        for (const Lit fixed_false : clause.literals) {
          const Lit witness_true = image[fixed_false];
          if (fixed_false == witness_true || values[fixed_false] ||
              values[witness_true] ||
              locally_fixed[fixed_false] ||
              std::find(clause.literals.begin(), clause.literals.end(),
                        witness_true) == clause.literals.end())
            continue;
          const bool canonical = fixed_false < witness_true;
          if ((configured.pair_orientation ==
                   SymmetryPairOrientation::Canonical &&
               !canonical) ||
              (configured.pair_orientation == SymmetryPairOrientation::Reverse &&
               canonical))
            continue;
          if (configured.pair_orientation == SymmetryPairOrientation::Seeded) {
            uint64_t pair_seed = static_cast<uint64_t>(configured.seed) ^
                                 0x504149524f524945ull;
            pair_seed ^= static_cast<uint64_t>(
                std::min(fixed_false, witness_true));
            pair_seed ^= static_cast<uint64_t>(
                             std::max(fixed_false, witness_true))
                         << 32;
            const bool select_canonical =
                (splitmix64_next(pair_seed) & 1u) == 0;
            if (canonical != select_canonical)
              continue;
          }
          ++result.instances;
          ++generated;
          locally_fixed[fixed_false] = 1;
          values[fixed_false] = -1;
          values[negate(fixed_false)] = 1;
          if (!visitor(
                  negative_fixing_instance(fixed_false, witness_true, image))) {
            visitor_stopped = true;
            emitted = true;
            break;
          }
          emitted = true;
          break;
        }
        if (emitted)
          break;
      }
    }
  }
  result.ticks = saturating_add(result.ticks, group_ticks);
  return settled();
}

} // namespace tactix
