#include "rules/semantic_extension/z3_forcing_recognition.hpp"

#include <algorithm>
#include <climits>
#include <map>
#include <set>
#include <utility>

namespace tactix::z3_forcing_semantic {
namespace {

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
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

using ClauseMap = std::map<std::vector<int32_t>, const Premise *>;

const Premise *find_clause(const ClauseMap &clauses,
                           std::vector<int32_t> literals) {
  std::sort(literals.begin(), literals.end());
  const auto found = clauses.find(literals);
  return found == clauses.end() ? nullptr : found->second;
}

} // namespace

Status recognize(const std::vector<Premise> &source, const Limits &limits,
                 Charge charge, void *charge_context, Image &out) {
  if (!limits.maximum_clauses || !limits.maximum_literals ||
      !limits.maximum_work || !limits.maximum_choices)
    return Status::LimitExceeded;
  if (source.empty()) return Status::NotRecognized;
  if (source.size() > limits.maximum_clauses)
    return Status::LimitExceeded;

  Meter meter{limits, charge, charge_context};
  ClauseMap clauses;
  std::set<uint64_t> occurrences, native_proof_ids;
  uint64_t literal_count = 0;
  for (const Premise &premise : source) {
    if (!premise.occurrence || !premise.native_proof_id ||
        !occurrences.insert(premise.occurrence).second ||
        !native_proof_ids.insert(premise.native_proof_id).second ||
        premise.literals.empty())
      return Status::NotRecognized;
    if (literal_count > limits.maximum_literals ||
        premise.literals.size() > limits.maximum_literals - literal_count)
      return Status::LimitExceeded;
    literal_count += premise.literals.size();
    if (!meter.take(uint64_t{1} + premise.literals.size())) return meter.status;

    std::vector<int32_t> normalized = premise.literals;
    for (int32_t literal : normalized)
      if (!literal || literal == INT32_MIN) return Status::NotRecognized;
    std::sort(normalized.begin(), normalized.end());
    if (std::adjacent_find(normalized.begin(), normalized.end()) !=
        normalized.end())
      return Status::NotRecognized;
    for (size_t i = 1; i < normalized.size(); ++i)
      if (variable(normalized[i - 1]) == variable(normalized[i]))
        return Status::NotRecognized;
    if (!clauses.emplace(std::move(normalized), &premise).second)
      return Status::NotRecognized;
  }

  std::vector<Choice> choices;
  std::set<uint64_t> classified;
  std::set<int32_t> roots, inputs;

  // Positive units identify asserted Tseitin OR roots.  Require exactly one
  // compatible backward clause, then require both forward implications.
  for (const auto &[normalized, premise] : clauses) {
    if (normalized.size() != 1 || normalized[0] < 0) continue;
    if (choices.size() >= limits.maximum_choices)
      return Status::LimitExceeded;
    const int32_t root = normalized[0];
    if (!roots.insert(root).second) return Status::NotRecognized;

    const Premise *backward = nullptr;
    int32_t left = 0, right = 0;
    for (const auto &[candidate, candidate_premise] : clauses) {
      if (!meter.take(1)) return meter.status;
      if (candidate.size() != 3) continue;
      if (!std::binary_search(candidate.begin(), candidate.end(), -root))
        continue;
      std::vector<int32_t> positive;
      for (int32_t literal : candidate)
        if (literal > 0) positive.push_back(literal);
      if (positive.size() != 2 || backward) return Status::NotRecognized;
      left = positive[0];
      right = positive[1];
      backward = candidate_premise;
    }
    if (!backward || left == root || right == root)
      return Status::NotRecognized;

    const Premise *left_forward = find_clause(clauses, {-left, root});
    const Premise *right_forward = find_clause(clauses, {-right, root});
    if (!left_forward || !right_forward || left_forward == right_forward)
      return Status::NotRecognized;

    Choice choice;
    choice.encoding = Encoding::AssertedOr;
    choice.left = left;
    choice.right = right;
    choice.root = root;
    choice.consequence = {left, right};
    choice.root_premise = premise->occurrence;
    choice.backward_premise = backward->occurrence;
    choice.left_forward_premise = left_forward->occurrence;
    choice.right_forward_premise = right_forward->occurrence;
    choice.rup_premises = {premise->occurrence, backward->occurrence};

    const uint64_t owned[] = {premise->occurrence, backward->occurrence,
                              left_forward->occurrence,
                              right_forward->occurrence};
    for (uint64_t occurrence : owned)
      if (!classified.insert(occurrence).second)
        return Status::NotRecognized;
    choices.push_back(std::move(choice));
  }

  // A positive binary is the simplified form of the same forcing obligation.
  for (const auto &[normalized, premise] : clauses) {
    if (classified.count(premise->occurrence)) continue;
    if (!meter.take(1)) return meter.status;
    if (normalized.size() != 2 || normalized[0] < 0 || normalized[1] < 0)
      continue;
    if (choices.size() >= limits.maximum_choices)
      return Status::LimitExceeded;
    Choice choice;
    choice.encoding = Encoding::DirectClause;
    choice.left = normalized[0];
    choice.right = normalized[1];
    choice.consequence = normalized;
    choice.direct_premise = premise->occurrence;
    classified.insert(premise->occurrence);
    choices.push_back(std::move(choice));
  }

  if (choices.empty() || classified.size() != source.size())
    return Status::NotRecognized;

  // TTF creates a fresh, disjoint pair for each forced row/ball position.
  // Enforcing that invariant prevents an arbitrary collection of Tseitin gates
  // from being mislabeled as one semantic family.
  for (const Choice &choice : choices) {
    if (!meter.take(1)) return meter.status;
    if (roots.count(choice.left) || roots.count(choice.right) ||
        !inputs.insert(choice.left).second ||
        !inputs.insert(choice.right).second)
      return Status::NotRecognized;
  }

  std::sort(choices.begin(), choices.end(), [](const Choice &a,
                                                const Choice &b) {
    if (a.left != b.left) return a.left < b.left;
    if (a.right != b.right) return a.right < b.right;
    return a.root < b.root;
  });
  Image image;
  image.choices = std::move(choices);
  image.classified_premises = source;
  std::sort(image.classified_premises.begin(), image.classified_premises.end(),
            [](const Premise &a, const Premise &b) {
              return a.occurrence < b.occurrence;
            });
  image.inspected_work = meter.work;
  out = std::move(image);
  return Status::Recognized;
}

} // namespace tactix::z3_forcing_semantic
