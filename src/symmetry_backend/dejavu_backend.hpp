#pragma once

#include <cstdint>

// This boundary is deliberately C-shaped.  The DejaVu translation unit is
// compiled with exceptions and RTTI while every caller in the solver core is
// compiled without them.  All storage returned here is allocated and released
// by the backend.
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
  // Start with this bounded prefix.  Each retained generator expands the
  // live cap by progress_generators_per_generator, without restarting DejaVu,
  // until maximum_generators or detector completion.
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
  // sources are identity. Targets may lie outside a requested prefix, exactly
  // as in dense output, and still require the caller's semantic validation.
  uint64_t *image_offsets = nullptr;
  uint32_t *image_points = nullptr;
  uint64_t image_entries = 0;
};

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
  const uint64_t begin = rows.image_offsets[row], end = rows.image_offsets[row + 1];
  if (rows.image_offsets[0] || rows.image_offsets[rows.count] != rows.image_entries ||
      begin > end || end > rows.image_entries ||
      (end && (!rows.image_points || !rows.images))) return false;
  for (uint64_t at = begin; at < end; ++at)
    if (rows.image_points[at] >= rows.domain_size ||
        rows.image_points[at] == rows.images[at] ||
        (at > begin && rows.image_points[at - 1] >= rows.image_points[at]))
      return false;
  for (uint32_t point = 0; point < rows.domain_size; ++point)
    destination[point] = point;
  for (uint64_t at = begin; at < end; ++at)
    destination[rows.image_points[at]] = rows.images[at];
  return true;
}

// Individualize each anchor from the same equitable root coloring and align
// singleton cells with the first anchor's refinement. UINT32_MAX marks a
// prefix vertex which refinement did not align uniquely. Callers still exact
// check every completed action against their own semantic representation.
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

// Equitable partition after individualizing one root-cell member.  `cells`
// stores a representative for every returned prefix vertex and `sizes`
// stores the full child-cell size.  This is candidate-generation data only;
// callers must still certify any permutation inferred from it.
struct IndividualizedPartition {
  Status status = Status::Failed;
  uint32_t domain_size = 0;
  uint32_t cell_count = 0;
  uint64_t ticks = 0;
  uint32_t *cells = nullptr;
  uint32_t *sizes = nullptr;
};

// Satsuma-style candidate reduction after individualizing the first member of
// an equitable root cell.  The returned list is either the original cell or
// the anchor followed by the largest proper remainder cell exposed by
// refinement.  This is a candidate-generation operation only; callers must
// still exact-check every completed permutation.
struct RefinementOrbit {
  Status status = Status::Failed;
  uint32_t count = 0;
  uint64_t ticks = 0;
  uint32_t *vertices = nullptr;
};

// Opaque prepared graph and equitable root coloring.  A caller that refines
// several anchor cells of the same formula can prepare once and reuse the
// session; the backend retains all DejaVu-owned state behind this boundary.
struct RefinementSession;
// Mutable pointwise-stabilizer branch. Each successful advance starts from the
// preceding refined coloring, so a deep branch does not replay its prefix.
// The parent RefinementSession must outlive the branch.
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
  // Optional sparse generator rows. Offsets select this representation;
  // generator_images are targets paired with sorted generator_points. Missing
  // sources are identity. The moved source and target sets must be identical.
  const uint64_t *generator_offsets = nullptr;
  const uint32_t *generator_points = nullptr;
  uint64_t generator_entries = 0;
};

struct GroupSettings {
  uint32_t seed = 0;
  uint32_t error_bound = 15;
  uint64_t maximum_instances = UINT64_MAX;
  uint64_t maximum_ticks = UINT64_MAX;
  // Zero preserves the historical clausal-fixing producer. One selects the
  // two-pass fix-and-cut chain; two additionally re-sifts retained stabilizer
  // generators at every base extension. The backend uses a narrow numeric ABI;
  // the typed public enums live with SubstitutionRedundancyParameters.
  uint8_t chain_mode = 0;
  // Zero preserves caller clause/literal order. One selects common-clause and
  // AMO-aware base ranking. Two takes the first AMO in caller order, then
  // falls back to common-clause ranking when no AMO is applicable. Three uses
  // that fallback once, then caller order for the rest of the chain. Four
  // additionally retains the first eligible clause as a forward scan frontier.
  // Five first minimizes active clause length, then uses AMO/common-clause
  // scoring and smaller-orbit ties without an early first-AMO exit.
  // Six uses that ranking for the initial level, then a forward caller-order
  // frontier for subsequent levels, starting the remainder at clause zero.
  uint8_t base_selection = 0;
  // Optional threshold between completed FixAndCut planning levels. Zero
  // disables it; maximum_ticks independently bounds planning and finalization.
  uint64_t planning_tick_limit = 0;
  bool factor_support_components = false;
  // FixAndCut only. Zero takes a cut level's base point in ascending point
  // order (ranked by common clauses and larger orbit when base_selection
  // ranks). One is the owner's rule: the largest stabilizer orbit, the highest
  // variable among equals, replaced only by a strictly better common-clause
  // score met in ascending literal order.
  uint8_t cut_point = 0;
  // FixAndCut only. Before any fix or cut level, take in ascending order every
  // open variable whose stabilizer orbit holds its own complement; each such
  // level's base is the variable's negative literal and materializes as that
  // negation-orbit unit (kind 3), which later levels read as a root value.
  bool negation_fixing_first = false;
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
  // Row-major count x domain_size images. A binary row is the inverse
  // transversal h with h(representative)=other. Unit rows are identity and
  // are retained only to keep this ownership boundary flat.
  uint32_t *images = nullptr;
  // Optional compact form: count+1 offsets into sorted moved-point/target
  // pairs: images and image_points are parallel arrays of image_entries
  // elements. This discriminator selects exactly one representation.
  // Points absent from a row are fixed. Without offsets, images keeps
  // its historical dense row-major layout. Component exports use this form
  // to avoid count * global-domain identity matrices.
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
  const uint64_t begin = rows.image_offsets[row], end = rows.image_offsets[row + 1];
  if (rows.image_offsets[0] || rows.image_offsets[rows.count] != rows.image_entries ||
      begin > end || end > rows.image_entries ||
      (end && (!rows.image_points || !rows.images)))
    return false;
  // Validate before touching caller scratch, including rejected rows.
  for (uint64_t at = begin; at < end; ++at) {
    const uint32_t point = rows.image_points[at], target = rows.images[at];
    if (point >= rows.domain_size || target >= rows.domain_size || point == target ||
        (at > begin && rows.image_points[at - 1] >= point))
      return false;
  }
  for (uint32_t point = 0; point < rows.domain_size; ++point)
    destination[point] = point;
  for (uint64_t at = begin; at < end; ++at)
    destination[rows.image_points[at]] = rows.images[at];
  return true;
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
// Return false to decline this completion and receive Complete with no image.
// Every unit of caller work must first be admitted by charge_work. A refused
// charge overrides the decision with Interrupted/Incomplete. Feedback must
// not mutate the prepared graph, recursively refine it, or retain the view.
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
