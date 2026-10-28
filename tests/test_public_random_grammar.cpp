#undef NDEBUG
#include "../tools/fuzz/public_random_grammar.hpp"

#include <array>
#include <cassert>
#include <string_view>

using tactix_public_random::Phase;
using tactix_public_random::Terminal;

int main() {
  using tactix_public_random::allows;
  using tactix_public_random::classify;
  using tactix_public_random::root_work;
  using tactix_public_random::successor;

  constexpr std::array<std::string_view, 46> public_actions{{
      "BCE", "BVE", "Backbone", "CCE", "ConflictAnalysis", "Congruence",
      "Decide", "Factor", "HyperBinaryRes", "HyperTernaryRes", "Instantiate",
      "LocalSearchGuidance", "Lucky", "Probe", "Propagate", "Reduce",
      "Reorder", "Rephase", "Restart", "Strengthen", "Substitute",
      "SubstitutionRedundancy", "SubsumeForward", "Sweep", "SwitchMode",
      "TransitiveReduce", "Vivify", "Walk", "Condition", "ParityConsequence",
      "AffineConsequence", "ExactCoverExtension", "IndependentSetCover",
      "SemanticVariableAdvice", "Gf3AffineConsequence", "DirectCspSupport",
      "DirectPhpRefutation", "Gf3GaussianConsequence",
      "TseitinCycleBreaking", "GraphIntervalEquivalence",
      "AffineMatrixInitialize", "NaryCspEquality", "Gf3GeneralRowConsequence",
      "NaryCspEqualityAmo", "NaryCspEqualityTransitivity",
      "CardinalitySearch",
  }};
  for (const auto action : public_actions)
    assert(classify(action) != Terminal::Unknown);

  assert(classify("BVE") == Terminal::Simplification);
  assert(classify("SubstitutionRedundancy") ==
         Terminal::StructuralReasoning);
  assert(classify("Walk") == Terminal::LocalSearch);
  assert(classify("Reduce") == Terminal::SearchMaintenance);
  assert(classify("not-a-public-action") == Terminal::Unknown);

  assert(root_work(Terminal::Simplification));
  assert(root_work(Terminal::StructuralReasoning));
  assert(root_work(Terminal::LocalSearch));
  assert(!root_work(Terminal::SearchMaintenance));

  assert(allows(Phase::RootWork, Terminal::Simplification));
  assert(allows(Phase::RootWork, Terminal::Decide));
  assert(allows(Phase::RootWork, Terminal::Propagate));
  assert(!allows(Phase::RootWork, Terminal::Restart));
  assert(!allows(Phase::RootWork, Terminal::SearchMaintenance));

  assert(allows(Phase::Search, Terminal::ConflictAnalysis));
  assert(allows(Phase::Search, Terminal::Propagate));
  assert(allows(Phase::Search, Terminal::Decide));
  assert(allows(Phase::Search, Terminal::SearchMaintenance));
  assert(allows(Phase::Search, Terminal::Restart));
  assert(!allows(Phase::Search, Terminal::Simplification));
  assert(!allows(Phase::Search, Terminal::StructuralReasoning));
  assert(!allows(Phase::Search, Terminal::LocalSearch));

  assert(successor(Phase::RootWork, Terminal::Decide) == Phase::Search);
  assert(successor(Phase::Search, Terminal::Restart, 3) == Phase::Search);
  assert(successor(Phase::Search, Terminal::Restart, 0) == Phase::RootWork);
}
