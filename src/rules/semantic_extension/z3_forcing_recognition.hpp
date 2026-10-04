#pragma once

#include <cstdint>
#include <vector>

namespace tactix::z3_forcing_semantic {

struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;
};

struct Limits {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t maximum_work = 0;
  uint32_t maximum_choices = 0;
};

using Charge = bool (*)(void *context, uint64_t amount);

enum class Status : uint8_t {
  Recognized,
  NotRecognized,
  LimitExceeded,
  Interrupted,
};

enum class Encoding : uint8_t {
  DirectClause,
  AssertedOr,
};

// A complete Boolean image of one TTF-style forcing choice.  In both forms the
// source entails `consequence` = (left or right).  DirectClause uses the
// consequence itself as a source premise.  AssertedOr retains the complete
// conservative definition root <-> (left or right), plus the asserted root.
struct Choice {
  Encoding encoding = Encoding::DirectClause;
  int32_t left = 0;
  int32_t right = 0;
  int32_t root = 0;
  std::vector<int32_t> consequence;

  uint64_t direct_premise = 0;
  uint64_t root_premise = 0;
  uint64_t backward_premise = 0;
  uint64_t left_forward_premise = 0;
  uint64_t right_forward_premise = 0;

  // Premises sufficient for a RUP check of `consequence`.  A direct source
  // clause has no derived-addition certificate.
  std::vector<uint64_t> rup_premises;
};

// Exact closed collection of disjoint two-way forcing choices.  Every source
// clause is consumed exactly once, source and native proof identities are
// unique, roots never serve as choice inputs, and choice inputs do not overlap.
struct Image {
  std::vector<Choice> choices;
  std::vector<Premise> classified_premises;
  uint64_t inspected_work = 0;
};

// Recognizes either a positive binary source clause or the complete Z3 Tseitin
// image {r}, {-left,r}, {-right,r}, {left,right,-r} for every choice.  `out` is
// changed only after the whole source has been classified successfully.
Status recognize(const std::vector<Premise> &source, const Limits &limits,
                 Charge charge, void *charge_context, Image &out);

} // namespace tactix::z3_forcing_semantic
