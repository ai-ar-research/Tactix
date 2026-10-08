#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

#include <functional>
#include <limits>
#include <utility>
#include <memory>
#include <vector>

namespace tactix {

struct SubstitutionRedundancyInstance {
  std::vector<Lit> clause;
  std::vector<Lit> witness;
  std::vector<std::pair<Lit, Lit>> substitution;
  // Internal producer provenance. Ordinary explicit applications ignore this
  // bit. The online executor consumes it only for a freshly enumerated
  // orbitopal prefix whose preceding members remain in their original order.
  bool orbitopal_prefix_certificate = false;
  // Internal producer provenance for an ordered clausal-fixing level from an
  // exact generator group. Binary members are inverse-transversal SST steps;
  // the final unit is RUP from the source clause and completed binary prefix.
  // Explicit callers cannot activate this certificate.
  bool symmetry_chain_certificate = false;
  // Internal producer provenance for a binary clause blocked on a literal
  // which occurred in exactly one source clause when the complete pass was
  // enumerated. Explicit callers cannot activate this certificate.
  bool unique_literal_amo_certificate = false;
  bool operator==(const SubstitutionRedundancyInstance &) const = default;
};

enum class SubstitutionRedundancyProducer : uint8_t {
  ExplicitOnly,
  Orbitopal,
  GeneralComplete,
  GraphSymmetry,
};

enum class OrbitopalOrientation : uint8_t {
  Rows,
  Columns,
  Both,
};

enum class OrbitopalColumnOrder : uint8_t {
  Canonical,
  CooccurrenceMaximumClique,
  CooccurrenceGreedyClique,
  CooccurrenceBoundedGreedyClique,
};

enum class OrbitopalCompletion : uint8_t {
  MatrixOnly,
  IndividualizedGraphCompletion,
};

enum class OrbitopalDiscoveryOrder : uint8_t {
  RefinementThenUniqueLiteral,
  UniqueLiteralThenRefinement,
  // Scope selection does not disable the independent AMO-completion option.
  RefinementOnly,
  UniqueLiteralOnly,
};

enum class OrbitopalRowOrbitSource : uint8_t {
  DetectedGenerators,
  RefinementCells,
};

enum class SymmetryFixingMode : uint8_t {
  StabilizerOnly,
  GeneratorsOnly,
  StabilizerThenGenerators,
};

enum class SymmetryTraversalOrder : uint8_t {
  Canonical,
  Reverse,
  Seeded,
};

enum class SymmetryClauseScope : uint8_t {
  All,
  UniqueLiteralsOnly,
};

enum class SymmetryPairOrientation : uint8_t {
  Canonical,
  Reverse,
  Seeded,
};

enum class SymmetryBinaryGraphMode : uint8_t {
  ClauseVertices,
  DirectEdges,
  Automatic,
};

enum class SymmetryClauseSelection : uint8_t {
  InputOrder,
  Shortest,
  AmoThenShortest,
};

// The legacy producer stops after the last clausal-fixing level.  FixAndCut
// follows the complete Schreier schedule: it records every base level, closes
// the chain, and emits a full orbit cut when no source clause can be fixed.
enum class SymmetryChainMode : uint8_t {
  ClausalFixing,
  FixAndCut,
  // Re-sift retained stabilizer generators at every base extension.
  FixAndCutResift,
};

// Base selection is a FixAndCut strategy setting, not part of X7. It is
// dormant for ClausalFixing. ClauseOrder preserves the
// caller's independently configured clause/literal order.  CommonClauseAmo
// prefers AMOs, then overlap with earlier base variables and smaller orbits.
// FirstAmoThenCommonClause takes the first applicable AMO in caller order;
// otherwise it retains common-clause scoring and the smaller-orbit tie break.
// FirstAmoThenOrderedRemainder uses that ranking on its first AMO-free scan,
// then follows caller order for the rest of the chain. ForwardRemainder also
// retains the first eligible clause as a forward scan frontier between levels.
enum class SymmetryBaseSelection : uint8_t {
  ClauseOrder,
  CommonClauseAmo,
  FirstAmoThenCommonClause,
  FirstAmoThenOrderedRemainder,
  FirstAmoThenForwardRemainder,
  // Prefer fewer active literals, then AMO/common-clause/smaller-orbit ties.
  ShortestClause,
  // Rank the initial level by length, then retain a forward clause frontier.
  FirstShortestThenForwardRemainder,
  // Satsuma's fix_and_cut scan exactly (symmetries.h): clauses in source
  // order from a forward frontier; the first applicable clause is taken and
  // its first literal is the base point; a later clause replaces it only if
  // it is an AMO (break), or if its first variable shares a clause with an
  // earlier base point and its orbit is smaller. Once a scan has reached the
  // last clause, AMO preference ends and every level takes its first
  // applicable clause.
  SatsumaFixAndCut,
};

// Where a FixAndCut cut level takes its base point once no clause is fixable.
// LargestOrbitLast is the owning preprocessor's rule: the largest stabilizer
// orbit, the highest variable among equals, replaced only by a strictly better
// common-clause score met in ascending literal order.
enum class SymmetryCutPoint : uint8_t {
  FirstOpen,
  LargestOrbitLast,
};

enum class SymmetryStructuredScope : uint8_t {
  WholeMatrix,
  AxisAnchors,
};

// A kind-2-only FixAndCut batch normally contains open binary cuts and only
// chooses a CDCL fundamental domain.  Admission nevertheless inspects the live
// root values: a checked cut that has become unit is a forcing consequence.
enum class SymmetryNonforcingCutAdmission : uint8_t {
  Publish,
  RequireForcingMember,
  // Withhold a lone nonforcing cut. Certified forcing units always publish;
  // multiple nonforcing rows demonstrate within-pass progress.
  RequireCompoundProgress,
};

struct GraphSymmetryParameters {
  uint64_t maximum_graph_vertices = UINT64_MAX;
  uint64_t maximum_graph_edges = UINT64_MAX;
  uint64_t maximum_generators = UINT64_MAX;
  uint64_t backend_search_budget = UINT64_MAX;
  uint64_t backend_support_budget = UINT64_MAX;
  uint64_t backend_component_size_limit = UINT64_MAX;
  uint64_t maximum_fixing_instances = UINT64_MAX;
  uint64_t maximum_generator_instances_per_pass = UINT64_MAX;
  uint64_t maximum_group_ticks = UINT64_MAX;
  // Above this variable count, skip Schreier-chain construction and use the
  // exact-generator negation/binary fixing scans. UINT64_MAX disables the
  // switch. The default policy selects the owning baseline's 50,000 limit.
  uint64_t schreier_domain_limit = UINT64_MAX;
  // Optional soft planning threshold, polled between complete FixAndCut
  // levels. Zero disables it. Finalization still obeys maximum_group_ticks.
  uint64_t chain_planning_tick_limit = 0;
  uint32_t seed = 0;
  uint32_t error_bound = 15;
  SymmetryFixingMode fixing_mode =
      SymmetryFixingMode::StabilizerThenGenerators;
  SymmetryTraversalOrder generator_order = SymmetryTraversalOrder::Canonical;
  SymmetryTraversalOrder clause_order = SymmetryTraversalOrder::Canonical;
  SymmetryTraversalOrder literal_order = SymmetryTraversalOrder::Canonical;
  SymmetryClauseScope clause_scope = SymmetryClauseScope::UniqueLiteralsOnly;
  SymmetryPairOrientation pair_orientation =
      SymmetryPairOrientation::Canonical;
  SymmetryBinaryGraphMode binary_graph_mode =
      SymmetryBinaryGraphMode::Automatic;
  // Automatic mode uses direct binary edges when
  // 1000 * binary_clauses >= this value * variables. Zero therefore selects
  // direct edges for every nonempty formula; UINT32_MAX remains a valid very
  // high caller-selected threshold.
  uint32_t binary_graph_density_permille = 1000;
  SymmetryClauseSelection clause_selection =
      SymmetryClauseSelection::AmoThenShortest;
  SymmetryChainMode chain_mode = SymmetryChainMode::ClausalFixing;
  SymmetryBaseSelection base_selection = SymmetryBaseSelection::ClauseOrder;
  SymmetryCutPoint cut_point = SymmetryCutPoint::FirstOpen;
  // FixAndCut: before any fix or cut level, fix every open variable whose
  // stabilizer orbit holds its own complement, as a negation-orbit unit.
  bool negation_fixing_first = false;
  // A clause is fixable only through its first stored literal (the owner's
  // orbit_overlaps_clause); one whose first literal is false is left to a cut.
  bool first_literal_fix_anchor = false;
  bool direct_generator_involutions_only = true;
  bool decompose = true;
  bool prefer_depth_first = false;
  bool strong_certification = false;
  // Optional discovery subgroup: fix both literals of each assigned root
  // variable pointwise. Explicit SR and unrestricted discovery remain legal.
  bool pointwise_root_stabilizer = false;
  // Factor the supplied generator subgroup by disjoint supports before
  // planning. Independent of the selected chain algorithm.
  bool factor_support_components = false;
  // Preserve first detector occurrence before applying generator traversal.
  bool preserve_detector_order = false;
  // Read-only structured discovery in this invocation, before generic search.
  // Retain exact generators even when their matrix yields no immediate unit.
  bool use_structured_generators = false;
  // Independently restrict generic discovery to the pointwise stabilizer of
  // complete discovered matrices. This never restricts explicit X7 instances.
  bool stabilize_structured_variables = false;
  // AxisAnchors fixes the first column for a row action and the first row
  // for a column action, retaining more of the overlapping automorphisms.
  SymmetryStructuredScope structured_scope = SymmetryStructuredScope::WholeMatrix;
  SymmetryNonforcingCutAdmission nonforcing_cut_admission =
      SymmetryNonforcingCutAdmission::Publish;

  bool valid() const {
    return error_bound > 0 && fixing_mode >= SymmetryFixingMode::StabilizerOnly &&
           fixing_mode <= SymmetryFixingMode::StabilizerThenGenerators &&
           generator_order >= SymmetryTraversalOrder::Canonical &&
           generator_order <= SymmetryTraversalOrder::Seeded &&
           clause_order >= SymmetryTraversalOrder::Canonical &&
           clause_order <= SymmetryTraversalOrder::Seeded &&
           literal_order >= SymmetryTraversalOrder::Canonical &&
           literal_order <= SymmetryTraversalOrder::Seeded &&
           clause_scope >= SymmetryClauseScope::All &&
           clause_scope <= SymmetryClauseScope::UniqueLiteralsOnly &&
           pair_orientation >= SymmetryPairOrientation::Canonical &&
           pair_orientation <= SymmetryPairOrientation::Seeded &&
           binary_graph_mode >= SymmetryBinaryGraphMode::ClauseVertices &&
           binary_graph_mode <= SymmetryBinaryGraphMode::Automatic &&
           clause_selection >= SymmetryClauseSelection::InputOrder &&
           clause_selection <= SymmetryClauseSelection::AmoThenShortest &&
           chain_mode >= SymmetryChainMode::ClausalFixing &&
           chain_mode <= SymmetryChainMode::FixAndCutResift &&
           base_selection >= SymmetryBaseSelection::ClauseOrder &&
           base_selection <= SymmetryBaseSelection::SatsumaFixAndCut &&
           cut_point >= SymmetryCutPoint::FirstOpen &&
           cut_point <= SymmetryCutPoint::LargestOrbitLast &&
           structured_scope >= SymmetryStructuredScope::WholeMatrix &&
           structured_scope <= SymmetryStructuredScope::AxisAnchors &&
           nonforcing_cut_admission >=
               SymmetryNonforcingCutAdmission::Publish &&
           nonforcing_cut_admission <=
               SymmetryNonforcingCutAdmission::RequireCompoundProgress;
  }
  bool operator==(const GraphSymmetryParameters &) const = default;
};

struct SubstitutionRedundancyParameters {
  // Work spent validating the SR condition by unit propagation. UINT64_MAX is
  // unbounded; zero is a valid admission limit.
  uint64_t maximum_validation_ticks = UINT64_MAX;
  SubstitutionRedundancyProducer producer =
      SubstitutionRedundancyProducer::ExplicitOnly;
  OrbitopalOrientation orbitopal_orientation = OrbitopalOrientation::Rows;
  uint32_t minimum_orbitopal_rows = 2;
  uint32_t minimum_orbitopal_columns = 2;
  uint64_t maximum_orbitopal_matrices = UINT64_MAX;
  uint64_t maximum_orbitopal_instances = UINT64_MAX;
  // Preserve canonical order below this width. Five matches Satsuma's
  // admission rule; callers can select every legal width starting at two.
  uint32_t minimum_orbitopal_clique_columns = 5;
  uint64_t maximum_orbitopal_clique_vertices = UINT64_MAX;
  uint64_t maximum_orbitopal_clique_ticks = UINT64_MAX;
  // Maximum compact states in the induced column-action traversal used only
  // when exact product generators do not directly contain a column-fixing row
  // transposition. UINT64_MAX is unbounded; zero disables that fallback.
  uint64_t maximum_orbitopal_product_states = UINT64_MAX;
  // Complete every selected unique-literal clause with its missing pairwise
  // at-most-one binaries before attempting matrix detection. The binaries are
  // ordinary X7 instances with a blocked-clause certificate.
  bool complete_unique_literal_amos = false;
  // If individualized color refinement does not itself yield an exact row
  // action, optionally search the configured generic generators to complete
  // it. This is a scheduling choice because it can dominate detection time.
  bool orbitopal_generator_fallback = true;
  // Maximum candidate cells examined by individualized row/product
  // refinement in one generated pass, including pointwise-stabilizer
  // descendants. UINT64_MAX is unbounded; zero disables this detector while
  // leaving explicit matrices and the other orbitopal paths available.
  uint64_t maximum_orbitopal_refinement_orbits = UINT64_MAX;
  // Stop a structured refinement producer after it has spent the owning
  // Satsuma split allowance without completing a useful matrix.  Every
  // completed matrix resets the allowance, so productive discovery keeps
  // going.  This is a scheduling choice; disabling it retains exhaustive
  // traversal up to the explicit admission limits above.
  bool orbitopal_progress_feedback = false;
  // Satsuma-style row admission: probe a deterministic individualized base
  // and examine only root cells of size at most factor * base length. The
  // Boolean keeps the filter selectable; the positive factor is policy.
  bool orbitopal_base_probe_filter = true;
  uint32_t orbitopal_base_probe_factor = 8;
  uint64_t maximum_detection_ticks = UINT64_MAX;
  uint64_t maximum_general_candidates_per_advance = UINT64_MAX;
  OrbitopalColumnOrder orbitopal_column_order =
      OrbitopalColumnOrder::Canonical;
  OrbitopalCompletion orbitopal_completion = OrbitopalCompletion::MatrixOnly;
  OrbitopalDiscoveryOrder orbitopal_discovery_order =
      OrbitopalDiscoveryOrder::RefinementThenUniqueLiteral;
  // Equitable cells propose row candidates before generic group discovery.
  // They are not automorphism certificates; exact action checks still apply.
  OrbitopalRowOrbitSource orbitopal_row_orbit_source =
      OrbitopalRowOrbitSource::DetectedGenerators;
  GraphSymmetryParameters graph_symmetry;

  bool valid() const {
    const bool valid_producer =
        producer >= SubstitutionRedundancyProducer::ExplicitOnly &&
        producer <= SubstitutionRedundancyProducer::GraphSymmetry;
    const bool valid_orientation =
        orbitopal_orientation >= OrbitopalOrientation::Rows &&
        orbitopal_orientation <= OrbitopalOrientation::Both;
    const bool valid_column_order =
        orbitopal_column_order >= OrbitopalColumnOrder::Canonical &&
        orbitopal_column_order <=
            OrbitopalColumnOrder::CooccurrenceBoundedGreedyClique;
    const bool valid_completion =
        orbitopal_completion >= OrbitopalCompletion::MatrixOnly &&
        orbitopal_completion <=
            OrbitopalCompletion::IndividualizedGraphCompletion;
    const bool valid_discovery_order =
        orbitopal_discovery_order >=
            OrbitopalDiscoveryOrder::RefinementThenUniqueLiteral &&
        orbitopal_discovery_order <=
            OrbitopalDiscoveryOrder::UniqueLiteralOnly;
    const bool valid_row_orbit_source =
        orbitopal_row_orbit_source >= OrbitopalRowOrbitSource::DetectedGenerators &&
        orbitopal_row_orbit_source <= OrbitopalRowOrbitSource::RefinementCells;
    return valid_producer &&
           ((producer != SubstitutionRedundancyProducer::Orbitopal &&
             !(producer == SubstitutionRedundancyProducer::GraphSymmetry &&
               (graph_symmetry.use_structured_generators ||
                graph_symmetry.stabilize_structured_variables))) ||
            (valid_orientation && valid_column_order && valid_completion &&
             valid_discovery_order && valid_row_orbit_source &&
             minimum_orbitopal_rows >= 2 &&
             minimum_orbitopal_columns >= 2 &&
             orbitopal_base_probe_factor >= 1 &&
             minimum_orbitopal_clique_columns >= 2 &&
             (orbitopal_completion == OrbitopalCompletion::MatrixOnly ||
              graph_symmetry.valid()))) &&
           (producer != SubstitutionRedundancyProducer::GraphSymmetry ||
            graph_symmetry.valid());
  }
  bool operator==(const SubstitutionRedundancyParameters &) const = default;
};

enum class SubstitutionRedundancyApplicationStatus : uint8_t {
  NoChange,
  Applied,
  Unsat,
  Invalid,
  Inapplicable,
  UnsupportedProofFormat,
  Interrupted,
  ResourceExhausted,
};

struct SubstitutionRedundancyApplicationResult {
  SubstitutionRedundancyApplicationStatus status =
      SubstitutionRedundancyApplicationStatus::Invalid;
  ApplyResult rule_result = ApplyResult::NoOp;
  uint64_t validation_ticks = 0;
  uint64_t witness_literals = 0;
};

enum class OrbitopalEnumerationStatus : uint8_t {
  Complete,
  Interrupted,
  LimitReached,
};

struct OrbitopalEnumerationResult {
  OrbitopalEnumerationStatus status = OrbitopalEnumerationStatus::Complete;
  uint64_t matrices = 0;
  uint64_t instances = 0;
  uint64_t ticks = 0;
  uint64_t refinement_orbits = 0;
  uint64_t reduced_refinement_orbits = 0;
  uint64_t largest_refinement_orbit = 0;
  uint64_t largest_reduced_orbit = 0;
  uint64_t largest_refinement_columns = 0;
  uint64_t refinement_unique_literal_columns = 0;
  uint64_t refinement_exact_failures = 0;
  uint64_t reused_inexact_matrices = 0;
  uint64_t unproductive_refinement_stops = 0;
  uint64_t column_neighborhoods = 0;
  uint64_t reused_column_neighborhoods = 0;
  uint64_t column_transports = 0;
  uint64_t clique_budget_stops = 0;
  uint64_t largest_ordering_clique = 0;
  uint64_t generator_fallbacks = 0;
  uint64_t product_stabilizer_descents = 0;
  uint64_t product_shapes = 0;
  uint64_t largest_product_rows = 0;
  uint64_t largest_product_columns = 0;
  uint64_t product_exact_actions = 0;
  uint64_t product_ulc_actions = 0;
  // Color splits of anchor individualizations in row-column (product) and row
  // detection, the measure of Satsuma's split limit (symmetries.h:2041, 2950).
  uint64_t refinement_splits = 0;
  // Discovery stopped at a limit the owning preprocessor reports as
  // `reached_limits` (Satsuma symmetries.h: split limit 2041/2950, graph size
  // satsuma.h:168-176). Caller-selected orbit, clique, product-state and
  // matrix caps and the zero-progress work window do not set it.
  bool detection_limit_reached = false;
  // Discovery stopped because `maximum_detection_ticks` ran out. Only the
  // caller knows whether that allowance is an owner limit (the cumulative
  // cost ceiling) or a renewable scheduling window.
  bool allowance_limit_reached = false;
};

enum class SymmetryEnumerationStatus : uint8_t {
  Complete,
  Interrupted,
  LimitReached,
  Invalid,
  ResourceExhausted,
  Failed,
};

struct SymmetryEnumerationResult {
  SymmetryEnumerationStatus status = SymmetryEnumerationStatus::Complete;
  uint64_t generators_proposed = 0;
  uint64_t generators_validated = 0;
  // A work-capped generic detector filled its complete affordable callback
  // prefix.  No arbitrary prefix is publishable, but the count is a
  // deterministic discovery-progress signal for a composing strategy.
  uint64_t discovery_progress_units = 0;
  uint64_t instances = 0;
  // Complete validated kind-2-only batches withheld by strategy admission.
  uint64_t nonforcing_orbit_cuts_declined = 0;
  // Reserved historical diagnostic. Certified forcing units are never
  // withheld by cut-admission policy, so current producers leave this zero.
  uint64_t singleton_forcing_rows_declined = 0;
  uint64_t ticks = 0;
  // True only when discovery establishes the full unrestricted clause-set group.
  bool complete_group = false;
  // Discovery stopped at a limit the owning preprocessor reports as
  // `reached_limits` (Satsuma symmetries.h:579 DejaVu limits, 1468/3099
  // Schreier cost at the configured planning limit, satsuma.h:168-176 graph
  // size). A structured sub-discovery contributes its own flags.
  bool detection_limit_reached = false;
  // Discovery or group planning stopped because `maximum_detection_ticks`
  // ran out; the caller classifies that allowance (see the orbitopal field).
  bool allowance_limit_reached = false;
};

// Composition-only resource policy for graph-symmetry discovery. Once group
// planning has admitted a fixing batch, finishing and validating that atomic
// batch may exceed the fresh-discovery allowance; all actual work is still
// reported in the result and charged by the caller. The ordinary rule API
// retains its prior per-call accounting when this option is false.
struct SymmetryEnumerationScheduling {
  bool finish_admitted_group = false;
  // When nonzero, an exact generic generator extends the current discovery
  // allowance inside the same detector session.  hard_detection_ticks is the
  // remaining run-global ceiling at invocation entry.
  uint64_t progress_ticks_per_generator = 0;
  uint64_t hard_detection_ticks = 0;
};

struct SubstitutionRedundancyGeneralCursor {
  const Kernel *owner = nullptr;
  uint64_t arena_generation = 0;
  uint64_t touch_epoch = 0;
  std::vector<Var> variables;
  std::vector<uint32_t> candidate_digits;
  std::vector<uint32_t> witness_digits;
  std::vector<uint32_t> substitution_digits;
  uint32_t pivot_choice = 0;
  bool initialized = false;
  bool complete = false;
};

enum class SubstitutionRedundancyGeneralStatus : uint8_t {
  Found,
  Complete,
  Interrupted,
  ResourceExhausted,
  LimitReached,
};

struct SubstitutionRedundancyGeneralResult {
  SubstitutionRedundancyGeneralStatus status =
      SubstitutionRedundancyGeneralStatus::Complete;
  uint64_t candidates = 0;
};

// Caller-owned discovery advice in current internal literal coordinates.
// Consumers must validate maps against their current full formula; no cached
// validity or group-completeness claim survives mutation or compaction.
struct SymmetryDiscoveryHints {
  std::vector<std::vector<Lit>> generators;
  std::vector<Var> fixed_variables;
};

using OrbitopalInstanceVisitor =
    std::function<bool(const SubstitutionRedundancyInstance &)>;
// Formula snapshot and equitable refinement that consecutive generated
// orbitopal detections reuse while the formula is unchanged. Satsuma builds one
// symmetry graph and one refinement per pass and runs all its special
// detectors on it (satsuma.h generate_symmetry_predicate). Any formula, root
// assignment or arena change, a different kernel or different graph limits
// rebuild it. The owner must outlive every enumeration that uses it.
class OrbitopalFormulaCache {
public:
  OrbitopalFormulaCache();
  ~OrbitopalFormulaCache();
  OrbitopalFormulaCache(const OrbitopalFormulaCache &) = delete;
  OrbitopalFormulaCache &operator=(const OrbitopalFormulaCache &) = delete;
  struct State;
  std::unique_ptr<State> state;
};
OrbitopalEnumerationResult substitution_redundancy_enumerate_orbitopal(
    const Kernel &, const SubstitutionRedundancyParameters &,
    const OrbitopalInstanceVisitor &, volatile sig_atomic_t *stop = nullptr,
    OrbitopalFormulaCache *cache = nullptr);
OrbitopalEnumerationResult substitution_redundancy_discover_orbitopal_generators(
    const Kernel &, const SubstitutionRedundancyParameters &,
    SymmetryDiscoveryHints &, volatile sig_atomic_t *stop = nullptr);
// Lower bound on the vertices of the graph the GraphSymmetry producer builds
// for the live formula (literal, negation-gadget and clause vertices before
// duplicate-binary and root-unit adjustments). `work` receives the clause
// headers visited.
uint64_t substitution_redundancy_symmetry_graph_vertices(
    const Kernel &, const GraphSymmetryParameters &, uint64_t &work);
SymmetryEnumerationResult substitution_redundancy_enumerate_symmetry(
    const Kernel &, const SubstitutionRedundancyParameters &,
    const OrbitopalInstanceVisitor &, volatile sig_atomic_t *stop = nullptr,
    const SymmetryDiscoveryHints *hints = nullptr,
    SymmetryEnumerationScheduling scheduling = {});
SubstitutionRedundancyGeneralResult substitution_redundancy_next_general(
    const Kernel &, const SubstitutionRedundancyParameters &,
    SubstitutionRedundancyGeneralCursor &, SubstitutionRedundancyInstance &,
    volatile sig_atomic_t *stop = nullptr);

RuleEligibility substitution_redundancy_guard(const Kernel &);
ApplyResult substitution_redundancy_apply(Kernel &);
SubstitutionRedundancyApplicationResult substitution_redundancy_apply_detailed(
    Kernel &, const SubstitutionRedundancyInstance &,
    const SubstitutionRedundancyParameters & = {},
    volatile sig_atomic_t *stop = nullptr);

// Per-literal occurrences in live large clauses, reused to authenticate one
// freshly enumerated batch of unique-literal AMO binaries. The batch adds only
// binaries and root units; any arena change (generation, size, live
// irredundant count) rebuilds it, and a retired clause can only make an entry
// overcount, which declines the certificate. Callers discard it between
// batches, because in-place rewrites outside a batch are not keyed.
struct UniqueLiteralOccurrenceCache {
  const Kernel *owner = nullptr;
  uint64_t arena_generation = 0;
  size_t arena_size = 0;
  uint64_t live_irredundant_large = 0;
  std::vector<uint32_t> count;
  std::vector<Ref> last;
};
ApplyResult substitution_redundancy_apply(
    Kernel &, const SubstitutionRedundancyInstance &,
    const SubstitutionRedundancyParameters & = {},
    volatile sig_atomic_t *stop = nullptr);
ApplyResult substitution_redundancy_apply_orbitopal(
    Kernel &, const SubstitutionRedundancyParameters &,
    volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
