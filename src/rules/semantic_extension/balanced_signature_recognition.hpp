#pragma once

#include <cstdint>
#include <vector>

namespace tactix::balanced_signature_semantic {

struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;
};

struct Limits {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t maximum_work = 0;
  uint64_t maximum_balance_tuples = 0;
  uint64_t maximum_alignment_trials = 0;
  uint32_t maximum_balls = 0;
  uint32_t maximum_width = 0;
};

using Charge = bool (*)(void *context, uint64_t amount);

enum class Status : uint8_t {
  Recognized,
  NotRecognized,
  LimitExceeded,
  Interrupted,
};

struct StatePair {
  int32_t negative = 0;
  int32_t positive = 0;
  uint64_t exclusion_premise = 0;
};

struct Signature {
  int32_t selector = 0;
  // Values are -1, 0, or +1 in the common coordinate frame.
  std::vector<int8_t> value;
  // Two implications per coordinate, in (negative, positive) state order.
  std::vector<uint64_t> definition_premises;
};

struct Ball {
  uint64_t cover_premise = 0;
  std::vector<StatePair> states;
  std::vector<Signature> signatures;
  // Row-major symmetric matrix.  The diagonal is zero.
  std::vector<uint64_t> exclusion_premises;
};

struct ForbiddenTuple {
  std::vector<uint32_t> signatures;
  uint64_t premise = 0;
};

// Exact image of the bounded, direct balanced-signature encoding recognized
// here: each ball chooses one of all nonzero ternary signatures, binary
// implications define two mutually-exclusive state bits per coordinate, and
// a negative selector clause excludes every and only tuple whose coordinate
// sums are nonzero.  The image is independent of variable numbering and
// retains the identity of every source clause.
struct Image {
  uint32_t balls = 0;
  uint32_t width = 0;
  std::vector<Ball> domains;
  std::vector<ForbiddenTuple> forbidden;
  std::vector<uint64_t> classified_occurrences;
  // Lossless source census, sorted by occurrence.  This retains native proof
  // identities even when occurrence numbers are only snapshot-local.
  std::vector<Premise> classified_premises;
  uint64_t inspected_work = 0;
};

// Recognition is read-only and transactional.  `out` is changed only for
// Recognized.  No assignment or solver outcome is accepted as evidence.
Status recognize(const std::vector<Premise> &source, const Limits &limits,
                 Charge charge, void *charge_context, Image &out);

} // namespace tactix::balanced_signature_semantic
