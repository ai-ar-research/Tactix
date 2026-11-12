#pragma once

#include <cstdint>

// A C-shaped boundary: DejaVu is compiled with exceptions and RTTI, the core
// without; the backend allocates and releases all returned storage.
namespace tactix::symmetry_backend {

enum class Status : uint8_t {
  Complete,
  Incomplete,
  Interrupted,
  Invalid,
  ResourceExhausted,
  Failed,
};

struct Graph {
  uint32_t vertex_count = 0;
  uint64_t edge_count = 0;
  const uint32_t *colors = nullptr;
  const uint32_t *degrees = nullptr;
  // Consecutive endpoint pairs: [u0,v0,u1,v1,...]. The graph must be simple;
  // a repeated edge is Invalid.
  const uint32_t *edges = nullptr;
};

struct DetectionSettings {
  uint32_t seed = 0;
  uint32_t error_bound = 15;
  // UINT64_MAX means the backend's unbounded setting.  Other values must fit
  // the corresponding DejaVu integer controls.
  uint64_t search_budget = UINT64_MAX;
  uint64_t support_budget = UINT64_MAX;
  uint64_t component_size_limit = UINT64_MAX;
  uint64_t maximum_generators = UINT64_MAX;
  // Bounded generator prefix, expanded by progress_generators_per_generator
  // per retained generator up to maximum_generators.
  uint64_t initial_generators = UINT64_MAX;
  uint64_t progress_generators_per_generator = 0;
  // Zero returns the full graph image. A nonzero prefix avoids retaining
  // clause-vertex images that an exact formula check does not consume.
  uint32_t returned_prefix_size = 0;
  bool decompose = true;
  bool prefer_depth_first = false;
  bool strong_certification = false;
  bool preserve_generator_order = false;
  // Internal transport choice only; identical maps/order/status in either
  // form. Dense output remains available to boundary/reference consumers.
  bool sparse_output = false;
};

struct Permutations {
  Status status = Status::Failed;
  bool complete_group = false;
  uint32_t domain_size = 0;
  uint64_t count = 0;
  // Row-major count x domain_size images.
  uint32_t *images = nullptr;
  // With offsets, images are targets paired with sorted image_points; missing
  // sources are identity. Targets still need the caller's validation.
  uint64_t *image_offsets = nullptr;
  uint32_t *image_points = nullptr;
  uint64_t image_entries = 0;
};

// Decode sparse row `row`, validating its sorted moved points first;
// `check_targets` also bounds every target by the domain.
template <class Rows>
inline bool materialize_sparse_image(const Rows &rows, uint64_t row, uint32_t *destination,
                                     bool check_targets) noexcept {
  const uint64_t begin = rows.image_offsets[row], end = rows.image_offsets[row + 1];
  if (rows.image_offsets[0] || rows.image_offsets[rows.count] != rows.image_entries ||
      begin > end || end > rows.image_entries ||
      (end && (!rows.image_points || !rows.images))) return false;
  for (uint64_t at = begin; at < end; ++at)
    if (rows.image_points[at] >= rows.domain_size ||
        (check_targets && rows.images[at] >= rows.domain_size) ||
        rows.image_points[at] == rows.images[at] ||
        (at > begin && rows.image_points[at - 1] >= rows.image_points[at]))
      return false;
  for (uint32_t point = 0; point < rows.domain_size; ++point)
    destination[point] = point;
  for (uint64_t at = begin; at < end; ++at)
    destination[rows.image_points[at]] = rows.images[at];
  return true;
}

inline bool materialize_permutation_image(const Permutations &rows, uint64_t row,
                                          uint32_t *destination) noexcept {
  if (row >= rows.count || !destination) return false;
  if (!rows.image_offsets) {
    if (!rows.images || (rows.domain_size &&
        rows.count > UINT64_MAX / rows.domain_size)) return false;
    for (uint32_t point = 0; point < rows.domain_size; ++point)
      destination[point] = rows.images[row * rows.domain_size + point];
    return true;
  }
  return materialize_sparse_image(rows, row, destination,
                                  /*check_targets=*/false);
}

// Individualize each anchor from the equitable root coloring and align
// singleton cells with the first anchor's refinement (UINT32_MAX: not aligned
// uniquely). Callers exact-check every completed action.
struct RefinementRows {
  Status status = Status::Failed;
  uint32_t anchor_count = 0;
  uint32_t domain_size = 0;
  uint64_t ticks = 0;
  uint32_t source_count = 0;
  // First-row prefix vertices in refinement discovery order. This preserves
  // the semantic column order used by Satsuma's row detector.
  uint32_t *sources = nullptr;
  // Row-major anchor_count x domain_size images.
  uint32_t *images = nullptr;
};

struct RefinementPartition {
  Status status = Status::Failed;
  uint32_t domain_size = 0;
  uint32_t cell_count = 0;
  // Equitable-root cell representative (the first vertex in the refined
  // cell) for every returned prefix vertex.
  uint32_t *cells = nullptr;
};

// Equitable partition after individualizing one root-cell member: a
// representative and full child-cell size per prefix vertex. Candidate data
// only.
struct IndividualizedPartition {
  Status status = Status::Failed;
  uint32_t domain_size = 0;
  uint32_t cell_count = 0;
  uint64_t ticks = 0;
  uint32_t *cells = nullptr;
  uint32_t *sizes = nullptr;
};

// Satsuma-style candidate reduction after individualizing the first member
// of an equitable root cell: the original cell, or the anchor followed by the
// largest proper remainder cell. Callers exact-check every permutation.
struct RefinementOrbit {
  Status status = Status::Failed;
  uint32_t count = 0;
  uint64_t ticks = 0;
  uint32_t *vertices = nullptr;
};

// Opaque prepared graph and equitable root coloring, reusable across anchor
// cells of one formula.
struct RefinementSession;
// Mutable pointwise-stabilizer branch advancing from the preceding coloring;
// its RefinementSession must outlive it.
struct RefinementBranch;

struct GroupInput {
  uint32_t domain_size = 0;
  uint64_t generator_count = 0;
  // Row-major generator_count x domain_size images.
  const uint32_t *generator_images = nullptr;
  uint64_t clause_count = 0;
  // clause_offsets has clause_count + 1 entries and ends at literal_count.
  const uint64_t *clause_offsets = nullptr;
  uint64_t literal_count = 0;
  const uint32_t *literals = nullptr;
  // -1 false, 0 unassigned, +1 true for every point in the domain.
  const int8_t *values = nullptr;
  // One byte per clause. A nonzero value certifies that every pair of clause
  // literals has its pairwise negative binary in the formula snapshot.
  const uint8_t *clause_is_amo = nullptr;
  // Optional sparse generator rows (offsets select them): targets paired with
  // sorted points, missing sources identity, equal moved source/target sets.
  const uint64_t *generator_offsets = nullptr;
  const uint32_t *generator_points = nullptr;
  uint64_t generator_entries = 0;
};

struct GroupSettings {
  uint32_t seed = 0;
  uint32_t error_bound = 15;
  uint64_t maximum_instances = UINT64_MAX;
  uint64_t maximum_ticks = UINT64_MAX;
  // 0: clausal fixing; 1: two-pass fix-and-cut; 2: also re-sift stabilizer
  // generators at every base extension (typed enums live with
  // SubstitutionRedundancyParameters).
  uint8_t chain_mode = 0;
  // 0: caller clause/literal order. 1: common-clause and AMO-aware ranking.
  // 2: first AMO in caller order, else common-clause ranking. 3: that fallback
  // once, then caller order. 4: 3 plus a forward scan frontier at the first
  // eligible clause. 5: shortest active clause, then AMO/common-clause scoring
  // and smaller-orbit ties. 6: ranking at the first level, then a caller-order
  // frontier from clause zero.
  uint8_t base_selection = 0;
  // Optional threshold between completed FixAndCut planning levels. Zero
  // disables it; maximum_ticks independently bounds planning and finalization.
  uint64_t planning_tick_limit = 0;
  bool factor_support_components = false;
  // FixAndCut only. 0: ascending base point (ranked by common clauses and
  // larger orbit when base_selection ranks). 1: the owner's rule: largest
  // stabilizer orbit, highest variable among equals, replaced only by a
  // strictly better common-clause score in ascending literal order.
  uint8_t cut_point = 0;
  // FixAndCut only: first take, ascending, every open variable whose
  // stabilizer orbit holds its complement; its base is the negative literal
  // and it materializes as a negation-orbit unit (kind 3).
  bool negation_fixing_first = false;
  // FixAndCut and clausal fixing: anchor a fixable clause on its first stored
  // literal (owner's orbit_overlaps_clause). Default: any clause whose open
  // literals share an orbit fixes.
  bool first_literal_fix_anchor = false;
};

struct Fixings {
  Status status = Status::Failed;
  uint32_t domain_size = 0;
  uint64_t count = 0;
  uint64_t ticks = 0;
  // 0 is a clausal-fixing binary, 1 its final representative unit, 2 a full
  // orbit-cut binary, and 3 a negation-orbit cut unit.
  uint8_t *kinds = nullptr;
  // Zero-based Schreier level for each row. Levels with no emitted action are
  // permitted, so these values need not be contiguous.
  uint32_t *levels = nullptr;
  uint32_t *representatives = nullptr;
  uint32_t *others = nullptr;
  // Row-major count x domain_size images; a binary row is the inverse
  // transversal h with h(representative)=other, unit rows are identity.
  uint32_t *images = nullptr;
  // Optional compact form: count+1 offsets into parallel sorted
  // moved-point/target arrays of image_entries elements; absent points are
  // fixed. Without offsets, images is dense row-major.
  uint64_t *image_offsets = nullptr;
  uint32_t *image_points = nullptr;
  uint64_t image_entries = 0;
};

inline bool materialize_fixing_image(const Fixings &rows, uint64_t row,
                                     uint32_t *destination) noexcept {
  if (row >= rows.count || !destination)
    return false;
  if (!rows.image_offsets) {
    if (!rows.images || (rows.domain_size && row > UINT64_MAX / rows.domain_size))
      return false;
    for (uint32_t point = 0; point < rows.domain_size; ++point)
      destination[point] = rows.images[row * rows.domain_size + point];
    return true;
  }
  return materialize_sparse_image(rows, row, destination,
                                  /*check_targets=*/true);
}

using StopCallback = bool (*)(void *) noexcept;

// Borrowed completed-row prefix, valid only for the feedback call. Remaining
// rows are not available to the caller. No vertex encoding is assumed here.
struct RefinementRowPrefix {
  uint32_t completed_rows;
  uint32_t anchor_count;
  uint32_t domain_size;
  const uint32_t *images;
};
using RowWorkCallback = bool (*)(void *);
// Return false to decline this completion (Complete with no image). All work
// must first be admitted by charge_work; a refusal yields
// Interrupted/Incomplete. Feedback must not mutate or retain the graph.
using RowFeedbackCallback = bool (*)(const RefinementRowPrefix &,
                                    RowWorkCallback charge_work,
                                    void *work_context, void *feedback_context);


// Malformed graph/callback metadata returns Invalid with no owned output.
Status detect(const Graph &, const DetectionSettings &, StopCallback,
              void *stop_context, Permutations &) noexcept;
void release(Permutations &) noexcept;
Status refine_rows(const Graph &, const uint32_t *anchors,
                   uint32_t anchor_count, uint32_t returned_prefix_size,
                   uint64_t maximum_ticks, StopCallback, void *stop_context,
                   RefinementRows &) noexcept;
Status prepare_refinement(const Graph &, uint64_t maximum_ticks, StopCallback,
                          void *stop_context, RefinementSession *&,
                          uint64_t &ticks) noexcept;
Status refine_rows(RefinementSession &, const uint32_t *anchors,
                   uint32_t anchor_count, uint32_t returned_prefix_size,
                   uint64_t maximum_ticks, StopCallback, void *stop_context,
                   RefinementRows &) noexcept;
Status refine_rows(RefinementSession &, const uint32_t *anchors,
                   uint32_t anchor_count, uint32_t returned_prefix_size,
                   uint64_t maximum_ticks, StopCallback, void *stop_context,
                   RefinementRows &, RowFeedbackCallback,
                   void *feedback_context) noexcept;
Status root_partition(RefinementSession &, uint32_t returned_prefix_size,
                      RefinementPartition &) noexcept;
Status prepare_refinement_branch(RefinementSession &,
                                 RefinementBranch *&) noexcept;
Status individualized_partition(RefinementBranch &, uint32_t anchor,
                                uint32_t returned_prefix_size,
                                uint64_t maximum_ticks, StopCallback,
                                void *stop_context,
                                IndividualizedPartition &) noexcept;
Status individualized_partition(RefinementSession &, const uint32_t *anchors,
                                uint32_t anchor_count,
                                uint32_t returned_prefix_size,
                                uint64_t maximum_ticks, StopCallback,
                                void *stop_context,
                                IndividualizedPartition &) noexcept;
Status individualized_partition(RefinementSession &, uint32_t anchor,
                                uint32_t returned_prefix_size,
                                uint64_t maximum_ticks, StopCallback,
                                void *stop_context,
                                IndividualizedPartition &) noexcept;
Status reduce_orbit(RefinementSession &, const uint32_t *vertices,
                    uint32_t count, uint32_t returned_prefix_size,
                    uint64_t maximum_ticks, StopCallback, void *stop_context,
                    RefinementOrbit &) noexcept;
Status probe_base_length(RefinementSession &, uint64_t maximum_ticks,
                         StopCallback, void *stop_context, uint32_t &length,
                         uint64_t &ticks) noexcept;
void release(RefinementSession *&) noexcept;
void release(RefinementBranch *&) noexcept;
void release(RefinementRows &) noexcept;
void release(RefinementPartition &) noexcept;
void release(IndividualizedPartition &) noexcept;
void release(RefinementOrbit &) noexcept;
Status fix_clauses(const GroupInput &, const GroupSettings &, StopCallback,
                   void *stop_context, Fixings &) noexcept;
void release(Fixings &) noexcept;
} // namespace tactix::symmetry_backend
