#undef NDEBUG
#include "strategy/inprocess_program.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace tactix::strategy;

int main() {
  std::vector<InprocessStep> program = reference_inprocess_program();
  std::string error;
  assert(program.size() == 18);
  assert(validate_inprocess_program(program, &error) && error.empty());
  assert(program[1].rule == "Substitute" &&
         program[7].rule == "Substitute" &&
         program[1].id != program[7].id);
  assert(program[1].followup == Followup::AfterCongruence);
  assert(program[7].followup == Followup::AfterSweepOrFinal);

  program[1].id = program[0].id;
  assert(!validate_inprocess_program(program, &error));
  assert(error == "StepId is duplicated");
  program = reference_inprocess_program();
  program[0].id = 0;
  assert(!validate_inprocess_program(program, &error));
  program = reference_inprocess_program();
  program[0].rule.clear();
  assert(!validate_inprocess_program(program, &error));
  program = reference_inprocess_program();
  program[0].phase = static_cast<InprocessPhase>(99);
  assert(!validate_inprocess_program(program, &error));
  program = reference_inprocess_program();
  program[0].preprocessing = false;
  program[0].followup = Followup::AfterCongruence;
  assert(!validate_inprocess_program(program, &error));

  // Reorder, omission, repetition and per-occurrence activation are all valid
  // syntax. Registry/type validation happens transactionally at attachment.
  program = {
      {90, "BVE", InprocessPhase::Eliminate, true},
      {91, "Sweep", InprocessPhase::Probe, false},
      {92, "BVE", InprocessPhase::Probe, true},
  };
  BveParameters posed;
  posed.growth_bound = 7;
  program[0].parameters = posed;
  assert(validate_inprocess_program(program, &error));
  program[1].parameters = posed;
  assert(!validate_inprocess_program(program, &error));
  assert(error == "BVE parameters are attached to a different rule");
  program[1].parameters = std::monostate{};
  std::get<BveParameters>(program[0].parameters).xor_arity_limit =
      tactix::BVE_XOR_ARITY_CAPACITY + 1;
  assert(!validate_inprocess_program(program, &error));

  program = reference_inprocess_program();
  program[12].parameters = SubsumeForwardParameters{};
  program[13].parameters = StrengthenParameters{};
  assert(validate_inprocess_program(program, &error));
  program[0].parameters = StrengthenParameters{};
  assert(!validate_inprocess_program(program, &error));
  assert(error == "C2 parameters are attached to a different rule");

  program = reference_inprocess_program();
  program[0].candidate_order = static_cast<CandidateOrder>(99);
  assert(!validate_inprocess_program(program, &error));
  assert(error == "candidate order is invalid");
}
