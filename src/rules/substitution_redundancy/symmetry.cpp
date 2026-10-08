#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include "rules/substitution_redundancy/clause_multiset.hpp"
#include "rules/substitution_redundancy/cut_orbit_index.hpp"
#include "rules/substitution_redundancy/literal_permutation.hpp"

#include "symmetry_backend/dejavu_backend.hpp"
#include "symmetry_backend/sparse_permutation.hpp"
#include "diagnostics/symmetry_trace.hpp"
#include "diagnostics/formula_fingerprint.hpp"
#include "diagnostics/graph_capture.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <vector>

namespace tactix {
namespace {

struct FormulaSnapshot {
  SymmetryClauseMultiset clause_index;
  std::vector<uint32_t> occurrences;
};

struct SymmetryGraphStorage {
  std::vector<uint32_t> colors;
  std::vector<uint32_t> degrees;
  std::vector<uint32_t> edges;
  uint32_t vertices = 0;
  uint64_t edge_count = 0;
  bool direct_binary_edges = false;
};

struct StopContext {
  const Kernel *kernel;
  volatile sig_atomic_t *external;
};

uint64_t saturating_add(uint64_t first, uint64_t second) {
  return second > UINT64_MAX - first ? UINT64_MAX : first + second;
}

uint64_t saturating_twice(uint64_t value) {
  return value > UINT64_MAX / 2 ? UINT64_MAX : 2 * value;
}

// A sparse generator's exact set-automorphism check charges one full clause
// count and literal-domain scan, at most every literal occurrence once while
// finding touched clauses, and at most every clause literal once while mapping
// those clauses.  This upper bound lets discovery admit only a generator batch
// that can be validated, while reserving one equally sized downstream work
// quantum for group planning.
uint64_t generator_validation_upper_bound(const FormulaSnapshot &snapshot,
                                          uint64_t literal_count) {
  uint64_t support = 0;
  for (const auto &clause : snapshot.clause_index.clauses())
    support = saturating_add(support, clause.size());
  return saturating_add(
      saturating_add(snapshot.clause_index.clauses().size(), literal_count),
      saturating_twice(support));
}

uint64_t splitmix64(uint64_t &state) {
  state += 0x9e3779b97f4a7c15ull;
  uint64_t value = state;
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
  return value ^ (value >> 31);
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
    const size_t selected = splitmix64(seed) % remaining;
    std::swap(values[remaining - 1], values[selected]);
  }
}

bool backend_stop(void *opaque) noexcept {
  const auto *context = static_cast<const StopContext *>(opaque);
  return (context->external && *context->external) ||
         (context->kernel->stop && *context->kernel->stop);
}

bool charge(const Kernel &kernel, const SubstitutionRedundancyParameters &p,
            volatile sig_atomic_t *stop, SymmetryEnumerationResult &result,
            uint64_t amount) {
  if ((stop && *stop) || (kernel.stop && *kernel.stop)) {
    result.status = SymmetryEnumerationStatus::Interrupted;
    return false;
  }
  if (result.ticks >= p.maximum_detection_ticks ||
      amount > p.maximum_detection_ticks - result.ticks) {
    result.ticks = p.maximum_detection_ticks;
    result.status = SymmetryEnumerationStatus::LimitReached;
    result.allowance_limit_reached = true;
    return false;
  }
  result.ticks += amount;
  return true;
}

bool snapshot_formula(const Kernel &kernel,
                      const SubstitutionRedundancyParameters &parameters,
                      volatile sig_atomic_t *stop,
                      SymmetryEnumerationResult &result,
                      FormulaSnapshot &snapshot) {
  const uint64_t literal_vertices = 2ull * kernel.nvars;
  if (literal_vertices > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
    result.status = SymmetryEnumerationStatus::LimitReached;
    result.detection_limit_reached = true;
    return false;
  }
  std::vector<std::vector<Lit>> clauses;
  snapshot.occurrences.assign(literal_vertices, 0);
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    for (const WatchEntry watch : kernel.watches.entries(literal)) {
      if (!charge(kernel, parameters, stop, result, 1))
        return false;
      if (watch.binary() && literal < watch.other())
        clauses.push_back({literal, watch.other()});
    }
  for (Ref ref = 0; ref < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage()) {
      if (!charge(kernel, parameters, stop, result, clause->size))
        return false;
      clauses.emplace_back(clause->begin(), clause->end());
    }
    ref += words;
  }
  for (Var var = 0; var < kernel.nvars; ++var) {
    if (!charge(kernel, parameters, stop, result, 1))
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

bool build_symmetry_graph(const Kernel &kernel,
                          const SubstitutionRedundancyParameters &parameters,
                          const FormulaSnapshot &snapshot,
                          volatile sig_atomic_t *stop,
                          SymmetryEnumerationResult &result,
                          SymmetryGraphStorage &storage) {
  const GraphSymmetryParameters &settings = parameters.graph_symmetry;
  // A uniquely colored assigned literal already fixes its incident unit
  // clause. Omitting that leaf preserves the projected literal group; the
  // complete snapshot, including every unit, remains the exact-check oracle.
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
    if (support > UINT64_MAX - clause.size()) {
      result.status = SymmetryEnumerationStatus::ResourceExhausted;
      return false;
    }
    support += clause.size();
    if (clause.size() == 2)
      ++binaries;
    else {
      ++nonbinary;
      nonbinary_support += clause.size();
    }
  }
  if (settings.binary_graph_mode == SymmetryBinaryGraphMode::DirectEdges) {
    storage.direct_binary_edges = true;
  } else if (settings.binary_graph_mode ==
             SymmetryBinaryGraphMode::Automatic) {
    const uint64_t threshold =
        static_cast<uint64_t>(kernel.nvars) *
        settings.binary_graph_density_permille;
    const uint64_t required =
        threshold / 1000 + static_cast<uint64_t>(threshold % 1000 != 0);
    storage.direct_binary_edges = binaries >= required;
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

  // The backend graph must be simple. Each further copy of a binary clause
  // becomes a clause vertex of degree two, so the graph group still preserves
  // the exact clause multiset. Repeats only enlarge the direct graph, so the
  // repeat-free size is admitted before the classification scan, which is
  // charged one tick per binary clause and polls the stop request.
  std::vector<bool> repeated_binary;
  uint64_t repeated_binaries = 0;
  if (storage.direct_binary_edges) {
    const uint64_t minimum_vertices =
        3ull * kernel.nvars + nonbinary;
    const uint64_t minimum_edges =
        2ull * kernel.nvars + binaries + nonbinary_support;
    if (over_size_limits(minimum_vertices, minimum_edges)) {
      result.status = SymmetryEnumerationStatus::LimitReached;
      result.detection_limit_reached = true;
      return false;
    }
    if (!charge(kernel, parameters, stop, result, binaries))
      return false;
    const auto &clauses = snapshot.clause_index.clauses();
    repeated_binary.assign(clauses.size(), false);
    std::unordered_set<uint64_t> direct;
    direct.reserve(static_cast<size_t>(binaries));
    uint64_t scanned = 0;
    for (size_t i = 0; i < clauses.size(); ++i) {
      if (clauses[i].size() != 2)
        continue;
      if (!(++scanned & 1023u) && !charge(kernel, parameters, stop, result, 0))
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
      storage.direct_binary_edges ? kernel.nvars : 0;
  const uint64_t clause_vertices =
      storage.direct_binary_edges ? nonbinary + repeated_binaries
                                  : snapshot.clause_index.clauses().size() -
                                        fixed_units;
  if (literal_vertices > UINT64_MAX - negation_vertices ||
      literal_vertices + negation_vertices > UINT64_MAX - clause_vertices) {
    result.status = SymmetryEnumerationStatus::ResourceExhausted;
    return false;
  }
  const uint64_t vertex_count =
      literal_vertices + negation_vertices + clause_vertices;
  const uint64_t negation_edges =
      storage.direct_binary_edges ? 2ull * kernel.nvars : kernel.nvars;
  const uint64_t clause_edges =
      storage.direct_binary_edges
          ? binaries + repeated_binaries + nonbinary_support
          : support;
  if (negation_edges > UINT64_MAX - clause_edges) {
    result.status = SymmetryEnumerationStatus::ResourceExhausted;
    return false;
  }
  const uint64_t edge_count = negation_edges + clause_edges;
  if (over_size_limits(vertex_count, edge_count)) {
    result.status = SymmetryEnumerationStatus::LimitReached;
    result.detection_limit_reached = true;
    return false;
  }
  if (!charge(kernel, parameters, stop, result, edge_count))
    return false;

  storage.vertices = static_cast<uint32_t>(vertex_count);
  storage.edge_count = edge_count;
  storage.colors.assign(storage.vertices, 1);
  storage.degrees.assign(storage.vertices, 0);
  storage.edges.reserve(static_cast<size_t>(2 * edge_count));
  // Satsuma gives each literal of an unused variable its own color
  // (symmetries.h compute_from_hypergraph, unused_color): a variable without
  // any clause is never moved, so it yields no generator and no unit.
  const auto unused = [&](Lit literal) {
    return !snapshot.occurrences[literal] &&
           !snapshot.occurrences[negate(literal)];
  };
  for (Lit literal = 0; literal < literal_vertices; ++literal)
    storage.colors[literal] = (kernel_var_retired(kernel, var_of(literal)) ||
                              unused(literal) ||
                              (settings.pointwise_root_stabilizer &&
                               kernel.value(literal)))
                                  ? 3 + literal
                                  : 0;

  const auto add_edge = [&](uint32_t first, uint32_t second) {
    if (first > second)
      std::swap(first, second);
    storage.edges.push_back(first);
    storage.edges.push_back(second);
    ++storage.degrees[first];
    ++storage.degrees[second];
  };
  uint32_t next_vertex = static_cast<uint32_t>(literal_vertices);
  if (storage.direct_binary_edges) {
    for (Var var = 0; var < kernel.nvars; ++var) {
      if (!(var & 1023u) && !charge(kernel, parameters, stop, result, 0))
        return false;
      const uint32_t gadget = next_vertex++;
      storage.colors[gadget] = 2;
      add_edge(make_lit(var, false), gadget);
      add_edge(make_lit(var, true), gadget);
    }
    const auto &clauses = snapshot.clause_index.clauses();
    for (size_t i = 0; i < clauses.size(); ++i) {
      const auto &clause = clauses[i];
      if (fixed_unit(clause))
        continue;
      if (!(storage.edges.size() & 2047u) &&
          !charge(kernel, parameters, stop, result, 0))
        return false;
      if (clause.size() == 2 && !repeated_binary[i]) {
        add_edge(clause[0], clause[1]);
        continue;
      }
      const uint32_t vertex = next_vertex++;
      for (const Lit literal : clause)
        add_edge(literal, vertex);
    }
  } else {
    for (Var var = 0; var < kernel.nvars; ++var)
      add_edge(make_lit(var, false), make_lit(var, true));
    for (const auto &clause : snapshot.clause_index.clauses()) {
      if (fixed_unit(clause))
        continue;
      if (!(storage.edges.size() & 2047u) &&
          !charge(kernel, parameters, stop, result, 0))
        return false;
      const uint32_t vertex = next_vertex++;
      for (const Lit literal : clause)
        add_edge(literal, vertex);
    }
  }
  if (next_vertex != storage.vertices ||
      storage.edges.size() != 2 * storage.edge_count) {
    result.status = SymmetryEnumerationStatus::Failed;
    return false;
  }
  for (const uint32_t degree : storage.degrees)
    if (degree > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
      result.status = SymmetryEnumerationStatus::ResourceExhausted;
      return false;
    }
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

bool exact_automorphism(const Kernel &kernel, const FormulaSnapshot &snapshot,
                        const std::vector<Lit> &mapping,
                        const SubstitutionRedundancyParameters &parameters,
                        volatile sig_atomic_t *stop,
                        SymmetryEnumerationResult &result) {
  if (!signed_literal_permutation(kernel, mapping))
    return false;
  const SymmetryTrace trace;
  SymmetryClauseCheckStats stats;
  const bool valid = snapshot.clause_index.preserves_set_support(
      mapping, [&](uint64_t amount) {
        return charge(kernel, parameters, stop, result, amount);
      }, trace.detailed() ? &stats : nullptr);
  if (trace.detailed())
    trace.event("symmetry_exact_index",
                "fixed\t%llu\tmapped\t%llu\tvalid\t%u\tstatus\t%u",
                static_cast<unsigned long long>(stats.fixed_clauses),
                static_cast<unsigned long long>(stats.mapped_clauses),
                static_cast<unsigned>(valid), static_cast<unsigned>(result.status));
  return valid;
}

bool exact_generator_automorphism(const Kernel &kernel, const FormulaSnapshot &snapshot,
                                  const SparseLiteralPermutation &mapping,
                                  SymmetryClauseVisitWorkspace &workspace,
                                  const SubstitutionRedundancyParameters &parameters,
                                  volatile sig_atomic_t *stop,
                                  SymmetryEnumerationResult &result) {
  // Preserve the old generator predicate's uncharged signed-bijection check;
  // transversal validation has a separate, intentionally different currency.
  if (mapping.domain != 2ull * kernel.nvars ||
      !mapping.validate([&](Var variable) { return kernel_var_retired(kernel, variable); },
                        [](uint64_t) { return true; }))
    return false;
  const SymmetryTrace trace;
  SymmetryClauseCheckStats stats;
  const bool valid = snapshot.clause_index.preserves_set_moved_reference(
      mapping, mapping.moved, workspace,
      [&](uint64_t amount) { return charge(kernel, parameters, stop, result, amount); },
      trace.detailed() ? &stats : nullptr);
  if (trace.detailed())
    trace.event("symmetry_exact_index",
                "fixed\t%llu\tmapped\t%llu\tvalid\t%u\tstatus\t%u",
                static_cast<unsigned long long>(stats.fixed_clauses),
                static_cast<unsigned long long>(stats.mapped_clauses),
                static_cast<unsigned>(valid), static_cast<unsigned>(result.status));
  return valid;
}

bool exact_sparse_automorphism(const Kernel &kernel, const FormulaSnapshot &snapshot,
                               const SparseLiteralPermutation &mapping,
                               SymmetryClauseVisitWorkspace &workspace,
                               const SubstitutionRedundancyParameters &parameters,
                               volatile sig_atomic_t *stop,
                               SymmetryEnumerationResult &result) {
  if (mapping.domain != 2ull * kernel.nvars ||
      !mapping.validate([&](Var variable) { return kernel_var_retired(kernel, variable); },
          [&](uint64_t amount) { return charge(kernel, parameters, stop, result, amount); }))
    return false;
  const SymmetryTrace trace;
  SymmetryClauseCheckStats stats;
  const bool valid = snapshot.clause_index.preserves_set_moved(
      mapping, mapping.moved, workspace,
      [&](uint64_t amount) { return charge(kernel, parameters, stop, result, amount); },
      trace.detailed() ? &stats : nullptr);
  if (trace.detailed())
    trace.event("symmetry_exact_sparse", "support\t%zu\tfixed\t%llu\tmapped\t%llu\tvalid\t%u\tstatus\t%u",
        mapping.moved.size(), static_cast<unsigned long long>(stats.fixed_clauses),
        static_cast<unsigned long long>(stats.mapped_clauses), static_cast<unsigned>(valid),
        static_cast<unsigned>(result.status));
  return valid;
}

struct EligibleClause {
  std::vector<Lit> literals;
  bool amo = false;
};

bool eligible_clauses(const Kernel &kernel, const FormulaSnapshot &snapshot,
                      const SubstitutionRedundancyParameters &parameters,
                      volatile sig_atomic_t *stop,
                      SymmetryEnumerationResult &enumeration,
                      std::vector<EligibleClause> &result) {
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
        if (!charge(kernel, parameters, stop, enumeration, 1))
          return false;
        if (!binaries.count(kernel_binary_key(negate(clause[first]),
                                               negate(clause[second])))) {
          amo = false;
          break;
        }
      }
    result.push_back({std::move(clause), amo});
  }
  apply_order(result, settings.clause_order,
              static_cast<uint64_t>(settings.seed) ^ 0x434c41555345534full);
  if (settings.clause_selection != SymmetryClauseSelection::InputOrder)
    std::stable_sort(
        result.begin(), result.end(), [&](const auto &first, const auto &second) {
          if (settings.clause_selection ==
                  SymmetryClauseSelection::AmoThenShortest &&
              first.amo != second.amo)
            return first.amo > second.amo;
          return first.literals.size() < second.literals.size();
        });
  uint64_t literal_seed =
      static_cast<uint64_t>(settings.seed) ^ 0x4c49544552414c53ull;
  for (auto &clause : result) {
    apply_order(clause.literals, settings.literal_order, literal_seed);
    splitmix64(literal_seed);
  }
  return true;
}

SubstitutionRedundancyInstance negative_fixing_instance(
    const Kernel &, Lit fixed_false, Lit witness_true,
    const SparseLiteralPermutation &image) {
  SubstitutionRedundancyInstance instance;
  instance.clause = {negate(fixed_false)};
  instance.witness = {negate(fixed_false), witness_true};
  image.positive_moves([&](Lit source, Lit target) {
    if (var_of(source) != var_of(fixed_false) && var_of(source) != var_of(witness_true))
      instance.substitution.emplace_back(source, target);
  });
  return instance;
}

template <class Visitor>
void positive_moves(const Kernel &kernel, const std::vector<Lit> &image, Visitor &&visit) {
  for (Var variable = 0; variable < kernel.nvars; ++variable) {
    const Lit source = make_lit(variable, false);
    if (image[source] != source) visit(source, image[source]);
  }
}

template <class Visitor>
void positive_moves(const Kernel &, const SparseLiteralPermutation &image, Visitor &&visit) {
  image.positive_moves(visit);
}

template <class Image>
SubstitutionRedundancyInstance binary_fixing_instance(
    const Kernel &kernel, Lit representative, Lit other,
    const Image &image) {
  SubstitutionRedundancyInstance instance;
  // The representative is the DSR pivot and the inverse transversal maps it
  // to `other`. Under the negated candidate, `other` is true, so the image of
  // every earlier cut in this level is immediately satisfied. Pivot order is
  // semantic in DSR; reversing these two literals is not an equivalent proof.
  instance.clause = {representative, negate(other)};
  instance.symmetry_chain_certificate = true;
  positive_moves(kernel, image, [&](Lit source, Lit target) {
    instance.substitution.emplace_back(source, target);
  });
  return instance;
}

template <class Image>
SubstitutionRedundancyInstance orbit_cut_instance(
    const Kernel &kernel, Lit representative, Lit other,
    const Image &image) {
  SubstitutionRedundancyInstance instance;
  // Complementary to clausal fixing: under the negated candidate,
  // representative is true and `other` is false. The inverse transversal maps
  // representative to `other`, satisfying every earlier member of this level.
  instance.clause = {negate(representative), other};
  instance.symmetry_chain_certificate = true;
  positive_moves(kernel, image, [&](Lit source, Lit target) {
    instance.substitution.emplace_back(source, target);
  });
  return instance;
}

template <class Image>
SubstitutionRedundancyInstance symmetry_generator_unit_instance(
    const Kernel &kernel, Lit representative, const Image &image) {
  SubstitutionRedundancyInstance instance;
  instance.clause = {representative};
  instance.symmetry_chain_certificate = true;
  // Keep DSR's implicit true image for the unit pivot. The remaining mapping
  // is the negation-orbit witness from an exact automorphism which maps the
  // representative to its complement.
  positive_moves(kernel, image, [&](Lit source, Lit target) {
    if (var_of(source) == var_of(representative)) return;
    instance.substitution.emplace_back(source, target);
  });
  return instance;
}

struct GeneratorFixingStats {
  uint64_t negation_units = 0;
  uint64_t binary_units = 0;
  bool visitor_stopped = false;
};

// The large-domain owner avoids a Schreier chain. It orders exact generators
// by the rarest point in their support, takes complement moves first, removes
// every generator touching a new unit, and repeats that discipline for binary
// clauses. The local fixed set makes later witnesses preserve earlier units.
bool generator_fixing_scans(
    const Kernel &kernel, const FormulaSnapshot &snapshot,
    const SubstitutionRedundancyParameters &parameters,
    const std::vector<SparseLiteralPermutation> &generators,
    const OrbitopalInstanceVisitor &visitor, volatile sig_atomic_t *stop,
    SymmetryEnumerationResult &result, GeneratorFixingStats &stats) {
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
        if (!charge(kernel, parameters, stop, result, 1))
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
    // Match the owning implementation's score-only ordering. Detector order
    // remains the input order and is deliberately not an extra tie-break.
    std::sort(order.begin(), order.end(), [&](size_t first, size_t second) {
      return rarity[first] < rarity[second];
    });
    return true;
  };
  const auto at_limit = [&] {
    return stats.negation_units + stats.binary_units ==
           parameters.graph_symmetry.maximum_fixing_instances;
  };

  if (!rank_active())
    return false;
  for (const size_t id : order) {
    if (at_limit())
      return true;
    const auto &image = generators[id];
    Lit applicable = INVALID_LIT;
    bool touches_fixed = false;
    for (const auto &[source, target] : image.moved) {
      if (!charge(kernel, parameters, stop, result, 1))
        return false;
      if (propagated[source]) {
        touches_fixed = true;
        break;
      }
      if (source == negate(target) && image[target] == source &&
          !kernel.value(source) &&
          (applicable == INVALID_LIT ||
           incidence[source] < incidence[applicable]))
        applicable = source;
    }
    if (touches_fixed) {
      active[id] = 0;
      continue;
    }
    if (applicable == INVALID_LIT)
      continue;
    active[id] = 0;
    propagated[applicable] = 1;
    ++stats.negation_units;
    ++result.instances;
    if (!visitor(symmetry_generator_unit_instance(kernel, applicable, image))) {
      stats.visitor_stopped = true;
      return true;
    }
  }

  // The owner runs binary fixing after negation fixing unconditionally
  // (satsuma.h); a formula can have no negation orbit and still fix thousands
  // of literals here.

  // The binary phase sees only generators which stabilize every unit emitted
  // by negation fixing, exactly as deleting marked generators in the owner.
  for (size_t id = 0; id < generators.size(); ++id) {
    if (!active[id])
      continue;
    for (const auto &[source, target] : generators[id].moved) {
      (void)target;
      if (!charge(kernel, parameters, stop, result, 1))
        return false;
      if (propagated[source]) {
        active[id] = 0;
        break;
      }
    }
  }
  std::fill(propagated.begin(), propagated.end(), 0);

  std::unordered_set<uint64_t> binaries;
  binaries.reserve(snapshot.clause_index.clauses().size());
  for (const auto &clause : snapshot.clause_index.clauses()) {
    if (clause.size() != 2)
      continue;
    if (!charge(kernel, parameters, stop, result, 1))
      return false;
    binaries.insert(kernel_binary_key(clause[0], clause[1]));
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
      if (!charge(kernel, parameters, stop, result, 1))
        return false;
      if (propagated[source]) {
        touches_fixed = true;
        break;
      }
      if (applicable == INVALID_LIT && source != target &&
          source != negate(target) && !kernel.value(source) &&
          !kernel.value(target) &&
          binaries.count(kernel_binary_key(source, target)))
        applicable = target;
    }
    if (touches_fixed || applicable == INVALID_LIT)
      continue;
    propagated[applicable] = 1;
    ++stats.binary_units;
    ++result.instances;
    // The witness is the generator itself: (source v applicable) is in the
    // formula, hence so is its image (applicable v image[applicable]), and
    // under the negated unit that image is true.
    if (!visitor(symmetry_generator_unit_instance(kernel, applicable, image))) {
      stats.visitor_stopped = true;
      return true;
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
  switch (status) {
  case symmetry_backend::Status::Complete:
    return SymmetryEnumerationStatus::Complete;
  case symmetry_backend::Status::Incomplete:
    return SymmetryEnumerationStatus::LimitReached;
  case symmetry_backend::Status::Interrupted:
    return SymmetryEnumerationStatus::Interrupted;
  case symmetry_backend::Status::Invalid:
    return SymmetryEnumerationStatus::Invalid;
  case symmetry_backend::Status::ResourceExhausted:
    return SymmetryEnumerationStatus::ResourceExhausted;
  case symmetry_backend::Status::Failed:
    return SymmetryEnumerationStatus::Failed;
  }
  return SymmetryEnumerationStatus::Failed;
}

} // namespace

uint64_t substitution_redundancy_symmetry_graph_vertices(
    const Kernel &kernel, const GraphSymmetryParameters &settings,
    uint64_t &work) {
  const uint64_t binaries = kernel.watches.binary_clauses();
  uint64_t large = 0;
  work = 0;
  for (Ref ref = 0; ref < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, ref);
    ++work;
    large += !clause->garbage();
    ref += clause_actual_words(clause);
  }
  bool direct = settings.binary_graph_mode == SymmetryBinaryGraphMode::DirectEdges;
  if (settings.binary_graph_mode == SymmetryBinaryGraphMode::Automatic) {
    const uint64_t threshold = static_cast<uint64_t>(kernel.nvars) *
                               settings.binary_graph_density_permille;
    direct = binaries >= threshold / 1000 + (threshold % 1000 != 0);
  }
  // Matches build_symmetry_graph: direct mode adds one negation vertex per
  // variable and represents first copies of binaries as edges.
  return (direct ? 3ull : 2ull) * kernel.nvars + large +
         (direct ? 0 : binaries);
}

SymmetryEnumerationResult substitution_redundancy_enumerate_symmetry(
    const Kernel &kernel,
    const SubstitutionRedundancyParameters &requested_parameters,
    const OrbitopalInstanceVisitor &visitor, volatile sig_atomic_t *stop,
    const SymmetryDiscoveryHints *hints,
    SymmetryEnumerationScheduling scheduling) {
  SymmetryEnumerationResult result;
  SubstitutionRedundancyParameters parameters = requested_parameters;
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
    // Zero is the public disabling value for every admission limit. No work
    // was truncated, so the disabled producer is a completed empty pass.
    return result;
  }

  // Every supported graph representation contains both literal vertices.
  // Reject an impossible caller-selected graph allowance before duplicating
  // the formula snapshot or running structured discovery inside this pass.
  // The same invocation would fail graph admission later; this is not an X7
  // guard or a restriction on separate structured/explicit applications.
  if (2ull * kernel.nvars > parameters.graph_symmetry.maximum_graph_vertices) {
    // The owner's graph component limit (satsuma.h:168-176).
    result.status = SymmetryEnumerationStatus::LimitReached;
    result.detection_limit_reached = true;
    return result;
  }

  FormulaSnapshot snapshot;
  if (!trace.measure("symmetry_snapshot", [&] {
        return snapshot_formula(kernel, parameters, stop, result, snapshot);
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
      if (hints->generators.size() > configured.maximum_generators) {
        result.status = SymmetryEnumerationStatus::LimitReached;
        return result;
      }
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
    if (discovery.status == OrbitopalEnumerationStatus::Interrupted) {
      result.status = SymmetryEnumerationStatus::Interrupted;
      return result;
    }
    structured_incomplete =
        discovery.status == OrbitopalEnumerationStatus::LimitReached;
    result.detection_limit_reached |= discovery.detection_limit_reached;
    result.allowance_limit_reached |= discovery.allowance_limit_reached;
    if (!configured.use_structured_generators)
      structured.generators.clear();
    if (!configured.stabilize_structured_variables)
      structured.fixed_variables.clear();
    // The structured producer shares this invocation's discovery allowance.
    // If it spent the last credit, generic graph construction and DejaVu
    // cannot publish a validated consequence: even the first subsequent
    // charge must fail.  End at the producer feedback boundary instead of
    // doing an unpublishable full-formula pass.  A caller that grants more
    // work starts a fresh typed invocation; an admitted group batch is not
    // involved yet.
    if (result.ticks >= parameters.maximum_detection_ticks) {
      result.status = SymmetryEnumerationStatus::LimitReached;
      result.allowance_limit_reached = true;
      return result;
    }
  }
  std::vector<SparseLiteralPermutation> generators;
  // Generator and transversal checks have different historical setup charges.
  // Their workspaces must remain separate to keep budgets independent of order.
  SymmetryClauseVisitWorkspace generator_workspace;
  std::vector<Var> fixed_variables;
  for (const auto *advice : {hints, static_cast<const SymmetryDiscoveryHints *>(&structured)}) {
    if (!advice) continue;
    if (advice->generators.size() > configured.maximum_generators - generators.size()) {
      result.status = SymmetryEnumerationStatus::LimitReached;
      return result;
    }
    for (const auto &image : advice->generators) {
      if (!charge(kernel, parameters, stop, result, image.size()))
        return result;
      if (!exact_automorphism(kernel, snapshot, image, parameters, stop, result)) {
        if (result.status == SymmetryEnumerationStatus::Complete)
          result.status = SymmetryEnumerationStatus::Invalid;
        return result;
      }
      SparseLiteralPermutation sparse{literal_count, {}};
      for (Lit source = 0; source < literal_count; ++source)
        if (source != image[source]) sparse.moved.emplace_back(source, image[source]);
      generators.push_back(std::move(sparse));
    }
    for (const Var variable : advice->fixed_variables) {
      if (!charge(kernel, parameters, stop, result, 1)) return result;
      if (variable >= kernel.nvars) {
        result.status = SymmetryEnumerationStatus::Invalid;
        return result;
      }
      fixed_variables.push_back(variable);
    }
  }
  structured.generators.clear();
  trace.event("symmetry_structured_handoff", "generators\t%zu\tfixed_variables\t%zu",
              generators.size(), fixed_variables.size());
  SymmetryGraphStorage graph_storage;
  if (!trace.measure("symmetry_graph", [&] {
        return build_symmetry_graph(kernel, parameters, snapshot, stop, result,
                                    graph_storage);
      }))
    return result;

  for (const Var variable : fixed_variables) {
    // These colors are disjoint from every gadget color and the other
    // literal colors. Both signs are fixed independently.
    graph_storage.colors[2 * variable] = 3 + 2 * variable;
    graph_storage.colors[2 * variable + 1] = 3 + 2 * variable + 1;
  }

  trace.event("symmetry_graph_ready", "clauses\t%zu\tvertices\t%u\tedges\t%llu",
              snapshot.clause_index.clauses().size(), graph_storage.vertices,
              static_cast<unsigned long long>(graph_storage.edge_count));
  diagnostics::capture_graph(graph_storage.vertices, graph_storage.edge_count,
      [&](uint64_t vertex) { return graph_storage.colors[vertex]; },
      [&](auto edge) {
        for (size_t i = 0; i < graph_storage.edges.size(); i += 2)
          edge(graph_storage.edges[i], graph_storage.edges[i + 1]);
      });
  symmetry_backend::DetectionSettings settings;
  settings.seed = configured.seed;
  settings.error_bound = configured.error_bound;
  settings.search_budget = configured.backend_search_budget;
  settings.support_budget = configured.backend_support_budget;
  settings.component_size_limit = configured.backend_component_size_limit;
  settings.maximum_generators = configured.maximum_generators - generators.size();
  settings.returned_prefix_size = literal_count;
  settings.sparse_output = true;
  settings.decompose = configured.decompose;
  settings.prefer_depth_first = configured.prefer_depth_first;
  settings.strong_certification = configured.strong_certification;
  settings.preserve_generator_order = configured.preserve_detector_order;
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
  // Under progress scheduling an exact generator earns the window which pays
  // for its validation, so the entry opportunity admits one generator whenever
  // the hard allowance affords it. Without that floor a formula whose single
  // validation quantum exceeds half the entry window can never start the
  // feedback it is entitled to.
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
  const symmetry_backend::Graph graph{
      graph_storage.vertices, graph_storage.edge_count,
      graph_storage.colors.data(), graph_storage.degrees.data(),
      graph_storage.edges.data()};
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
  // DejaVu's own limits (symmetries.h:579) stop the detector before it
  // completes its group. A stop at the generator cap derived from the tick
  // allowance (or no affordable generator at all) is an allowance stop.
  if (detection == symmetry_backend::Status::Incomplete) {
    if (!settings.initial_generators ||
        (detector_work_capped && proposed.count >= settings.maximum_generators))
      result.allowance_limit_reached = true;
    else
      result.detection_limit_reached = true;
  }
  result.status = convert(detection);
  // Completeness of the remaining subgroup says nothing about completeness
  // of the combined basis in the unrestricted original automorphism group.
  // The physical graph may distinguish duplicate counts even though exact
  // semantic validation admits their set symmetries. Its completeness claim
  // cannot establish the full set-automorphism group in that case.
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
  // Generator fixing derives each unit from one exactly validated generator,
  // so a work-capped prefix loses only later generators. The discard concerns
  // a chain planned over an arbitrary subgroup.
  if (detector_work_capped && !large_domain_generator_fixing &&
      detection == symmetry_backend::Status::Incomplete) {
    if (!scheduling.progress_ticks_per_generator && proposed.count &&
        proposed.count == settings.maximum_generators)
      result.discovery_progress_units = proposed.count;
    // The derived cap proved that the shared allowance cannot validate an
    // unrestricted detector stream and still give group planning one useful
    // work quantum.  A prefix is valid group input, but publishing its
    // arbitrary callback-order subgroup can perturb search after the producer
    // has already reported that it could not finish.  Keep independently
    // validated structured/hinted generators, but discard this work-limited
    // generic prefix.  The finite cap still makes the probe self-terminating.
    symmetry_backend::release(proposed);
    result.complete_group = false;
    if (generators.empty()) {
      // This is the same accounting outcome as charge() declining an atomic
      // operation whose upper bound does not fit: the local allowance is no
      // longer useful to this producer.  Saturation also prevents the shared
      // startup scope from spending the rejected remainder on another
      // formula-wide structured retry after this no-progress result.
      result.ticks = parameters.maximum_detection_ticks;
      result.status = SymmetryEnumerationStatus::LimitReached;
      return result;
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
        for (Lit source = 0; source < literal_count; ++source) {
          const Lit target = proposed.images[row * literal_count + source];
          if (source != target) image.moved.emplace_back(source, target);
        }
    }
    if (literal_image && exact_generator_automorphism(kernel, snapshot, image,
                           generator_workspace, parameters, stop, result))
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
      generators.empty()) {
    if (result.status == SymmetryEnumerationStatus::Complete &&
        detection_incomplete)
      result.status = SymmetryEnumerationStatus::LimitReached;
    return result;
  }

  if (large_domain_generator_fixing) {
    GeneratorFixingStats fixing;
    // Each returned instance is a proof-checked root unit. Cut-admission policy
    // may still withhold a lone nonforcing binary below, but it never suppresses
    // a valid unit merely because this pass found no second consequence.
    const bool completed = trace.measure("symmetry_generator_fixing", [&] {
      return generator_fixing_scans(kernel, snapshot, parameters, generators,
                                    visitor, stop, result, fixing);
    });
    trace.event("symmetry_generator_fixing_result",
                "negation_units\t%llu\tbinary_units\t%llu\tvisitor_stopped\t%u",
                static_cast<unsigned long long>(fixing.negation_units),
                static_cast<unsigned long long>(fixing.binary_units),
                static_cast<unsigned>(fixing.visitor_stopped));
    if (!completed)
      return result;
    if (result.status == SymmetryEnumerationStatus::Complete &&
        detection_incomplete)
      result.status = SymmetryEnumerationStatus::LimitReached;
    return result;
  }

  std::vector<EligibleClause> clauses;
  if (!trace.measure("symmetry_eligible_clauses", [&] {
        return eligible_clauses(kernel, snapshot, parameters, stop, result, clauses);
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
        result.ticks >= parameters.maximum_detection_ticks) {
      result.status = SymmetryEnumerationStatus::LimitReached;
      result.allowance_limit_reached = true;
      return result;
    }
    const uint64_t planning_allowance =
        parameters.maximum_detection_ticks - result.ticks;
    SubstitutionRedundancyParameters finalization_parameters = parameters;
    if (scheduling.finish_admitted_group)
      finalization_parameters.maximum_detection_ticks = UINT64_MAX;
    const SubstitutionRedundancyParameters &group_parameters =
        scheduling.finish_admitted_group ? finalization_parameters : parameters;
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
    if (fixing_status == SymmetryEnumerationStatus::LimitReached) {
      result.status = fixing_status;
      // The owner's Schreier cost limit (symmetries.h:1468, 3099) is the
      // configured planning limit. Planning cut short by a smaller remaining
      // tick allowance stopped at that allowance instead.
      const uint64_t configured_planning =
          configured.chain_planning_tick_limit
              ? configured.chain_planning_tick_limit
              : UINT64_MAX;
      if (scheduling.finish_admitted_group &&
          planning_allowance < configured_planning &&
          fixings.ticks >= planning_allowance)
        result.allowance_limit_reached = true;
      else
        result.detection_limit_reached = true;
    }
    group_ticks = fixings.ticks;
    if (scheduling.finish_admitted_group) {
      if (!charge(kernel, group_parameters, stop, result, group_ticks)) {
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
          if (!charge(kernel, group_parameters, stop, result, end - begin)) {
            valid_chain = false;
            break;
          }
          image.moved.reserve(end - begin);
          for (auto at = begin; at < end; ++at)
            image.moved.emplace_back(fixings.image_points[at], fixings.images[at]);
        } else {
          if (!fixings.images) { valid_chain = false; break; }
          if (!charge(kernel, group_parameters, stop, result, literal_count)) {
            valid_chain = false;
            break;
          }
          for (Lit literal = 0; literal < literal_count; ++literal) {
            const Lit target = fixings.images[row * literal_count + literal];
            if (target != literal) image.moved.emplace_back(literal, target);
          }
        }
      }
      // Every materialized backend map crosses an untrusted boundary. It must
      // be an exact automorphism of the detector snapshot and map the selected
      // base to the advertised orbit point before structural certification.
      if (kind != 1 &&
          (!exact_sparse_automorphism(kernel, snapshot, image, visit_workspace,
                                     group_parameters, stop,
                                     result) ||
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

    // A later level's witness must preserve every earlier complete cut orbit.
    // Fixing its base and mapping the orbit into itself makes each earlier cut
    // image either another already-published cut or a tautology.
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
      // Kinds 1 and 3 materialize as syntactic units.  The backend normally
      // chooses both endpoints of kinds 0 and 2 from the open root orbit, but
      // make admission depend on the checked clause's live meaning instead of
      // that producer invariant: a binary with exactly one root-false literal
      // is unit and must never be withheld as a nonforcing cut.
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
                ? binary_fixing_instance(kernel, row.representative, row.other,
                                         image)
            : row.kind == 1
                ? representative_unit_instance(row.representative)
            : row.kind == 2
                ? orbit_cut_instance(kernel, row.representative, row.other,
                                     image)
                : symmetry_generator_unit_instance(kernel, row.representative,
                                                   image);
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

  // A stabilizer batch is applied before another pass re-detects the group on
  // the strengthened root formula.  If that batch is empty, try direct
  // generator involutions.  Each accepted unit becomes a local fixed point;
  // later proposals in the same batch must stabilize every earlier one.
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
            const bool select_canonical = (splitmix64(pair_seed) & 1u) == 0;
            if (canonical != select_canonical)
              continue;
          }
          ++result.instances;
          ++generated;
          locally_fixed[fixed_false] = 1;
          values[fixed_false] = -1;
          values[negate(fixed_false)] = 1;
          if (!visitor(negative_fixing_instance(
                  kernel, fixed_false, witness_true, image))) {
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
  result.ticks = result.ticks > UINT64_MAX - group_ticks
                     ? UINT64_MAX
                     : result.ticks + group_ticks;
  if (result.status == SymmetryEnumerationStatus::Complete &&
      detection_incomplete)
    result.status = SymmetryEnumerationStatus::LimitReached;
  return result;
}

} // namespace tactix
