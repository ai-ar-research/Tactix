#include "rules/semantic_extension/ttf_truth_table.hpp"

#include <algorithm>
#include <climits>
#include <map>
#include <set>

namespace tactix::ttf_truth_table {
namespace {

uint32_t var(int32_t lit) {
  return static_cast<uint32_t>(lit < 0 ? -int64_t(lit) : lit);
}

struct Meter {
  const Limits &limits;
  Charge charge;
  void *context;
  uint64_t work = 0;
  Status status = Status::Recognized;
  bool take(uint64_t amount) {
    if (work > limits.maximum_work || amount > limits.maximum_work - work) {
      status = Status::LimitExceeded;
      return false;
    }
    if (charge && !charge(context, amount)) {
      status = Status::Interrupted;
      return false;
    }
    work += amount;
    return true;
  }
};

struct Gate {
  int32_t root = 0;
  std::vector<int32_t> inputs;
  uint64_t backward = 0;
  std::vector<uint64_t> forwards;
};

using ClauseMap = std::map<std::vector<int32_t>, const Premise *>;

const Premise *find(const ClauseMap &clauses, std::vector<int32_t> key) {
  std::sort(key.begin(), key.end());
  const auto it = clauses.find(key);
  return it == clauses.end() ? nullptr : it->second;
}

} // namespace

Status recognize(const std::vector<Premise> &source,
                 const std::vector<uint64_t> &amo_owned_occurrences,
                 const Limits &limits, Charge charge, void *charge_context,
                 Image &out) {
  if (!limits.maximum_clauses || !limits.maximum_literals ||
      !limits.maximum_work || !limits.maximum_stars ||
      !limits.maximum_domains || !limits.maximum_domain_leaves)
    return Status::LimitExceeded;
  if (source.empty()) return Status::NotRecognized;
  if (source.size() > limits.maximum_clauses)
    return Status::LimitExceeded;
  Meter meter{limits, charge, charge_context};
  ClauseMap clauses;
  std::map<uint64_t, const Premise *> by_occurrence;
  std::map<int32_t, std::vector<const Premise *>> occurrences;
  std::set<uint64_t> native_proof_ids;
  uint64_t literals = 0;
  for (const Premise &p : source) {
    if (!p.occurrence || !p.native_proof_id || p.literals.empty() ||
        !by_occurrence.emplace(p.occurrence, &p).second ||
        !native_proof_ids.insert(p.native_proof_id).second)
      return Status::NotRecognized;
    if (literals > limits.maximum_literals ||
        p.literals.size() > limits.maximum_literals - literals)
      return Status::LimitExceeded;
    literals += p.literals.size();
    if (!meter.take(uint64_t{1} + p.literals.size())) return meter.status;
    std::vector<int32_t> key = p.literals;
    for (int32_t lit : key)
      if (!lit || lit == INT32_MIN) return Status::NotRecognized;
    std::sort(key.begin(), key.end());
    if (std::adjacent_find(key.begin(), key.end()) != key.end())
      return Status::NotRecognized;
    for (size_t i = 1; i < key.size(); ++i)
      if (var(key[i - 1]) == var(key[i])) return Status::NotRecognized;
    if (!clauses.emplace(key, &p).second) return Status::NotRecognized;
    std::set<int32_t> seen;
    for (int32_t lit : key)
      if (seen.insert(static_cast<int32_t>(var(lit))).second)
        occurrences[static_cast<int32_t>(var(lit))].push_back(&p);
  }
  std::set<uint64_t> amo(amo_owned_occurrences.begin(),
                         amo_owned_occurrences.end());
  if (amo.size() != amo_owned_occurrences.size())
    return Status::NotRecognized;
  for (uint64_t id : amo)
    if (!by_occurrence.count(id)) return Status::NotRecognized;

  std::map<int32_t, Gate> gates;
  for (const auto &[key, p] : clauses) {
    if (!meter.take(1)) return meter.status;
    if (key.size() < 3) continue;
    std::vector<int32_t> positive;
    int32_t root = 0;
    uint32_t negatives = 0;
    for (int32_t lit : key)
      if (lit < 0) {
        root = -lit;
        ++negatives;
      } else {
        positive.push_back(lit);
      }
    if (negatives != 1 || positive.size() + 1 != key.size()) continue;
    Gate gate{root, positive, p->occurrence, {}};
    for (int32_t input : positive) {
      const Premise *forward = find(clauses, {-input, root});
      if (!forward) {
        gate.forwards.clear();
        break;
      }
      gate.forwards.push_back(forward->occurrence);
    }
    if (gate.forwards.size() == positive.size() &&
        !gates.emplace(root, std::move(gate)).second)
      return Status::NotRecognized;
  }

  std::map<std::vector<int32_t>, std::vector<std::pair<int32_t, uint64_t>>>
      positive_supports;
  for (const auto &[key, p] : clauses) {
    if (!meter.take(1)) return meter.status;
    std::vector<int32_t> positive;
    int32_t negative = 0;
    uint32_t negatives = 0;
    for (int32_t lit : key)
      if (lit < 0) {
        negative = -lit;
        ++negatives;
      } else {
        positive.push_back(lit);
      }
    if (negatives == 1 && positive.size() >= 3)
      positive_supports[positive].push_back({negative, p->occurrence});
  }
  std::vector<std::vector<int32_t>> supports;
  size_t support_arity = 0;
  for (const auto &[positive, uses] : positive_supports) {
    if (!meter.take(1)) return meter.status;
    if (uses.size() < 2) continue;
    if (!support_arity) support_arity = positive.size();
    if (positive.size() != support_arity) return Status::NotRecognized;
    supports.push_back(positive);
  }
  if (supports.size() < 3) return Status::NotRecognized;
  if (supports.size() > limits.maximum_stars) return Status::LimitExceeded;

  std::set<uint64_t> classified;
  std::map<uint64_t, RootUse> roles;
  std::vector<Star> stars;
  std::vector<std::map<int32_t, AtomClass>> star_atoms;
  std::set<int32_t> atom_pool;
  for (const std::vector<int32_t> &support : supports) {
    if (!meter.take(uint64_t{1} + support.size())) return meter.status;
    Star star;
    star.variables = support;
    for (int32_t variable : support) {
      const Premise *unit = find(clauses, {-variable});
      if (!unit) continue;
      if (star.fixed_false) return Status::NotRecognized;
      star.fixed_false = variable;
      star.fixed_false_premise = unit->occurrence;
      classified.insert(unit->occurrence);
    }
    if (!star.fixed_false) return Status::NotRecognized;
    for (const auto &[root, gate] : gates) {
      if (!meter.take(1)) return meter.status;
      if (gate.inputs == support) {
        if (star.participation_root) return Status::NotRecognized;
        star.participation_root = root;
        classified.insert(gate.backward);
        roles[gate.backward] = RootUse::Definition;
        for (uint64_t id : gate.forwards) {
          classified.insert(id);
          roles[id] = RootUse::Definition;
        }
    }
      }
    std::map<int32_t, AtomClass> atoms;
    const auto &uses = positive_supports.at(support);
    for (auto [atom, premise] : uses) {
      if (!meter.take(1)) return meter.status;
      if (atom == star.participation_root) continue;
      AtomClass entry;
      entry.atom = atom;
      entry.nonzero = true;
      entry.nonzero_premise = premise;
      atoms.emplace(atom, std::move(entry));
      atom_pool.insert(atom);
      classified.insert(premise);
      roles[premise] = RootUse::NonzeroLink;
    }
    if (star.participation_root) {
      for (const Premise *p : occurrences[star.participation_root]) {
        if (!meter.take(uint64_t{1} + p->literals.size())) return meter.status;
        std::vector<int32_t> key = p->literals;
        std::sort(key.begin(), key.end());
        if (key.size() != 2 || key[0] >= 0 || key[1] >= 0) continue;
        int32_t atom = 0;
        if (key[0] == -star.participation_root) atom = -key[1];
        if (key[1] == -star.participation_root) atom = -key[0];
        if (!atom) continue;
        AtomClass &entry = atoms[atom];
        if (entry.atom && entry.nonzero) return Status::NotRecognized;
        entry.atom = atom;
        entry.zero_encoding = ZeroEncoding::GuardedByParticipation;
        entry.zero_premises = {p->occurrence};
        atom_pool.insert(atom);
        classified.insert(p->occurrence);
        roles[p->occurrence] = RootUse::ParticipationGuard;
      }
    }
    stars.push_back(std::move(star));
    star_atoms.push_back(std::move(atoms));
  }

  for (const auto &[root, gate] : gates)
    if (gate.inputs.size() == 2) atom_pool.insert(root);
  for (const auto &[key, p] : clauses)
    if (key.size() == 1 && key[0] > 0) atom_pool.insert(key[0]);

  for (uint32_t s = 0; s < stars.size(); ++s) {
    for (int32_t atom : atom_pool) {
      if (!meter.take(uint64_t{1} + stars[s].variables.size()))
        return meter.status;
      std::vector<uint64_t> premises;
      for (int32_t variable : stars[s].variables) {
        const Premise *p = find(clauses, {-atom, -variable});
        if (!p) {
          premises.clear();
          break;
        }
        premises.push_back(p->occurrence);
      }
      if (premises.empty()) continue;
      if (star_atoms[s].count(atom)) return Status::NotRecognized;
      AtomClass entry;
      entry.atom = atom;
      entry.zero_encoding = ZeroEncoding::DirectStar;
      entry.zero_premises = premises;
      star_atoms[s].emplace(atom, std::move(entry));
      for (uint64_t id : premises) {
        classified.insert(id);
        roles[id] = RootUse::ZeroLink;
      }
    }
  }

  std::map<std::vector<int32_t>, std::vector<uint32_t>> domain_stars;
  for (uint32_t s = 0; s < stars.size(); ++s) {
    std::set<int32_t> leaves;
    for (auto &[atom, entry] : star_atoms[s]) {
      if (!meter.take(1)) return meter.status;
      const auto gate = gates.find(atom);
      entry.leaves = gate != gates.end() && gate->second.inputs.size() == 2
                         ? gate->second.inputs
                         : std::vector<int32_t>{atom};
      for (int32_t leaf : entry.leaves)
        if (!leaves.insert(leaf).second) return Status::NotRecognized;
      stars[s].atoms.push_back(entry);
    }
    if (leaves.size() < 2 || leaves.size() > limits.maximum_domain_leaves)
      return leaves.size() > limits.maximum_domain_leaves
                 ? Status::LimitExceeded
                 : Status::NotRecognized;
    domain_stars[std::vector<int32_t>(leaves.begin(), leaves.end())].push_back(s);
  }
  if (domain_stars.size() > limits.maximum_domains)
    return Status::LimitExceeded;

  std::vector<Domain> domains;
  size_t width = 0, domain_size = 0;
  std::set<uint32_t> assigned_stars;
  for (const auto &[leaves, indices] : domain_stars) {
    if (!meter.take(uint64_t{1} + indices.size())) return meter.status;
    if (!width) width = indices.size();
    if (!domain_size) domain_size = leaves.size();
    if (indices.size() != width || leaves.size() != domain_size)
      return Status::NotRecognized;
    Domain domain;
    domain.leaves = leaves;
    domain.stars = indices;
    for (uint32_t s : indices) {
      assigned_stars.insert(s);
      for (const AtomClass &atom : stars[s].atoms) {
        const Premise *unit = find(clauses, {atom.atom});
        if (!unit) continue;
        if (domain.forced_root && domain.forced_root != atom.atom)
          return Status::NotRecognized;
        domain.forced_root = atom.atom;
        domain.forced_unit_premise = unit->occurrence;
      }
    }
    if (!domain.forced_root || !gates.count(domain.forced_root) ||
        gates.at(domain.forced_root).inputs.size() != 2)
      return Status::NotRecognized;
    classified.insert(domain.forced_unit_premise);
    roles[domain.forced_unit_premise] = RootUse::Assertion;
    domains.push_back(std::move(domain));
  }
  if (assigned_stars.size() != stars.size() || domains.size() < 3)
    return Status::NotRecognized;

  // Recover coordinate frames from star intersections.  In a pair-variable
  // incidence frame, distinct domains intersect in two directed pair atoms;
  // fixed-false diagonals occur in one star and every active atom in two.
  std::vector<uint8_t> visited(stars.size());
  std::vector<Coordinate> coordinates;
  for (uint32_t start = 0; start < stars.size(); ++start) {
    if (visited[start]) continue;
    Coordinate coordinate;
    std::vector<uint32_t> queue{start};
    visited[start] = 1;
    for (size_t q = 0; q < queue.size(); ++q) {
      const uint32_t a = queue[q];
      coordinate.stars.push_back(a);
      std::set<int32_t> av(stars[a].variables.begin(), stars[a].variables.end());
      for (uint32_t b = 0; b < stars.size(); ++b) {
        if (!meter.take(1)) return meter.status;
        if (visited[b]) continue;
        bool intersects = false;
        for (int32_t v : stars[b].variables)
          if (av.count(v)) {
            intersects = true;
            break;
          }
        if (intersects) {
          visited[b] = 1;
          queue.push_back(b);
        }
      }
    }
    coordinates.push_back(std::move(coordinate));
  }
  if (coordinates.size() != width) return Status::NotRecognized;
  std::vector<uint32_t> star_domain(stars.size(), UINT32_MAX);
  for (uint32_t d = 0; d < domains.size(); ++d)
    for (uint32_t s : domains[d].stars) star_domain[s] = d;
  for (Coordinate &coordinate : coordinates) {
    if (coordinate.stars.size() != domains.size()) return Status::NotRecognized;
    if (domains.size() > (SIZE_MAX - 1) / 2 ||
        support_arity != 2 * (domains.size() - 1) + 1)
      return Status::NotRecognized;
    std::set<uint32_t> ds;
    std::map<int32_t, uint32_t> incidence;
    for (uint32_t s : coordinate.stars) {
      ds.insert(star_domain[s]);
      for (int32_t v : stars[s].variables) ++incidence[v];
    }
    if (ds.size() != domains.size()) return Status::NotRecognized;
    for (uint32_t i = 0; i < coordinate.stars.size(); ++i)
      for (uint32_t j = i + 1; j < coordinate.stars.size(); ++j) {
        if (!meter.take(uint64_t{1} +
                        stars[coordinate.stars[j]].variables.size()))
          return meter.status;
        uint32_t intersection = 0;
        std::set<int32_t> left(stars[coordinate.stars[i]].variables.begin(),
                               stars[coordinate.stars[i]].variables.end());
        for (int32_t v : stars[coordinate.stars[j]].variables)
          intersection += left.count(v);
        if (intersection != 2) return Status::NotRecognized;
      }
    for (const auto &[v, count] : incidence) {
      bool fixed_false = false;
      for (uint32_t s : coordinate.stars)
        fixed_false |= stars[s].fixed_false == v;
      if ((fixed_false && count != 1) || (!fixed_false && count != 2))
        return Status::NotRecognized;
    }
  }

  std::set<int32_t> census_roots;
  for (const Domain &d : domains) census_roots.insert(d.forced_root);
  for (const Star &s : stars)
    if (s.participation_root) census_roots.insert(s.participation_root);
  for (const Star &s : stars)
    for (const AtomClass &a : s.atoms)
      if (gates.count(a.atom) && gates.at(a.atom).inputs.size() == 2)
        census_roots.insert(a.atom);
  for (int32_t root : census_roots) {
    const Gate &gate = gates.at(root);
    classified.insert(gate.backward);
    roles.emplace(gate.backward, RootUse::Definition);
    for (uint64_t id : gate.forwards) {
      classified.insert(id);
      roles.emplace(id, RootUse::Definition);
    }
  }

  std::vector<RootCensus> censuses;
  for (int32_t root : census_roots) {
    RootCensus census;
    census.root = root;
    for (const Premise *p : occurrences[root]) {
      if (!meter.take(1)) return meter.status;
      const auto role = roles.find(p->occurrence);
      const bool shared = amo.count(p->occurrence);
      if (role == roles.end() && !shared) return Status::NotRecognized;
      census.occurrences.push_back(
          {p->occurrence,
           role == roles.end() ? RootUse::SharedAmo : role->second, shared});
      classified.insert(p->occurrence);
    }
    std::sort(census.occurrences.begin(), census.occurrences.end(),
              [](const CensusEntry &a, const CensusEntry &b) {
                return a.occurrence < b.occurrence;
              });
    censuses.push_back(std::move(census));
  }

  Image image;
  image.stars = std::move(stars);
  image.domains = std::move(domains);
  image.coordinates = std::move(coordinates);
  image.root_census = std::move(censuses);
  for (uint64_t id : classified) image.classified_premises.push_back(*by_occurrence.at(id));
  image.inspected_work = meter.work;
  out = std::move(image);
  return Status::Recognized;
}

Status plan_parity_frontier(const Image &image, const Limits &limits,
                            Charge charge, void *charge_context,
                            ParityFrontierPlan &out) {
  if (!limits.maximum_work || !limits.maximum_plan_additions ||
      !limits.maximum_plan_literals)
    return Status::LimitExceeded;
  Meter meter{limits, charge, charge_context};
  for (uint32_t c = 0; c < image.coordinates.size(); ++c) {
    const Coordinate &coordinate = image.coordinates[c];
    if (!(coordinate.stars.size() & 1)) continue;
    bool all_nonzero = true;
    std::vector<const AtomClass *> forced;
    for (uint32_t s : coordinate.stars) {
      const Star &star = image.stars[s];
      const Domain *domain = nullptr;
      for (const Domain &candidate : image.domains)
        if (std::find(candidate.stars.begin(), candidate.stars.end(), s) !=
            candidate.stars.end()) {
          domain = &candidate;
          break;
        }
      if (!domain) return Status::NotRecognized;
      const auto atom = std::find_if(
          star.atoms.begin(), star.atoms.end(), [&](const AtomClass &entry) {
            return entry.atom == domain->forced_root;
          });
      if (atom == star.atoms.end() || !atom->nonzero) {
        all_nonzero = false;
        break;
      }
      forced.push_back(&*atom);
    }
    if (!all_nonzero) continue;
    if (coordinate.stars.size() > limits.maximum_plan_additions)
      return Status::LimitExceeded;
    ParityFrontierPlan plan;
    plan.coordinate = c;
    uint64_t literal_count = 0;
    for (uint32_t i = 0; i < coordinate.stars.size(); ++i) {
      if (!meter.take(1)) return meter.status;
      const uint32_t s = coordinate.stars[i];
      const Star &star = image.stars[s];
      const Domain *domain = nullptr;
      uint32_t domain_index = 0;
      for (; domain_index < image.domains.size(); ++domain_index)
        if (std::find(image.domains[domain_index].stars.begin(),
                      image.domains[domain_index].stars.end(), s) !=
            image.domains[domain_index].stars.end()) {
          domain = &image.domains[domain_index];
          break;
        }
      PlannedAddition addition;
      addition.domain = domain_index;
      addition.star = s;
      for (int32_t variable : star.variables)
        if (variable != star.fixed_false) addition.clause.push_back(variable);
      literal_count += addition.clause.size();
      if (literal_count > limits.maximum_plan_literals)
        return Status::LimitExceeded;
      addition.rup_premises = {domain->forced_unit_premise,
                               forced[i]->nonzero_premise,
                               star.fixed_false_premise};
      plan.required_amo_leaf_sets.push_back(addition.clause);
      plan.additions.push_back(std::move(addition));
    }
    plan.inspected_work = meter.work;
    out = std::move(plan);
    return Status::Recognized;
  }
  return Status::NotRecognized;
}

} // namespace tactix::ttf_truth_table
