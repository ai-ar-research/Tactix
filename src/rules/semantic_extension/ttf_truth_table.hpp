#pragma once

#include <cstdint>
#include <vector>

namespace tactix::ttf_truth_table {

struct Premise {
  uint64_t occurrence = 0;
  std::vector<int32_t> literals;
  uint64_t native_proof_id = 0;
};

struct Limits {
  uint64_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint64_t maximum_work = 0;
  uint64_t maximum_plan_literals = 0;
  uint32_t maximum_stars = 0;
  uint32_t maximum_domains = 0;
  uint32_t maximum_domain_leaves = 0;
  uint32_t maximum_plan_additions = 0;
};

using Charge = bool (*)(void *context, uint64_t amount);

enum class Status : uint8_t {
  Recognized,
  NotRecognized,
  LimitExceeded,
  Interrupted,
};

enum class ZeroEncoding : uint8_t { GuardedByParticipation, DirectStar };

struct AtomClass {
  int32_t atom = 0;
  std::vector<int32_t> leaves;
  bool nonzero = false;
  uint64_t nonzero_premise = 0;
  ZeroEncoding zero_encoding = ZeroEncoding::DirectStar;
  std::vector<uint64_t> zero_premises;
};

struct Star {
  std::vector<int32_t> variables;
  int32_t fixed_false = 0;
  uint64_t fixed_false_premise = 0;
  int32_t participation_root = 0;
  std::vector<AtomClass> atoms;
};

struct Domain {
  std::vector<int32_t> leaves;
  int32_t forced_root = 0;
  uint64_t forced_unit_premise = 0;
  std::vector<uint32_t> stars;
};

struct Coordinate {
  std::vector<uint32_t> stars;
};

enum class RootUse : uint8_t {
  Definition,
  Assertion,
  NonzeroLink,
  ZeroLink,
  ParticipationGuard,
  SharedAmo,
};

struct CensusEntry {
  uint64_t occurrence = 0;
  RootUse use = RootUse::Definition;
  bool shared_with_amo = false;
};

struct RootCensus {
  int32_t root = 0;
  std::vector<CensusEntry> occurrences;
};

struct Image {
  std::vector<Star> stars;
  std::vector<Domain> domains;
  std::vector<Coordinate> coordinates;
  std::vector<RootCensus> root_census;
  std::vector<Premise> classified_premises;
  uint64_t inspected_work = 0;
};

// `source` is the complete live-clause snapshot.  Repeated positive supports
// recover the direct truth-table star grammar without variable positions.
// Every occurrence of an accepted Tseitin root must be classified here or be
// named in `amo_owned_occurrences`; the latter retains multi-owner provenance.
Status recognize(const std::vector<Premise> &source,
                 const std::vector<uint64_t> &amo_owned_occurrences,
                 const Limits &limits, Charge charge, void *charge_context,
                 Image &out);

struct PlannedAddition {
  std::vector<int32_t> clause;
  std::vector<uint64_t> rup_premises;
  uint32_t domain = 0;
  uint32_t star = 0;
};

struct ParityFrontierPlan {
  uint32_t coordinate = 0;
  std::vector<PlannedAddition> additions;
  // Each active star still needs its already-recognized AMO component.  This
  // increment deliberately stops before claiming a complete refutation.
  std::vector<std::vector<int32_t>> required_amo_leaf_sets;
  uint64_t inspected_work = 0;
};

// Chooses the first odd coordinate on which every forced TTF atom is nonzero
// and plans one reduced participation clause per domain.  Plans are pure and
// transactional; each addition is RUP from listed source premises.
Status plan_parity_frontier(const Image &image, const Limits &limits,
                            Charge charge, void *charge_context,
                            ParityFrontierPlan &out);

} // namespace tactix::ttf_truth_table
