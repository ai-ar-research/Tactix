#pragma once

#include <cstdint>
#include <vector>

namespace tactix::z3_amo_semantic {

struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;
};

struct Limits {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t maximum_work = 0;
  uint32_t maximum_leaves = 0;
  uint32_t maximum_gates = 0;
};

using Charge = bool (*)(void *context, uint64_t amount);

enum class Status : uint8_t {
  Recognized,
  NotRecognized,
  LimitExceeded,
  Interrupted,
};

struct OrGate {
  int32_t output = 0;
  std::vector<int32_t> inputs;
  uint64_t backward_premise = 0;
  // In input order: each premise proves input -> output.
  std::vector<uint64_t> forward_premises;
};

struct Exclusion {
  int32_t first = 0;
  int32_t second = 0;
  uint64_t premise = 0;
};

struct PairWitness {
  uint32_t first = 0;
  uint32_t second = 0;
  // This guarded exclusion is over ancestors whose OR definitions contain
  // the two leaves.  The complete gate image supplies its resolution path.
  uint64_t exclusion_premise = 0;
  int32_t first_atom = 0;
  int32_t second_atom = 0;
};

// Exact image of Z3's AtMost(..., 1) lowering after card2bv(totalizer),
// bit-blast, and tseitin-cnf.  `root` is asserted true.  OR gates compress
// disjoint leaf groups; guarded exclusions cover every leaf pair exactly once.
struct Image {
  int32_t root = 0;
  uint64_t root_premise = 0;
  std::vector<int32_t> leaves;
  std::vector<OrGate> gates;
  std::vector<Exclusion> exclusions;
  std::vector<PairWitness> pairs;
  std::vector<Premise> classified_premises;
  uint64_t inspected_work = 0;
};

// `source` must be one closed component: the positive root unit, complete OR
// definitions, and all root-guarded exclusions.  Every source clause must be
// consumed exactly once.  Recognition is numbering-independent and changes
// `out` only on success.
Status recognize(const std::vector<Premise> &source, const Limits &limits,
                 Charge charge, void *charge_context, Image &out);

} // namespace tactix::z3_amo_semantic
