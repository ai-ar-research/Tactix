// Optional capture allocation stays inside a noexcept exception island. A
// failed partial import leaves older immutable captures valid; its environment
// stops without changing the live formula or publishing a partial proof.
#include "rules/sweep/sweep.hpp"
#include <new>

namespace tactix {
const SubStep *SweepProofSteps::retain(SubStep &&step) {
  if (by_id.size() <= step.id) by_id.resize(size_t(step.id) + 1, nullptr);
  auto &saved = by_id[step.id];
  if (!saved || saved->lits != step.lits || saved->chain != step.chain) {
    steps.push_back(std::move(step));
    saved = &steps.back();
  }
  return saved;
}

bool sweep_share_steps(SubDerivation &d, std::shared_ptr<SweepProofSteps> &program,
                       SweepDirProof &out) noexcept {
  try {
    if (!program) program = std::make_shared<SweepProofSteps>();
    auto selected = std::make_shared<std::vector<const SubStep *>>();
    selected->reserve(d.steps.size());
    // Core traversal is complete. Replay uses only each lemma's resolution
    // chain.
    for (auto &step : d.steps) {
      std::vector<uint32_t>().swap(step.antecedents);
      selected->push_back(program->retain(std::move(step)));
    }
    out.program = program;
    out.shared_step_count = selected->size();
    out.program_steps = std::move(selected);
    return true;
  } catch (const std::bad_alloc &) {
    out.program.reset();
    out.program_steps.reset();
    out.shared_step_count = 0;
    return false;
  }
}
} // namespace tactix
