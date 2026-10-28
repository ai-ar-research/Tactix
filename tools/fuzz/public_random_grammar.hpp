#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// The public-random trace language is deliberately defined independently of
// tactix_rules_applicable().  Applicability guards a selected terminal; it
// does not decide which sequences the policy is capable of generating.  Keep
// this classification exhaustive with respect to the public action registry.
namespace tactix_public_random {

enum class Phase : uint8_t { RootWork, Search };

enum class Terminal : uint8_t {
  Simplification,
  StructuralReasoning,
  LocalSearch,
  Propagate,
  ConflictAnalysis,
  Decide,
  SearchMaintenance,
  Restart,
  Unknown,
};

inline constexpr std::array<std::string_view, 15> SIMPLIFICATION_ACTIONS{{
    "BCE", "BVE", "Backbone", "CCE", "Congruence", "HyperBinaryRes",
    "HyperTernaryRes", "Instantiate", "Probe", "Strengthen", "Substitute",
    "SubsumeForward", "Sweep", "TransitiveReduce", "Vivify",
}};

inline constexpr std::array<std::string_view, 20>
    STRUCTURAL_REASONING_ACTIONS{{
        "Factor", "SubstitutionRedundancy", "Condition", "ParityConsequence",
        "AffineConsequence", "ExactCoverExtension", "IndependentSetCover",
        "SemanticVariableAdvice", "Gf3AffineConsequence", "DirectCspSupport",
        "DirectPhpRefutation", "Gf3GaussianConsequence",
        "TseitinCycleBreaking", "GraphIntervalEquivalence",
        "AffineMatrixInitialize", "NaryCspEquality", "Gf3GeneralRowConsequence",
        "NaryCspEqualityAmo", "NaryCspEqualityTransitivity",
        "CardinalitySearch",
    }};

inline constexpr std::array<std::string_view, 3> LOCAL_SEARCH_ACTIONS{{
    "LocalSearchGuidance", "Lucky", "Walk",
}};

inline constexpr std::array<std::string_view, 4> SEARCH_MAINTENANCE_ACTIONS{{
    "Reduce", "Reorder", "Rephase", "SwitchMode",
}};

template <std::size_t N>
constexpr bool member(std::string_view name,
                      const std::array<std::string_view, N> &actions) {
  for (const auto action : actions)
    if (name == action) return true;
  return false;
}

constexpr Terminal classify(std::string_view name) {
  if (member(name, SIMPLIFICATION_ACTIONS))
    return Terminal::Simplification;
  if (member(name, STRUCTURAL_REASONING_ACTIONS))
    return Terminal::StructuralReasoning;
  if (member(name, LOCAL_SEARCH_ACTIONS)) return Terminal::LocalSearch;
  if (member(name, SEARCH_MAINTENANCE_ACTIONS))
    return Terminal::SearchMaintenance;
  if (name == "Propagate") return Terminal::Propagate;
  if (name == "ConflictAnalysis") return Terminal::ConflictAnalysis;
  if (name == "Decide") return Terminal::Decide;
  if (name == "Restart") return Terminal::Restart;
  return Terminal::Unknown;
}

constexpr bool root_work(Terminal terminal) {
  return terminal == Terminal::Simplification ||
         terminal == Terminal::StructuralReasoning ||
         terminal == Terminal::LocalSearch;
}

// Administrative propagation is permitted in either phase.  Decide starts a
// search epoch from RootWork and continues one while in Search.  Restart is a
// Search production: keep-level zero returns to RootWork, while a positive
// keep-level remains inside Search.
constexpr bool allows(Phase phase, Terminal terminal) {
  if (terminal == Terminal::Propagate) return true;
  if (phase == Phase::RootWork)
    return root_work(terminal) || terminal == Terminal::Decide;
  return terminal == Terminal::ConflictAnalysis ||
         terminal == Terminal::Decide ||
         terminal == Terminal::SearchMaintenance ||
         terminal == Terminal::Restart;
}

constexpr Phase successor(Phase phase, Terminal terminal,
                          uint64_t restart_keep_level = 0) {
  if (phase == Phase::RootWork && terminal == Terminal::Decide)
    return Phase::Search;
  if (phase == Phase::Search && terminal == Terminal::Restart &&
      restart_keep_level == 0)
    return Phase::RootWork;
  return phase;
}

constexpr std::string_view name(Phase phase) {
  return phase == Phase::RootWork ? "root-work" : "search";
}

constexpr std::string_view name(Terminal terminal) {
  switch (terminal) {
  case Terminal::Simplification: return "simplification";
  case Terminal::StructuralReasoning: return "structural-reasoning";
  case Terminal::LocalSearch: return "local-search";
  case Terminal::Propagate: return "propagate";
  case Terminal::ConflictAnalysis: return "conflict-analysis";
  case Terminal::Decide: return "decide";
  case Terminal::SearchMaintenance: return "search-maintenance";
  case Terminal::Restart: return "restart";
  case Terminal::Unknown: return "unknown";
  }
  return "unknown";
}

} // namespace tactix_public_random
