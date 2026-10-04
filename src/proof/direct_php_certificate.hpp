// A pure event planner for the cubic DRAT proof of the direct pigeonhole
// principle due to Grosof, Zhang, and Heule (arXiv:2207.11284).
//
// This is deliberately independent of Kernel and Proof.  A future adapter can
// replay AddRup and AddRat as witness-free native DSR/DRAT additions (the pivot
// of every AddRat event is clause.front()) and Delete as an ordinary clausal
// deletion.  Planning is atomic: exact count-only preflight, including the
// permanent auxiliary namespace, succeeds before any event vector is built.
//
// The older pigeonhole_certificate.hpp is also cubic, but uses moved occupancy
// prefixes.  This planner instead preserves the paper's compact recursive AMO
// representation and checker-oriented deletion schedule.
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tactix::direct_php_certificate {

using Literal = int32_t;
using Clause = std::vector<Literal>;
using Matrix = std::vector<std::vector<Literal>>;

enum class EventKind : uint8_t { AddRup, AddRat, Delete };

enum class EventRole : uint8_t {
  BaseDefinition,
  AmoDefinition,
  AmoDerived,
  PigeonCover,
  RetireOriginalAmo,
  RetirePriorLayer,
  Conclusion,
};

struct Event {
  EventKind kind = EventKind::AddRup;
  EventRole role = EventRole::Conclusion;
  // Number of holes in the recursively introduced layer.  The conclusion is
  // layer zero and retirement records name the layer being retired.
  uint32_t layer = 0;
  Clause clause;
};

struct Accounting {
  uint64_t additions = 0;
  uint64_t rup_additions = 0;
  uint64_t rat_additions = 0;
  uint64_t deletions = 0;
  uint64_t addition_literals = 0;
  uint64_t deletion_literals = 0;
  uint64_t fresh_variables = 0;
  uint64_t events = 0;
  uint64_t maximum_event_width = 0;
  uint64_t next_fresh = 0;

  friend bool operator==(const Accounting &, const Accounting &) = default;
};

struct Limits {
  // Literal limit covers additions and deletions together.
  uint64_t maximum_additions = std::numeric_limits<uint64_t>::max();
  uint64_t maximum_deletions = std::numeric_limits<uint64_t>::max();
  uint64_t maximum_literals = std::numeric_limits<uint64_t>::max();
  uint64_t maximum_fresh_variables = std::numeric_limits<uint64_t>::max();
  uint64_t maximum_events = std::numeric_limits<uint64_t>::max();
};

enum class Status : uint8_t {
  Ok,
  NoHoles,
  NotOverfull,
  RaggedMatrix,
  InvalidVariable,
  RepeatedVariable,
  FreshNamespaceCollision,
  ArithmeticOverflow,
  LimitExceeded,
  InternalMismatch,
};

struct Plan {
  uint32_t input_pigeons = 0;
  uint32_t holes = 0;
  uint32_t core_pigeons = 0;
  Literal first_fresh = 0;
  Accounting accounting;
  std::vector<Event> events;
};

namespace detail {

inline bool add_u64(uint64_t a, uint64_t b, uint64_t &out) {
  if (a > std::numeric_limits<uint64_t>::max() - b)
    return false;
  out = a + b;
  return true;
}

inline bool mul_u64(uint64_t a, uint64_t b, uint64_t &out) {
  if (a && b > std::numeric_limits<uint64_t>::max() / a)
    return false;
  out = a * b;
  return true;
}

inline bool accumulate(uint64_t &target, uint64_t amount) {
  return add_u64(target, amount, target);
}

inline bool count_event(Accounting &a, EventKind kind, uint64_t width) {
  if (!accumulate(a.events, 1))
    return false;
  a.maximum_event_width = std::max(a.maximum_event_width, width);
  if (kind == EventKind::Delete) {
    return accumulate(a.deletions, 1) &&
           accumulate(a.deletion_literals, width);
  }
  if (!accumulate(a.additions, 1) ||
      !accumulate(a.addition_literals, width))
    return false;
  if (kind == EventKind::AddRup)
    return accumulate(a.rup_additions, 1);
  return accumulate(a.rat_additions, 1);
}

inline bool stage_counts(uint64_t k, Accounting &stage) {
  // k+1 pigeons and k holes are introduced at this layer.
  uint64_t x_fresh = 0;
  if (!mul_u64(k, k + 1, x_fresh))
    return false;
  const uint64_t groups = k >= 2 ? (k - 2) / 2 : 0;
  uint64_t y_fresh = 0;
  if (!mul_u64(k, groups, y_fresh) ||
      !add_u64(x_fresh, y_fresh, stage.fresh_variables))
    return false;

  // Base definitions: k holes, k four-clause variables plus one optimized
  // two-clause final variable.  Their literal count is k*(11k+5).
  uint64_t base_additions = 0, base_literals = 0, inner = 0;
  if (!mul_u64(k, 4 * k + 2, base_additions) ||
      !add_u64(11 * k, 5, inner) || !mul_u64(k, inner, base_literals))
    return false;
  if (!accumulate(stage.events, base_additions) ||
      !accumulate(stage.additions, base_additions) ||
      !accumulate(stage.rat_additions, base_additions) ||
      !accumulate(stage.addition_literals, base_literals))
    return false;

  // Every non-final AMO group contributes six binary clauses and one
  // four-literal definition.  The final group has two, three, or four items.
  const uint64_t final_size = k + 1 - 2 * groups;
  uint64_t group_additions_per_hole = 0, group_literals_per_hole = 0;
  if (!mul_u64(groups, 7, group_additions_per_hole) ||
      !accumulate(group_additions_per_hole,
                  final_size * (final_size - 1) / 2) ||
      !mul_u64(groups, 16, group_literals_per_hole) ||
      !accumulate(group_literals_per_hole, final_size * (final_size - 1)))
    return false;
  uint64_t group_additions = 0, group_literals = 0;
  if (!mul_u64(k, group_additions_per_hole, group_additions) ||
      !mul_u64(k, group_literals_per_hole, group_literals))
    return false;
  if (!accumulate(stage.events, group_additions) ||
      !accumulate(stage.additions, group_additions) ||
      !accumulate(stage.rat_additions, group_additions) ||
      !accumulate(stage.addition_literals, group_literals))
    return false;

  // One k-literal RUP cover for each of k+1 pigeons.
  if (!accumulate(stage.events, k + 1) ||
      !accumulate(stage.additions, k + 1) ||
      !accumulate(stage.rup_additions, k + 1) ||
      !mul_u64(k, k + 1, inner) ||
      !accumulate(stage.addition_literals, inner))
    return false;
  stage.maximum_event_width =
      k <= 3 ? 3 : std::max<uint64_t>(4, k);
  return true;
}

inline bool add_accounting(Accounting &to, const Accounting &from) {
  return accumulate(to.additions, from.additions) &&
         accumulate(to.rup_additions, from.rup_additions) &&
         accumulate(to.rat_additions, from.rat_additions) &&
         accumulate(to.deletions, from.deletions) &&
         accumulate(to.addition_literals, from.addition_literals) &&
         accumulate(to.deletion_literals, from.deletion_literals) &&
         accumulate(to.fresh_variables, from.fresh_variables) &&
         accumulate(to.events, from.events) &&
         (to.maximum_event_width =
              std::max(to.maximum_event_width, from.maximum_event_width),
          true);
}

inline Status validate(const Matrix &variables, Literal first_fresh,
                       uint32_t &pigeons, uint32_t &holes,
                       Literal &maximum_input) {
  if (variables.size() > std::numeric_limits<uint32_t>::max())
    return Status::ArithmeticOverflow;
  pigeons = static_cast<uint32_t>(variables.size());
  if (!pigeons)
    return Status::NotOverfull;
  if (variables[0].size() > std::numeric_limits<uint32_t>::max())
    return Status::ArithmeticOverflow;
  holes = static_cast<uint32_t>(variables[0].size());
  if (!holes)
    return Status::NoHoles;
  if (pigeons <= holes)
    return Status::NotOverfull;

  std::unordered_set<Literal> seen;
  maximum_input = 0;
  for (const auto &row : variables) {
    if (row.size() != holes)
      return Status::RaggedMatrix;
    for (const Literal variable : row) {
      if (variable <= 0)
        return Status::InvalidVariable;
      if (!seen.insert(variable).second)
        return Status::RepeatedVariable;
      maximum_input = std::max(maximum_input, variable);
    }
  }
  if (first_fresh <= maximum_input)
    return Status::FreshNamespaceCollision;
  return Status::Ok;
}

inline Status preflight_impl(const Matrix &variables, Literal first_fresh,
                             const Limits &limits, Accounting &result,
                             uint32_t &pigeons, uint32_t &holes) {
  Literal maximum_input = 0;
  const Status normalized =
      validate(variables, first_fresh, pigeons, holes, maximum_input);
  if (normalized != Status::Ok)
    return normalized;

  Accounting total;
  Accounting previous_stage;
  for (uint64_t k = holes - 1; k >= 1; --k) {
    Accounting stage;
    if (!stage_counts(k, stage) || !add_accounting(total, stage))
      return Status::ArithmeticOverflow;

    if (k == holes - 1) {
      // The paper retires only the pairwise AMOs of the original direct PHP.
      uint64_t pairs = 0, deleted = 0;
      if (!mul_u64(uint64_t(holes) + 1, holes, pairs))
        return Status::ArithmeticOverflow;
      pairs /= 2;
      if (!mul_u64(holes, pairs, deleted) ||
          !accumulate(total.deletions, deleted) ||
          !accumulate(total.events, deleted) ||
          !mul_u64(deleted, 2, deleted) ||
          !accumulate(total.deletion_literals, deleted))
        return Status::ArithmeticOverflow;
      total.maximum_event_width = std::max<uint64_t>(2, total.maximum_event_width);
    } else {
      if (!accumulate(total.deletions, previous_stage.additions) ||
          !accumulate(total.events, previous_stage.additions) ||
          !accumulate(total.deletion_literals,
                      previous_stage.addition_literals))
        return Status::ArithmeticOverflow;
    }
    previous_stage = stage;
    if (k == 1)
      break;
  }
  if (!count_event(total, EventKind::AddRup, 0))
    return Status::ArithmeticOverflow;

  if (total.fresh_variables &&
      uint64_t(first_fresh) + total.fresh_variables - 1 >
          uint64_t(std::numeric_limits<Literal>::max()))
    return Status::ArithmeticOverflow;
  total.next_fresh = uint64_t(first_fresh) + total.fresh_variables;

  uint64_t literals = 0;
  if (!add_u64(total.addition_literals, total.deletion_literals, literals))
    return Status::ArithmeticOverflow;
  if (total.additions > limits.maximum_additions ||
      total.deletions > limits.maximum_deletions ||
      literals > limits.maximum_literals ||
      total.fresh_variables > limits.maximum_fresh_variables ||
      total.events > limits.maximum_events)
    return Status::LimitExceeded;
  result = total;
  return Status::Ok;
}

struct Builder {
  Plan &plan;
  uint64_t next_fresh;
  Accounting accounting;

  Literal fresh() {
    const Literal result = static_cast<Literal>(next_fresh++);
    ++accounting.fresh_variables;
    return result;
  }

  void emit(EventKind kind, EventRole role, uint32_t layer, Clause clause) {
    count_event(accounting, kind, clause.size());
    plan.events.push_back(Event{kind, role, layer, std::move(clause)});
  }
};

struct LayerResult {
  Matrix variables;
  std::vector<Clause> additions;
};

inline LayerResult build_layer(Builder &out, const Matrix &prior, uint32_t k) {
  LayerResult result;
  result.variables.assign(k + 1, std::vector<Literal>(k));
  const size_t addition_begin = out.plan.events.size();
  const uint32_t old_last = static_cast<uint32_t>(prior.size() - 1);

  for (uint32_t hole = 0; hole < k; ++hole) {
    std::vector<Literal> amo;
    amo.reserve(k + 1);
    for (uint32_t pigeon = 0; pigeon <= k; ++pigeon) {
      const Literal x = out.fresh();
      result.variables[pigeon][hole] = x;
      const Literal old_same = prior[pigeon][hole];
      const Literal old_last_hole = prior[pigeon][k];
      const Literal displaced = prior[old_last][hole];
      if (pigeon < k) {
        out.emit(EventKind::AddRat, EventRole::BaseDefinition, k,
                 {-x, old_same, old_last_hole});
        out.emit(EventKind::AddRat, EventRole::BaseDefinition, k,
                 {-x, old_same, displaced});
      }
      out.emit(EventKind::AddRat, EventRole::BaseDefinition, k,
               {x, -old_same});
      out.emit(EventKind::AddRat, EventRole::BaseDefinition, k,
               {x, -old_last_hole, -displaced});
      amo.push_back(x);
    }

    while (amo.size() > 1) {
      if (amo.size() > 4) {
        const Literal y = out.fresh();
        std::vector<Literal> group{amo[0], amo[1], amo[2], y};
        for (size_t first = 0; first < group.size(); ++first)
          for (size_t second = first + 1; second < group.size(); ++second)
            out.emit(EventKind::AddRat,
                     second == 3 ? EventRole::AmoDefinition
                                 : EventRole::AmoDerived,
                     k, {-group[second], -group[first]});
        out.emit(EventKind::AddRat, EventRole::AmoDefinition, k,
                 {y, group[0], group[1], group[2]});
        amo.erase(amo.begin(), amo.begin() + 3);
        amo.insert(amo.begin(), -y);
      } else {
        for (size_t first = 0; first < amo.size(); ++first)
          for (size_t second = first + 1; second < amo.size(); ++second)
            out.emit(EventKind::AddRat, EventRole::AmoDerived, k,
                     {-amo[second], -amo[first]});
        amo.clear();
      }
    }
  }

  for (uint32_t pigeon = 0; pigeon <= k; ++pigeon)
    out.emit(EventKind::AddRup, EventRole::PigeonCover, k,
             result.variables[pigeon]);

  const size_t addition_end = out.plan.events.size();
  result.additions.reserve(addition_end - addition_begin);
  for (size_t i = addition_begin; i < addition_end; ++i)
    result.additions.push_back(out.plan.events[i].clause);
  return result;
}

} // namespace detail

inline Status preflight(const Matrix &variables, Literal first_fresh,
                        const Limits &limits, Accounting &accounting) {
  uint32_t pigeons = 0, holes = 0;
  Accounting local;
  const Status status = detail::preflight_impl(
      variables, first_fresh, limits, local, pigeons, holes);
  if (status == Status::Ok)
    accounting = local;
  return status;
}

// `variables` is a normalized P-by-H matrix of distinct positive external
// variable identities.  P must exceed H.  The first H+1 rows form the
// refutation core; additional pigeons remain in the checker formula and need
// no special treatment.  Callers still own exact source-clause recognition:
// every selected row needs its ALO and every selected column its pairwise AMO.
inline Status plan(const Matrix &variables, Literal first_fresh,
                   const Limits &limits, Plan &output) {
  Accounting expected;
  uint32_t pigeons = 0, holes = 0;
  const Status status = detail::preflight_impl(
      variables, first_fresh, limits, expected, pigeons, holes);
  if (status != Status::Ok)
    return status;

  Plan local;
  local.input_pigeons = pigeons;
  local.holes = holes;
  local.core_pigeons = holes + 1;
  local.first_fresh = first_fresh;
  detail::Builder builder{local, static_cast<uint64_t>(first_fresh), {}};

  if (expected.events > std::numeric_limits<size_t>::max())
    return Status::ArithmeticOverflow;
  local.events.reserve(static_cast<size_t>(expected.events));
  Matrix prior(variables.begin(), variables.begin() + holes + 1);
  std::vector<Clause> prior_layer_additions;
  for (uint32_t k = holes - 1; k >= 1; --k) {
    detail::LayerResult current = detail::build_layer(builder, prior, k);
    if (k == holes - 1) {
      for (uint32_t hole = 0; hole < holes; ++hole)
        for (uint32_t first = 0; first <= holes; ++first)
          for (uint32_t second = first + 1; second <= holes; ++second)
            builder.emit(EventKind::Delete, EventRole::RetireOriginalAmo,
                         holes,
                         {-variables[first][hole], -variables[second][hole]});
    } else {
      for (const Clause &clause : prior_layer_additions)
        builder.emit(EventKind::Delete, EventRole::RetirePriorLayer, k + 1,
                     clause);
    }
    prior = std::move(current.variables);
    prior_layer_additions = std::move(current.additions);
    if (k == 1)
      break;
  }
  builder.emit(EventKind::AddRup, EventRole::Conclusion, 0, {});

  builder.accounting.next_fresh = builder.next_fresh;
  local.accounting = builder.accounting;
  if (!(local.accounting == expected))
    return Status::InternalMismatch;
  output = std::move(local);
  return Status::Ok;
}

inline Status plan(const Matrix &variables, Literal first_fresh, Plan &output) {
  return plan(variables, first_fresh, Limits{}, output);
}

} // namespace tactix::direct_php_certificate
