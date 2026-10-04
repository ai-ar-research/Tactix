#include "rules/lucky/tseitin_cycle_plan.hpp"

#include <algorithm>
#include <map>
#include <limits>
#include <queue>
#include <set>
#include <utility>

namespace tactix::tseitin_cycle {
namespace {

using Endpoints = std::vector<Vertex>;

struct CheckedInput {
  std::vector<VertexEquation> equations;
  std::map<Edge, Endpoints> endpoints;
  std::set<Edge> unavailable;
};

class Work {
  uint64_t limit_;
  bool (*interrupted_)(void *) = nullptr;
  void *interrupt_state_ = nullptr;
  uint64_t used_ = 0;
  bool exhausted_ = false;
  bool was_interrupted_ = false;

public:
  explicit Work(uint64_t limit, bool (*interrupted)(void *) = nullptr,
                void *interrupt_state = nullptr)
      : limit_(limit), interrupted_(interrupted),
        interrupt_state_(interrupt_state) {}
  bool spend(uint64_t amount = 1) {
    if (interrupted_ && interrupted_(interrupt_state_)) {
      was_interrupted_ = true;
      return false;
    }
    if (amount > limit_ - used_) {
      exhausted_ = true;
      return false;
    }
    used_ += amount;
    return true;
  }
  uint64_t used() const { return used_; }
  bool exhausted() const { return exhausted_; }
  bool interrupted() const { return was_interrupted_; }
};

struct DisjointSet {
  std::vector<size_t> parent;
  explicit DisjointSet(size_t size) : parent(size) {
    for (size_t i = 0; i < size; ++i)
      parent[i] = i;
  }
  size_t find(size_t value) {
    while (parent[value] != value) {
      parent[value] = parent[parent[value]];
      value = parent[value];
    }
    return value;
  }
  bool join(size_t left, size_t right) {
    left = find(left);
    right = find(right);
    if (left == right)
      return false;
    if (left > right)
      std::swap(left, right);
    parent[right] = left;
    return true;
  }
};

DeclineReason check_input(const Input &input, CheckedInput &checked,
                          Work &work) {
  if (!input.occurrence_classification_complete)
    return DeclineReason::IncompleteOccurrenceClassification;
  if (input.equations.empty())
    return DeclineReason::EmptySystem;

  checked.equations = input.equations;
  for (Edge edge : input.unavailable_edges) {
    if (!work.spend())
      return DeclineReason::WorkLimit;
    if (edge <= 0 || !checked.unavailable.insert(edge).second)
      return DeclineReason::InvalidEdge;
  }
  std::sort(checked.equations.begin(), checked.equations.end(),
            [](const VertexEquation &a, const VertexEquation &b) {
              return a.vertex < b.vertex;
            });
  std::set<Vertex> vertices;
  std::map<std::pair<Edge, Vertex>, uint32_t> expected;
  bool odd_charge = false;
  for (VertexEquation &equation : checked.equations) {
    if (!work.spend())
      return DeclineReason::WorkLimit;
    if (!equation.vertex)
      return DeclineReason::InvalidVertex;
    if (!vertices.insert(equation.vertex).second)
      return DeclineReason::DuplicateVertex;
    odd_charge ^= equation.charge;
    std::sort(equation.edges.begin(), equation.edges.end());
    for (size_t i = 0; i < equation.edges.size(); ++i) {
      if (!work.spend())
        return DeclineReason::WorkLimit;
      const Edge edge = equation.edges[i];
      if (edge <= 0)
        return DeclineReason::InvalidEdge;
      if (i && edge == equation.edges[i - 1])
        return DeclineReason::DuplicateEdgeInEquation;
      checked.endpoints[edge].push_back(equation.vertex);
      ++expected[{edge, equation.vertex}];
    }
  }

  std::map<std::pair<Edge, Vertex>, uint32_t> observed;
  for (const ClassifiedOccurrence &occurrence : input.occurrences) {
    if (!work.spend())
      return DeclineReason::WorkLimit;
    if (occurrence.edge <= 0)
      return DeclineReason::InvalidEdge;
    if (occurrence.kind == OccurrenceKind::Residual)
      return DeclineReason::ResidualOccurrence;
    ++observed[{occurrence.edge, occurrence.vertex}];
  }
  if (observed != expected)
    return DeclineReason::OccurrenceMismatch;

  for (auto &[edge, endpoints] : checked.endpoints) {
    (void)edge;
    if (!work.spend())
      return DeclineReason::WorkLimit;
    std::sort(endpoints.begin(), endpoints.end());
    if (endpoints.size() != 2 || endpoints[0] == endpoints[1])
      return DeclineReason::EdgeNotInExactlyTwoVertices;
  }

  // Connectivity is checked independently of the later spanning-tree choice.
  std::map<Vertex, std::vector<Vertex>> adjacent;
  for (Vertex vertex : vertices)
    adjacent[vertex];
  for (const auto &[edge, endpoints] : checked.endpoints) {
    (void)edge;
    adjacent[endpoints[0]].push_back(endpoints[1]);
    adjacent[endpoints[1]].push_back(endpoints[0]);
  }
  std::set<Vertex> reached;
  std::queue<Vertex> pending;
  pending.push(*vertices.begin());
  reached.insert(*vertices.begin());
  while (!pending.empty()) {
    if (!work.spend())
      return DeclineReason::WorkLimit;
    const Vertex vertex = pending.front();
    pending.pop();
    for (Vertex next : adjacent[vertex])
      if (reached.insert(next).second)
        pending.push(next);
  }
  if (reached.size() != vertices.size())
    return DeclineReason::Disconnected;
  if (!odd_charge)
    return DeclineReason::EvenCharge;
  return DeclineReason::None;
}

bool lookup(const std::vector<bool> &assignment, Edge edge, bool &value) {
  if (edge <= 0 || static_cast<size_t>(edge) > assignment.size())
    return false;
  value = assignment[static_cast<size_t>(edge) - 1];
  return true;
}

} // namespace

Result plan_one(const Input &input, Limits limits) {
  Result result;
  if (!limits.additions) {
    result.decline = DeclineReason::NoAdditionAllowance;
    return result;
  }
  Work work(limits.work, limits.interrupted, limits.interrupt_state);
  CheckedInput checked;
  result.decline = check_input(input, checked, work);
  if (result.decline != DeclineReason::None) {
    if (result.decline == DeclineReason::WorkLimit && work.interrupted())
      result.decline = DeclineReason::Interrupted;
    result.work = work.used();
    return result;
  }

  std::vector<Vertex> vertices;
  vertices.reserve(checked.equations.size());
  for (const VertexEquation &equation : checked.equations)
    vertices.push_back(equation.vertex);
  std::map<Vertex, size_t> index;
  for (size_t i = 0; i < vertices.size(); ++i)
    index[vertices[i]] = i;

  // Kruskal with ascending edge IDs fixes both the spanning forest and the
  // first candidate chord independently of input order.
  using TreeArc = std::pair<Vertex, Edge>;
  std::map<Vertex, std::vector<TreeArc>> tree;
  DisjointSet sets(vertices.size());
  Edge chord = 0;
  Endpoints chord_endpoints;
  for (const auto &[edge, endpoints] : checked.endpoints) {
    if (!work.spend()) {
      result.decline = work.interrupted() ? DeclineReason::Interrupted
                                          : DeclineReason::WorkLimit;
      result.work = work.used();
      return result;
    }
    if (checked.unavailable.count(edge))
      continue;
    if (sets.join(index[endpoints[0]], index[endpoints[1]])) {
      tree[endpoints[0]].push_back({endpoints[1], edge});
      tree[endpoints[1]].push_back({endpoints[0], edge});
    } else if (!chord) {
      chord = edge;
      chord_endpoints = endpoints;
    }
  }
  if (!chord) {
    result.decline = DeclineReason::Acyclic;
    result.work = work.used();
    return result;
  }

  for (auto &[vertex, arcs] : tree) {
    (void)vertex;
    std::sort(arcs.begin(), arcs.end(), [](const TreeArc &a, const TreeArc &b) {
      if (a.second != b.second)
        return a.second < b.second;
      return a.first < b.first;
    });
  }
  std::map<Vertex, std::pair<Vertex, Edge>> parent;
  std::queue<Vertex> pending;
  const Vertex start = chord_endpoints[0], goal = chord_endpoints[1];
  parent[start] = {0, 0};
  pending.push(start);
  while (!pending.empty() && !parent.count(goal)) {
    if (!work.spend()) {
      result.decline = work.interrupted() ? DeclineReason::Interrupted
                                          : DeclineReason::WorkLimit;
      result.work = work.used();
      return result;
    }
    const Vertex vertex = pending.front();
    pending.pop();
    for (const TreeArc &arc : tree[vertex])
      if (!parent.count(arc.first)) {
        parent[arc.first] = {vertex, arc.second};
        pending.push(arc.first);
      }
  }
  if (!parent.count(goal)) {
    // The chord endpoints were connected when Kruskal rejected the edge.
    result.decline = DeclineReason::InvalidCandidate;
    result.work = work.used();
    return result;
  }

  std::vector<Edge> reverse_path;
  for (Vertex at = goal; at != start; at = parent[at].first)
    reverse_path.push_back(parent[at].second);
  std::reverse(reverse_path.begin(), reverse_path.end());
  result.candidate.pivot = chord;
  result.candidate.satisfying_value = false;
  result.candidate.cycle_edges.push_back(chord);
  result.candidate.cycle_edges.insert(result.candidate.cycle_edges.end(),
                                      reverse_path.begin(), reverse_path.end());
  if (result.candidate.cycle_edges.size() > limits.witness_length) {
    result.candidate = {};
    result.decline = DeclineReason::WitnessLengthLimit;
    result.work = work.used();
    return result;
  }
  result.candidate.substitution.push_back(
      {chord, ImageKind::Constant, false});
  for (Edge edge : reverse_path)
    result.candidate.substitution.push_back(
        {edge, ImageKind::NegatedSelf, false});
  result.requires_live_revalidation = true;
  result.work = work.used();
  return result;
}

bool validate_candidate_bounded(const Input &input, const Candidate &candidate,
                                Work &work) {
  CheckedInput checked;
  if (check_input(input, checked, work) != DeclineReason::None ||
      candidate.pivot <= 0 ||
      candidate.satisfying_value || candidate.cycle_edges.size() < 2 ||
      candidate.cycle_edges.front() != candidate.pivot ||
      candidate.substitution.size() != candidate.cycle_edges.size())
    return false;
  std::set<Edge> cycle(candidate.cycle_edges.begin(),
                       candidate.cycle_edges.end());
  if (!work.spend(candidate.cycle_edges.size())) return false;
  if (cycle.size() != candidate.cycle_edges.size())
    return false;
  for (Edge edge : cycle)
    if (checked.unavailable.count(edge))
      return false;
  for (size_t i = 0; i < candidate.substitution.size(); ++i) {
    if (!work.spend()) return false;
    const SubstitutionImage &image = candidate.substitution[i];
    if (image.source != candidate.cycle_edges[i])
      return false;
    if (!i) {
      if (image.kind != ImageKind::Constant || image.constant)
        return false;
    } else if (image.kind != ImageKind::NegatedSelf || image.constant) {
      return false;
    }
  }
  std::map<Edge, uint32_t> degree;
  for (const VertexEquation &equation : input.equations) {
    if (!work.spend()) return false;
    uint32_t touched = 0;
    for (Edge edge : equation.edges) {
      if (!work.spend()) return false;
      if (cycle.count(edge))
        ++touched;
      ++degree[edge];
    }
    if (touched != 0 && touched != 2)
      return false;
  }
  for (Edge edge : cycle)
    if (!work.spend() || degree[edge] != 2)
      return false;
  return true;
}

bool validate_candidate(const Input &input, const Candidate &candidate) {
  Work unlimited(std::numeric_limits<uint64_t>::max());
  return validate_candidate_bounded(input, candidate, unlimited);
}

bool validate_candidate(const Input &input, const Candidate &candidate,
                        uint64_t maximum_work, uint64_t &used,
                        bool &exhausted) {
  bool was_interrupted = false;
  return validate_candidate(input, candidate, maximum_work, nullptr, nullptr,
                            used, exhausted, was_interrupted);
}

bool validate_candidate(const Input &input, const Candidate &candidate,
                        uint64_t maximum_work,
                        bool (*interrupted)(void *), void *interrupt_state,
                        uint64_t &used, bool &exhausted,
                        bool &was_interrupted) {
  Work work(maximum_work, interrupted, interrupt_state);
  const bool valid = validate_candidate_bounded(input, candidate, work);
  used = work.used();
  exhausted = work.exhausted();
  was_interrupted = work.interrupted();
  return valid;
}

bool preserves_relations_on_falsifying_assignment(
    const Input &input, const Candidate &candidate,
    const std::vector<bool> &assignment) {
  if (!validate_candidate(input, candidate))
    return false;
  bool pivot_value = false;
  if (!lookup(assignment, candidate.pivot, pivot_value) || !pivot_value)
    return false;
  std::map<Edge, ImageKind> images;
  for (const SubstitutionImage &image : candidate.substitution)
    images[image.source] = image.kind;
  for (const VertexEquation &equation : input.equations) {
    bool before = false, after = false;
    for (Edge edge : equation.edges) {
      bool value = false;
      if (!lookup(assignment, edge, value))
        return false;
      before ^= value;
      const auto found = images.find(edge);
      if (found == images.end())
        after ^= value;
      else if (found->second == ImageKind::NegatedSelf)
        after ^= !value;
      // Constant image is false and contributes nothing.
    }
    if ((before == equation.charge) != (after == equation.charge))
      return false;
  }
  return true;
}

const char *decline_reason_name(DeclineReason reason) {
  switch (reason) {
  case DeclineReason::None: return "none";
  case DeclineReason::NoAdditionAllowance: return "no-addition-allowance";
  case DeclineReason::WorkLimit: return "work-limit";
  case DeclineReason::Interrupted: return "interrupted";
  case DeclineReason::IncompleteOccurrenceClassification:
    return "incomplete-occurrence-classification";
  case DeclineReason::EmptySystem: return "empty-system";
  case DeclineReason::InvalidVertex: return "invalid-vertex";
  case DeclineReason::DuplicateVertex: return "duplicate-vertex";
  case DeclineReason::InvalidEdge: return "invalid-edge";
  case DeclineReason::DuplicateEdgeInEquation:
    return "duplicate-edge-in-equation";
  case DeclineReason::ResidualOccurrence: return "residual-occurrence";
  case DeclineReason::OccurrenceMismatch: return "occurrence-mismatch";
  case DeclineReason::EdgeNotInExactlyTwoVertices:
    return "edge-not-in-exactly-two-vertices";
  case DeclineReason::Disconnected: return "disconnected";
  case DeclineReason::EvenCharge: return "even-charge";
  case DeclineReason::Acyclic: return "acyclic";
  case DeclineReason::WitnessLengthLimit: return "witness-length-limit";
  case DeclineReason::InvalidCandidate: return "invalid-candidate";
  }
  return "unknown";
}

} // namespace tactix::tseitin_cycle
