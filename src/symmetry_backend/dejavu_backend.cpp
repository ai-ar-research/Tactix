#include "symmetry_backend/dejavu_backend.hpp"
#include "symmetry_backend/sparse_permutation.hpp"
#include "symmetry_backend/group_input.hpp"
#include "diagnostics/symmetry_trace.hpp"

#include "dejavu.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <vector>
#include <unordered_map>

#include "symmetry_backend/component_factorization.hpp"

namespace tactix::symmetry_backend {
namespace {

struct generator_limit_requested {};

bool stopped(StopCallback callback, void *context) noexcept {
  return callback && callback(context);
}

bool as_backend_int(uint64_t value, int &result) noexcept {
  if (value == UINT64_MAX) {
    result = -1;
    return true;
  }
  if (value > static_cast<uint64_t>(std::numeric_limits<int>::max()))
    return false;
  result = static_cast<int>(value);
  return true;
}

bool as_backend_i64(uint64_t value, int64_t &result) noexcept {
  if (value == UINT64_MAX) {
    result = -1;
    return true;
  }
  if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
    return false;
  result = static_cast<int64_t>(value);
  return true;
}

enum class GraphShape : uint8_t { Simple, RepeatedEdge, Stopped };

// DejaVu requires a simple graph (its debug-only sgraph::sanity_check asserts
// the multiedge test); a repeated edge corrupts partition refinement. The scan
// polls like the graph copy that precedes it.
GraphShape graph_shape(const dejavu::sgraph &graph, StopCallback stop,
                       void *stop_context) {
  std::vector<int> seen(static_cast<size_t>(graph.v_size), -1);
  for (int vertex = 0; vertex < graph.v_size; ++vertex) {
    if (!(vertex & 1023) && stopped(stop, stop_context))
      return GraphShape::Stopped;
    for (int at = graph.v[vertex]; at < graph.v[vertex] + graph.d[vertex];
         ++at) {
      if (seen[graph.e[at]] == vertex)
        return GraphShape::RepeatedEdge;
      seen[graph.e[at]] = vertex;
    }
  }
  return GraphShape::Simple;
}

} // namespace

void release(Permutations &result) noexcept {
  std::free(result.images);
  std::free(result.image_offsets);
  std::free(result.image_points);
  result = {};
}

void release(RefinementRows &result) noexcept {
  std::free(result.sources);
  std::free(result.images);
  result = {};
}

void release(RefinementPartition &result) noexcept {
  std::free(result.cells);
  result = {};
}

void release(IndividualizedPartition &result) noexcept {
  std::free(result.cells);
  std::free(result.sizes);
  result = {};
}

void release(RefinementOrbit &result) noexcept {
  std::free(result.vertices);
  result = {};
}

void release(Fixings &result) noexcept {
  std::free(result.kinds);
  std::free(result.levels);
  std::free(result.representatives);
  std::free(result.others);
  std::free(result.images);
  std::free(result.image_offsets);
  std::free(result.image_points);
  result = {};
}

Status detect(const Graph &input, const DetectionSettings &settings,
              StopCallback stop, void *stop_context,
              Permutations &result) noexcept {
  release(result);
  if (!input.vertex_count ||
      (input.edge_count && !input.edges) || !input.colors || !input.degrees ||
      input.vertex_count > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      input.edge_count > static_cast<uint64_t>(std::numeric_limits<int>::max()) ||
      !settings.error_bound ||
      settings.error_bound > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      settings.returned_prefix_size > input.vertex_count) {
    result.status = Status::Invalid;
    return result.status;
  }
  int search_budget = -1;
  int support_budget = -1;
  int64_t component_limit = -1;
  if (!as_backend_int(settings.search_budget, search_budget) ||
      !as_backend_int(settings.support_budget, support_budget) ||
      !as_backend_i64(settings.component_size_limit, component_limit)) {
    result.status = Status::Invalid;
    return result.status;
  }
  if (stopped(stop, stop_context)) {
    result.status = Status::Interrupted;
    return result.status;
  }

  try {
    const SymmetryTrace trace;
    const auto poll = [&]() {
      if (stopped(stop, stop_context))
        throw dejavu::termination_requested();
    };
    dejavu::static_graph graph(static_cast<int>(input.vertex_count),
                               static_cast<int>(input.edge_count));
    for (uint32_t vertex = 0; vertex < input.vertex_count; ++vertex) {
      if (!(vertex & 1023u))
        poll();
      if (input.colors[vertex] >
              static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
          input.degrees[vertex] >
              static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        result.status = Status::Invalid;
        return result.status;
      }
      graph.add_vertex(static_cast<int>(input.colors[vertex]),
                       static_cast<int>(input.degrees[vertex]));
    }
    for (uint64_t edge = 0; edge < input.edge_count; ++edge) {
      if (!(edge & 1023u))
        poll();
      const uint32_t first = input.edges[2 * edge];
      const uint32_t second = input.edges[2 * edge + 1];
      if (first >= input.vertex_count || second >= input.vertex_count) {
        result.status = Status::Invalid;
        return result.status;
      }
      graph.add_edge(static_cast<int>(first), static_cast<int>(second));
    }
    switch (graph_shape(*graph.get_sgraph(), stop, stop_context)) {
    case GraphShape::Simple:
      break;
    case GraphShape::RepeatedEdge:
      result.status = Status::Invalid;
      return result.status;
    case GraphShape::Stopped:
      result.status = Status::Interrupted;
      return result.status;
    }

    dejavu::solver solver;
    solver.set_print(false);
    solver.set_seed(std::bit_cast<int32_t>(settings.seed));
    solver.set_error_bound(static_cast<int>(settings.error_bound));
    solver.set_limit_budget(search_budget);
    solver.set_limit_schreier_support(support_budget);
    solver.set_limit_component(component_limit);
    solver.set_decompose(settings.decompose);
    solver.set_prefer_dfs(settings.prefer_depth_first);
    solver.set_strong_certification(settings.strong_certification);
    solver.set_termination_hook(stop, stop_context);

    const uint32_t returned_domain = settings.returned_prefix_size
                                         ? settings.returned_prefix_size
                                         : input.vertex_count;
    std::vector<detail::SparsePermutation> found;
    bool invalid_callback = false;
    bool generator_limit_reached = false;
    uint64_t live_generator_limit =
        std::min(settings.initial_generators, settings.maximum_generators);
    uint64_t callbacks = 0, support_sum = 0, dropped = 0;
    dejavu_hook hook = [&](int domain, const int *permutation, int support_size,
                           const int *support) {
      if (trace.enabled()) {
        ++callbacks;
        support_sum += static_cast<uint64_t>(std::max(0, support_size));
      }
      if (stopped(stop, stop_context))
        return;
      if (domain != static_cast<int>(input.vertex_count))
        return;
      detail::SparsePermutation image;
      if (!detail::capture_permutation_prefix(domain, permutation, support_size,
                                              support, returned_domain, image)) {
        invalid_callback = true;
        return;
      }
#ifndef NDEBUG
      // Pin the vendor invariant: all moved points occur in its support list.
      // Signed bijectivity alone would not prove that a truncated permutation
      // is still an automorphism, especially for the direct-binary consumer.
      size_t moved_at = 0;
      for (uint32_t point = 0; point < returned_domain; ++point) {
        uint32_t target = point;
        if (moved_at < image.moved.size() && image.moved[moved_at].first == point)
          target = image.moved[moved_at++].second;
        assert(target == static_cast<uint32_t>(permutation[point]));
      }
#endif
      if (image.moved.empty())
        return;
      if (found.size() >= live_generator_limit) {
        generator_limit_reached = true;
        if (trace.enabled())
          ++dropped;
        throw generator_limit_requested{};
      }
      if (trace.detailed()) {
        // Preserve the existing full-image diagnostic fingerprint. This one
        // temporary is never retained by the detector or used for scheduling.
        std::vector<uint32_t> dense(returned_domain);
        for (uint32_t point = 0; point < returned_domain; ++point) dense[point] = point;
        for (const auto &[point, target] : image.moved) dense[point] = target;
        trace.permutation("backend_generator", found.size(), dense.data(), returned_domain);
      }
      found.push_back(std::move(image));
      if (settings.progress_generators_per_generator &&
          live_generator_limit < settings.maximum_generators) {
        const uint64_t room =
            settings.maximum_generators - live_generator_limit;
        live_generator_limit +=
            std::min(room, settings.progress_generators_per_generator);
      }
      // A finite caller cap is a work limit, not merely an output truncation.
      // Stop as soon as the requested prefix exists instead of continuing an
      // automorphism search whose further callbacks must all be discarded.
      if (found.size() == live_generator_limit) {
        generator_limit_reached = true;
        throw generator_limit_requested{};
      }
    };
    try {
      solver.automorphisms(&graph, &hook);
    } catch (const generator_limit_requested &) {
      generator_limit_reached = true;
    }
    trace.event("backend_detection", "callbacks\t%llu\tsupport_sum\t%llu\t"
                "retained\t%zu\tdropped\t%llu\tinitial_cap\t%llu\tcap\t%llu\t"
                "backend_limit\t%u\tdeterministic\t%u",
                static_cast<unsigned long long>(callbacks),
                static_cast<unsigned long long>(support_sum), found.size(),
                static_cast<unsigned long long>(dropped),
                static_cast<unsigned long long>(settings.initial_generators),
                static_cast<unsigned long long>(settings.maximum_generators),
                solver.get_reached_limit(),
                solver.get_deterministic_termination());

    if (stopped(stop, stop_context)) {
      result.status = Status::Interrupted;
      return result.status;
    }
    if (invalid_callback) {
      // Broken callback metadata is an invalid backend result, not a smaller
      // complete group. As on other Invalid exits, no output storage escapes.
      result.status = Status::Invalid;
      return result.status;
    }
    if (settings.preserve_generator_order) {
      // Sort indices to identify duplicates without copying full images or
      // changing first callback occurrence. The canonical path stays intact.
      std::vector<size_t> order(found.size());
      for (size_t i = 0; i < order.size(); ++i) order[i] = i;
      std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return found[a] < found[b];
      });
      std::vector<bool> duplicate(found.size(), false);
      for (size_t i = 1; i < order.size(); ++i)
        duplicate[order[i]] = found[order[i]] == found[order[i - 1]];
      size_t kept = 0;
      for (size_t i = 0; i < found.size(); ++i)
        if (!duplicate[i]) {
          if (kept != i) found[kept] = std::move(found[i]);
          ++kept;
        }
      found.resize(kept);
    } else {
      std::sort(found.begin(), found.end());
      found.erase(std::unique(found.begin(), found.end()), found.end());
    }
    if (found.size() > settings.maximum_generators)
      found.resize(static_cast<size_t>(settings.maximum_generators));

    uint64_t entries = 0;
    for (const auto &image : found) {
      if (image.moved.size() > UINT64_MAX - entries) throw std::bad_alloc();
      entries += image.moved.size();
    }
    if (settings.sparse_output) {
      if (found.size() >= SIZE_MAX / sizeof(uint64_t) ||
          entries > SIZE_MAX / sizeof(uint32_t)) throw std::bad_alloc();
      result.image_offsets = static_cast<uint64_t *>(
          std::malloc((found.size() + 1) * sizeof(uint64_t)));
      if (!result.image_offsets) throw std::bad_alloc();
      if (entries) {
        result.images = static_cast<uint32_t *>(
            std::malloc(static_cast<size_t>(entries) * sizeof(uint32_t)));
        result.image_points = static_cast<uint32_t *>(
            std::malloc(static_cast<size_t>(entries) * sizeof(uint32_t)));
        if (!result.images || !result.image_points) throw std::bad_alloc();
      }
      uint64_t at = 0;
      result.image_offsets[0] = 0;
      for (size_t row = 0; row < found.size(); ++row) {
        for (const auto &[point, target] : found[row].moved) {
          result.image_points[at] = point;
          result.images[at++] = target;
        }
        result.image_offsets[row + 1] = at;
      }
      result.image_entries = entries;
    } else {
      if (returned_domain && found.size() > SIZE_MAX / sizeof(uint32_t) / returned_domain)
        throw std::bad_alloc();
      const size_t cells = found.size() * returned_domain;
      if (cells) {
        result.images = static_cast<uint32_t *>(std::malloc(cells * sizeof(uint32_t)));
        if (!result.images) throw std::bad_alloc();
        for (size_t row = 0; row < found.size(); ++row) {
          auto *image = result.images + row * returned_domain;
          for (uint32_t point = 0; point < returned_domain; ++point) image[point] = point;
          for (const auto &[point, target] : found[row].moved) image[point] = target;
        }
      }
    }
    trace.event("backend_transport", "sparse\t%u\tdomain\t%u\tmaps\t%zu\tmoved_entries\t%llu",
                static_cast<unsigned>(settings.sparse_output), returned_domain,
                found.size(), static_cast<unsigned long long>(entries));
    result.domain_size = returned_domain;
    result.count = found.size();
    const bool resource_limit_reached =
        solver.get_reached_limit() || generator_limit_reached;
    result.complete_group = solver.get_deterministic_termination() &&
                            !resource_limit_reached;
    // Probabilistic Schreier termination completed the configured search even
    // when it did not prove that the returned generators span the full group.
    // Keep that fact in complete_group; Incomplete means an actual resource
    // cut selected by the caller.
    result.status = resource_limit_reached ? Status::Incomplete : Status::Complete;
    return result.status;
  } catch (const dejavu::termination_requested &) {
    release(result);
    result.status = Status::Interrupted;
    return result.status;
  } catch (const std::bad_alloc &) {
    release(result);
    result.status = Status::ResourceExhausted;
    return result.status;
  } catch (...) {
    release(result);
    result.status = Status::Failed;
    return result.status;
  }
}

// Completed first-anchor refinement only, scoped to one immutable session.
// Work and stop events retain the original logical accounting on reuse.
struct FirstRefinement {
  struct Event { uint64_t work; int root_color; int size; int vertex; };
  std::unique_ptr<dejavu::ds::coloring> child;
  std::vector<Event> events;
  uint32_t anchor = UINT32_MAX;
  uint64_t ticks = 0;
};

struct RefinementSession {
  std::unique_ptr<dejavu::static_graph> graph;
  dejavu::sgraph *internal = nullptr;
  dejavu::ds::coloring root;
  std::unique_ptr<FirstRefinement> first_refinement;
  uint32_t vertex_count = 0;
};

struct RefinementBranch {
  dejavu::sgraph *internal = nullptr;
  dejavu::ds::coloring coloring;
  dejavu::ir::refinement refinement;
  uint32_t vertex_count = 0;
};

void release(RefinementSession *&session) noexcept {
  delete session;
  session = nullptr;
}

void release(RefinementBranch *&branch) noexcept {
  delete branch;
  branch = nullptr;
}

class RefinementMeter {
public:
  RefinementMeter(uint64_t &ticks, uint64_t maximum, StopCallback stop,
                  void *context, bool &exhausted, bool &interrupted)
      : ticks(ticks), maximum(maximum), stop(stop), context(context),
        exhausted(exhausted), interrupted(interrupted) {}

  std::function<dejavu::ir::type_worklist_color_hook>
  worklist_hook(dejavu::sgraph *graph, dejavu::ds::coloring &coloring) {
    return [this, graph, &coloring](int color, int size) {
      return charge(static_cast<uint64_t>(graph->d[coloring.lab[color]]),
                    static_cast<uint64_t>(size));
    };
  }

  std::function<dejavu::ir::type_worklist_color_hook>
  worklist_hook(dejavu::sgraph *graph, dejavu::ds::coloring *&coloring) {
    return [this, graph, &coloring](int color, int size) {
      return charge(static_cast<uint64_t>(graph->d[coloring->lab[color]]),
                    static_cast<uint64_t>(size));
    };
  }

  std::function<dejavu::ir::type_split_color_hook> split_hook() {
    return [this](int, int, int) {
      if (stopped(stop, context)) {
        interrupted = true;
        return false;
      }
      return !exhausted;
    };
  }

  bool charge(uint64_t degree, uint64_t width) {
    return charge(degree, width, [](uint64_t) {});
  }

  template <class Observer>
  bool charge(uint64_t degree, uint64_t width, Observer observe) {
    if (stopped(stop, context)) {
      interrupted = true;
      return false;
    }
    if (exhausted)
      return false;
    const uint64_t work =
        degree && width > (UINT64_MAX - 1) / degree
            ? UINT64_MAX
            : 1 + degree * width;
    observe(work);
    if (ticks >= maximum || work > maximum - ticks) {
      ticks = maximum;
      exhausted = true;
      return false;
    }
    ticks += work;
    return true;
  }

private:
  uint64_t &ticks;
  uint64_t maximum;
  StopCallback stop;
  void *context;
  bool &exhausted;
  bool &interrupted;
};

Status materialize_partition(dejavu::ds::coloring &coloring,
                             uint32_t vertex_count,
                             uint32_t returned_prefix_size,
                             IndividualizedPartition &result) {
  if (returned_prefix_size > SIZE_MAX / sizeof(uint32_t)) {
    result.status = Status::ResourceExhausted;
    return result.status;
  }
  result.cells = static_cast<uint32_t *>(std::malloc(
      static_cast<size_t>(returned_prefix_size) * sizeof(uint32_t)));
  result.sizes = static_cast<uint32_t *>(std::malloc(
      static_cast<size_t>(returned_prefix_size) * sizeof(uint32_t)));
  if (returned_prefix_size && (!result.cells || !result.sizes)) {
    release(result);
    result.status = Status::ResourceExhausted;
    return result.status;
  }
  std::vector<uint8_t> seen(vertex_count, 0);
  for (uint32_t vertex = 0; vertex < returned_prefix_size; ++vertex) {
    const int color = coloring.vertex_to_col[vertex];
    const int size = color >= 0 && color < static_cast<int>(vertex_count)
                         ? coloring.ptn[color] + 1
                         : 0;
    const int representative = size > 0 ? coloring.lab[color] : -1;
    if (representative < 0 ||
        representative >= static_cast<int>(vertex_count)) {
      release(result);
      result.status = Status::Failed;
      return result.status;
    }
    result.cells[vertex] = static_cast<uint32_t>(representative);
    result.sizes[vertex] = static_cast<uint32_t>(size);
    if (!seen[color]) {
      seen[color] = 1;
      ++result.cell_count;
    }
  }
  result.domain_size = returned_prefix_size;
  result.status = Status::Complete;
  return result.status;
}

Status prepare_refinement_branch(RefinementSession &session,
                                 RefinementBranch *&branch) noexcept {
  release(branch);
  try {
    branch = new RefinementBranch;
    branch->internal = session.internal;
    branch->vertex_count = session.vertex_count;
    branch->coloring.copy_any(&session.root);
    return Status::Complete;
  } catch (const std::bad_alloc &) {
    release(branch);
    return Status::ResourceExhausted;
  } catch (...) {
    release(branch);
    return Status::Failed;
  }
}

Status root_partition(RefinementSession &session,
                      uint32_t returned_prefix_size,
                      RefinementPartition &result) noexcept {
  release(result);
  if (returned_prefix_size > session.vertex_count) {
    result.status = Status::Invalid;
    return result.status;
  }
  if (returned_prefix_size) {
    result.cells = static_cast<uint32_t *>(
        std::malloc(static_cast<size_t>(returned_prefix_size) *
                    sizeof(uint32_t)));
    if (!result.cells) {
      result.status = Status::ResourceExhausted;
      return result.status;
    }
  }
  std::vector<uint8_t> seen(session.vertex_count, 0);
  for (uint32_t vertex = 0; vertex < returned_prefix_size; ++vertex) {
    const int color = session.root.vertex_to_col[vertex];
    if (color < 0 || color >= static_cast<int>(session.vertex_count)) {
      release(result);
      result.status = Status::Failed;
      return result.status;
    }
    const int representative = session.root.lab[color];
    if (representative < 0 ||
        representative >= static_cast<int>(session.vertex_count)) {
      release(result);
      result.status = Status::Failed;
      return result.status;
    }
    result.cells[vertex] = static_cast<uint32_t>(representative);
    if (!seen[color]) {
      seen[color] = 1;
      ++result.cell_count;
    }
  }
  result.domain_size = returned_prefix_size;
  result.status = Status::Complete;
  return result.status;
}

Status individualized_partition(RefinementBranch &branch, uint32_t anchor,
                                uint32_t returned_prefix_size,
                                uint64_t maximum_ticks, StopCallback stop,
                                void *stop_context,
                                IndividualizedPartition &result) noexcept {
  release(result);
  if (anchor >= returned_prefix_size ||
      returned_prefix_size > branch.vertex_count) {
    result.status = Status::Invalid;
    return result.status;
  }
  if (stopped(stop, stop_context)) {
    result.status = Status::Interrupted;
    return result.status;
  }
  try {
    uint64_t charged = 0;
    bool exhausted = false, interrupted = false;
    RefinementMeter meter(charged, maximum_ticks, stop, stop_context,
                          exhausted, interrupted);
    const auto worklist_hook =
        meter.worklist_hook(branch.internal, branch.coloring);
    const auto split_hook = meter.split_hook();
    const int color = branch.coloring.vertex_to_col[anchor];
    if (branch.coloring.ptn[color] != 0) {
      const int individualized =
          dejavu::ir::refinement::individualize_vertex(
              &branch.coloring, static_cast<int>(anchor));
      branch.refinement.refine_coloring(branch.internal, &branch.coloring,
                                        individualized, -1, &split_hook,
                                        &worklist_hook);
    }
    result.ticks = charged;
    if (interrupted) {
      result.status = Status::Interrupted;
      return result.status;
    }
    if (exhausted) {
      result.status = Status::Incomplete;
      return result.status;
    }
    return materialize_partition(branch.coloring, branch.vertex_count,
                                 returned_prefix_size, result);
  } catch (const std::bad_alloc &) {
    release(result);
    result.status = Status::ResourceExhausted;
    return result.status;
  } catch (...) {
    release(result);
    result.status = Status::Failed;
    return result.status;
  }
}

Status individualized_partition(RefinementSession &session,
                                const uint32_t *anchors,
                                uint32_t anchor_count,
                                uint32_t returned_prefix_size,
                                uint64_t maximum_ticks, StopCallback stop,
                                void *stop_context,
                                IndividualizedPartition &result) noexcept {
  release(result);
  if (!anchors || !anchor_count ||
      returned_prefix_size > session.vertex_count) {
    result.status = Status::Invalid;
    return result.status;
  }
  for (uint32_t i = 0; i < anchor_count; ++i)
    if (anchors[i] >= returned_prefix_size) {
      result.status = Status::Invalid;
      return result.status;
    }
  if (stopped(stop, stop_context)) {
    result.status = Status::Interrupted;
    return result.status;
  }

  try {
    dejavu::ds::coloring child;
    child.copy_any(&session.root);
    dejavu::ir::refinement refinement;
    uint64_t charged = 0;
    bool exhausted = false, interrupted = false;
    RefinementMeter meter(charged, maximum_ticks, stop, stop_context,
                          exhausted, interrupted);
    const auto worklist_hook = meter.worklist_hook(session.internal, child);
    const auto split_hook = meter.split_hook();
    for (uint32_t i = 0; i < anchor_count && !exhausted && !interrupted; ++i) {
      if (stopped(stop, stop_context)) {
        interrupted = true;
        break;
      }
      const int color = child.vertex_to_col[anchors[i]];
      if (child.ptn[color] == 0)
        continue;
      const int individualized =
          dejavu::ir::refinement::individualize_vertex(
              &child, static_cast<int>(anchors[i]));
      refinement.refine_coloring(session.internal, &child, individualized, -1,
                                 &split_hook, &worklist_hook);
    }
    result.ticks = charged;
    if (interrupted) {
      result.status = Status::Interrupted;
      return result.status;
    }
    if (exhausted) {
      result.status = Status::Incomplete;
      return result.status;
    }

    return materialize_partition(child, session.vertex_count,
                                 returned_prefix_size, result);
  } catch (const std::bad_alloc &) {
    release(result);
    result.status = Status::ResourceExhausted;
    return result.status;
  } catch (...) {
    release(result);
    result.status = Status::Failed;
    return result.status;
  }
}

Status individualized_partition(RefinementSession &session, uint32_t anchor,
                                uint32_t returned_prefix_size,
                                uint64_t maximum_ticks, StopCallback stop,
                                void *stop_context,
                                IndividualizedPartition &result) noexcept {
  return individualized_partition(session, &anchor, 1, returned_prefix_size,
                                  maximum_ticks, stop, stop_context, result);
}

Status reduce_orbit(RefinementSession &session, const uint32_t *vertices,
                    uint32_t count, uint32_t returned_prefix_size,
                    uint64_t maximum_ticks, StopCallback stop,
                    void *stop_context, RefinementOrbit &result) noexcept {
  release(result);
  session.first_refinement.reset();
  if (!vertices || count < 2 || returned_prefix_size > session.vertex_count ||
      vertices[0] >= returned_prefix_size) {
    result.status = Status::Invalid;
    return result.status;
  }
  const int root_color = session.root.vertex_to_col[vertices[0]];
  for (uint32_t i = 0; i < count; ++i)
    if (vertices[i] >= returned_prefix_size ||
        session.root.vertex_to_col[vertices[i]] != root_color) {
      result.status = Status::Invalid;
      return result.status;
    }
  if (stopped(stop, stop_context)) {
    result.status = Status::Interrupted;
    return result.status;
  }

  try {
    std::unique_ptr<FirstRefinement> captured(new FirstRefinement);
    std::unique_ptr<dejavu::ds::coloring> child_owner(new dejavu::ds::coloring);
    auto &child = *child_owner;
    child.copy_any(&session.root);
    const auto capture_split = [&](int, int color, int size) {
      if (captured) {
        const int vertex = child.lab[color];
        captured->events.push_back({0, session.root.vertex_to_col[vertex],
                                    size, vertex});
      }
      return true;
    };
    const int individualized =
        dejavu::ir::refinement::individualize_vertex(
            &child, static_cast<int>(vertices[0]), capture_split);
    dejavu::ir::refinement refinement;
    uint64_t charged = 0;
    bool exhausted = false, interrupted = false;
    RefinementMeter meter(charged, maximum_ticks, stop, stop_context,
                          exhausted, interrupted);
    const std::function<dejavu::ir::type_worklist_color_hook> worklist_hook =
        [&](int color, int size) {
          const uint64_t degree =
              static_cast<uint64_t>(session.internal->d[child.lab[color]]);
          const uint64_t width = static_cast<uint64_t>(size);
          return meter.charge(degree, width, [&](uint64_t work) {
            if (captured)
              captured->events.push_back({work, -1, 0, -1});
          });
        };
    const std::function<dejavu::ir::type_split_color_hook> split_hook =
        [&](int old_color, int color, int size) {
          capture_split(old_color, color, size);
          if (stopped(stop, stop_context)) {
            interrupted = true;
            return false;
          }
          return !exhausted;
        };
    refinement.refine_coloring(session.internal, &child, individualized, -1,
                               &split_hook, &worklist_hook);
    result.ticks = charged;
    if (interrupted) {
      result.status = Status::Interrupted;
      return result.status;
    }
    if (exhausted) {
      result.status = Status::Incomplete;
      return result.status;
    }

    int selected_color = -1;
    uint32_t selected_size = 0;
    for (uint32_t i = 1; i < count; ++i) {
      const int color = child.vertex_to_col[vertices[i]];
      const uint32_t size = static_cast<uint32_t>(child.ptn[color] + 1);
      if (size < count - 1 && size > selected_size) {
        selected_color = color;
        selected_size = size;
      }
    }
    result.count = selected_color < 0 ? count : selected_size + 1;
    result.vertices = static_cast<uint32_t *>(
        std::malloc(static_cast<size_t>(result.count) * sizeof(uint32_t)));
    if (!result.vertices) {
      result.count = 0;
      result.status = Status::ResourceExhausted;
      return result.status;
    }
    if (selected_color < 0) {
      std::copy(vertices, vertices + count, result.vertices);
    } else {
      result.vertices[0] = vertices[0];
      for (uint32_t i = 0; i < selected_size; ++i) {
        const int vertex = child.lab[selected_color + static_cast<int>(i)];
        if (vertex < 0 || vertex >= static_cast<int>(returned_prefix_size)) {
          release(result);
          result.status = Status::Failed;
          return result.status;
        }
        result.vertices[i + 1] = static_cast<uint32_t>(vertex);
      }
    }
    if (captured) {
      captured->child = std::move(child_owner);
      captured->anchor = vertices[0];
      captured->ticks = charged;
      session.first_refinement = std::move(captured);
    }
    result.status = Status::Complete;
    return result.status;
  } catch (const std::bad_alloc &) {
    release(result);
    result.status = Status::ResourceExhausted;
    return result.status;
  } catch (...) {
    release(result);
    result.status = Status::Failed;
    return result.status;
  }
}

Status probe_base_length(RefinementSession &session, uint64_t maximum_ticks,
                         StopCallback stop, void *stop_context,
                         uint32_t &length, uint64_t &ticks) noexcept {
  length = 0;
  ticks = 0;
  if (stopped(stop, stop_context))
    return Status::Interrupted;
  try {
    dejavu::ds::coloring child;
    child.copy_any(&session.root);
    dejavu::ir::refinement refinement;
    bool exhausted = false, interrupted = false;
    RefinementMeter meter(ticks, maximum_ticks, stop, stop_context, exhausted,
                          interrupted);
    const auto worklist_hook = meter.worklist_hook(session.internal, child);
    const auto split_hook = meter.split_hook();
    for (uint32_t vertex = 0; vertex < session.vertex_count; ++vertex) {
      if (child.cells >= static_cast<int>(session.vertex_count))
        break;
      const int color = child.vertex_to_col[vertex];
      if (!child.ptn[color])
        continue;
      const int individualized =
          dejavu::ir::refinement::individualize_vertex(
              &child, static_cast<int>(vertex));
      ++length;
      refinement.refine_coloring(session.internal, &child, individualized, -1,
                                 &split_hook, &worklist_hook);
      if (interrupted)
        return Status::Interrupted;
      if (exhausted)
        return Status::Incomplete;
    }
    return Status::Complete;
  } catch (const std::bad_alloc &) {
    return Status::ResourceExhausted;
  } catch (...) {
    return Status::Failed;
  }
}

Status prepare_refinement(const Graph &input, uint64_t maximum_ticks,
                          StopCallback stop, void *stop_context,
                          RefinementSession *&session,
                          uint64_t &ticks) noexcept {
  release(session);
  ticks = 0;
  if (!input.vertex_count || !input.colors || !input.degrees ||
      (input.edge_count && !input.edges) ||
      input.vertex_count >
          static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      input.edge_count >
          static_cast<uint64_t>(std::numeric_limits<int>::max()))
    return Status::Invalid;
  if (stopped(stop, stop_context))
    return Status::Interrupted;

  try {
    auto prepared = std::make_unique<RefinementSession>();
    prepared->graph = std::make_unique<dejavu::static_graph>(
        static_cast<int>(input.vertex_count), static_cast<int>(input.edge_count));
    prepared->vertex_count = input.vertex_count;
    std::vector<int> initial_colors(input.vertex_count);
    for (uint32_t vertex = 0; vertex < input.vertex_count; ++vertex) {
      if (!(vertex & 1023u) && stopped(stop, stop_context))
        return Status::Interrupted;
      if (input.colors[vertex] >
              static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
          input.degrees[vertex] >
              static_cast<uint32_t>(std::numeric_limits<int>::max()))
        return Status::Invalid;
      initial_colors[vertex] = static_cast<int>(input.colors[vertex]);
      prepared->graph->add_vertex(initial_colors[vertex],
                                  static_cast<int>(input.degrees[vertex]));
    }
    for (uint64_t edge = 0; edge < input.edge_count; ++edge) {
      if (!(edge & 1023u) && stopped(stop, stop_context))
        return Status::Interrupted;
      const uint32_t first = input.edges[2 * edge];
      const uint32_t second = input.edges[2 * edge + 1];
      if (first >= input.vertex_count || second >= input.vertex_count ||
          first >= second)
        return Status::Invalid;
      prepared->graph->add_edge(first, second);
    }

    prepared->internal = prepared->graph->get_sgraph();
    switch (graph_shape(*prepared->internal, stop, stop_context)) {
    case GraphShape::Simple:
      break;
    case GraphShape::RepeatedEdge:
      return Status::Invalid;
    case GraphShape::Stopped:
      return Status::Interrupted;
    }
    prepared->internal->initialize_coloring(&prepared->root,
                                             initial_colors.data());
    dejavu::ir::refinement refinement;
    uint64_t charged = 0;
    bool exhausted = false, interrupted = false;
    RefinementMeter meter(charged, maximum_ticks, stop, stop_context,
                          exhausted, interrupted);
    const auto worklist_hook =
        meter.worklist_hook(prepared->internal, prepared->root);
    const auto split_hook = meter.split_hook();
    refinement.refine_coloring(prepared->internal, &prepared->root, -1, -1,
                               &split_hook, &worklist_hook);
    ticks = charged;
    if (interrupted)
      return Status::Interrupted;
    if (exhausted)
      return Status::Incomplete;
    session = prepared.release();
    return Status::Complete;
  } catch (const std::bad_alloc &) {
    return Status::ResourceExhausted;
  } catch (...) {
    return Status::Failed;
  }
}

Status refine_rows(RefinementSession &session, const uint32_t *anchors,
                   uint32_t anchor_count, uint32_t returned_prefix_size,
                   uint64_t maximum_ticks, StopCallback stop,
                   void *stop_context, RefinementRows &result,
                   RowFeedbackCallback feedback,
                   void *feedback_context) noexcept {
  release(result);
  if (!anchors || anchor_count < 2 ||
      returned_prefix_size > session.vertex_count ||
      (returned_prefix_size &&
       anchor_count > UINT64_MAX / returned_prefix_size)) {
    result.status = Status::Invalid;
    return result.status;
  }
  for (uint32_t row = 0; row < anchor_count; ++row)
    if (anchors[row] >= returned_prefix_size) {
      result.status = Status::Invalid;
      return result.status;
    }
  if (stopped(stop, stop_context)) {
    result.status = Status::Interrupted;
    return result.status;
  }

  try {
    const int anchor_color = session.root.vertex_to_col[anchors[0]];
    for (uint32_t row = 1; row < anchor_count; ++row)
      if (session.root.vertex_to_col[anchors[row]] != anchor_color) {
        result.status = Status::Complete;
        return result.status;
      }
    if (!session.root.ptn[anchor_color]) {
      result.status = Status::Complete;
      return result.status;
    }

    const uint64_t cells =
        static_cast<uint64_t>(anchor_count) * returned_prefix_size;
    if (cells > SIZE_MAX / sizeof(uint32_t)) {
      result.status = Status::ResourceExhausted;
      return result.status;
    }
    result.images = static_cast<uint32_t *>(
        std::malloc(static_cast<size_t>(cells) * sizeof(uint32_t)));
    if (cells && !result.images) {
      result.status = Status::ResourceExhausted;
      return result.status;
    }
    std::fill(result.images, result.images + cells, UINT32_MAX);

    dejavu::ir::refinement refinement;
    auto first_refinement = std::move(session.first_refinement);
    uint64_t charged = 0;
    bool exhausted = false, interrupted = false;
    struct FeedbackMeter {
      uint64_t &charged;
      uint64_t maximum;
      StopCallback stop;
      void *context;
      bool &exhausted;
      bool &interrupted;
    } feedback_meter{charged, maximum_ticks, stop, stop_context,
                     exhausted, interrupted};
    const RowWorkCallback charge_feedback = [](void *opaque) {
      auto &meter = *static_cast<FeedbackMeter *>(opaque);
      if (stopped(meter.stop, meter.context)) {
        meter.interrupted = true;
        return false;
      }
      if (meter.exhausted || meter.interrupted)
        return false;
      if (meter.charged >= meter.maximum) {
        meter.exhausted = true;
        return false;
      }
      ++meter.charged;
      return true;
    };
    // DejaVu must report every split through its split callback.
    // Only root cells split by this individualization can provide new
    // singleton images or an equal remainder block. Preserve ascending cell
    // order while omitting unchanged root cells from result extraction.
    std::vector<uint8_t> touched_root(session.vertex_count, 0);
    std::vector<int> touched_roots;
    dejavu::ds::coloring *current = &session.root;
    std::vector<uint32_t> *current_singletons = nullptr;
    RefinementMeter meter(charged, maximum_ticks, stop, stop_context,
                          exhausted, interrupted);
    const auto worklist_hook = meter.worklist_hook(session.internal, current);
    const std::function<dejavu::ir::type_split_color_hook> split_hook =
        [&](int, int new_color, int new_size) {
          if (stopped(stop, stop_context)) {
            interrupted = true;
            return false;
          }
          const int root_color =
              session.root.vertex_to_col[current->lab[new_color]];
          if (!touched_root[root_color]) {
            touched_root[root_color] = 1;
            touched_roots.push_back(root_color);
          }
          if (current_singletons && new_size == 1) {
            const int vertex = current->lab[new_color];
            if (vertex >= 0 &&
                vertex < static_cast<int>(returned_prefix_size) &&
                session.root.ptn[session.root.vertex_to_col[vertex]] > 0)
              current_singletons->push_back(static_cast<uint32_t>(vertex));
          }
          return !exhausted;
        };

    std::vector<uint32_t> reference_cells(session.vertex_count, UINT32_MAX);
    std::vector<uint32_t> reference_order;
    std::unordered_map<int, std::vector<uint32_t>> reference_blocks;
    const bool reusable_first = first_refinement &&
        first_refinement->anchor == anchors[0] &&
        maximum_ticks > first_refinement->ticks;
    auto child_owner = reusable_first ? std::move(first_refinement->child) :
        std::unique_ptr<dejavu::ds::coloring>(new dejavu::ds::coloring);
    auto &child = *child_owner;
    for (uint32_t row = 0; row < anchor_count; ++row) {
      for (const int color : touched_roots) touched_root[color] = 0;
      touched_roots.clear();
      if (row || !reusable_first)
        child.copy_any(&session.root);
      current = &child;
      std::vector<uint32_t> discovered_singletons;
      current_singletons = &discovered_singletons;
      const bool reuse_first = !row && reusable_first;
      if (reuse_first) {
        // Each event polls exactly where the completed native first row did.
        // A tight work allowance falls back to native execution, preserving
        // bounded partial outputs without inferring an intermediate coloring.
        for (const auto &event : first_refinement->events) {
          if (stopped(stop, stop_context)) {
            interrupted = true;
            break;
          }
          if (event.work) {
            charged += event.work;
          } else {
            if (!touched_root[event.root_color]) {
              touched_root[event.root_color] = 1;
              touched_roots.push_back(event.root_color);
            }
            if (event.size == 1 && event.vertex >= 0 &&
                event.vertex < static_cast<int>(returned_prefix_size) &&
                session.root.ptn[event.root_color] > 0)
              discovered_singletons.push_back(
                  static_cast<uint32_t>(event.vertex));
          }
        }
      } else {
        const int individualized =
            dejavu::ir::refinement::individualize_vertex(
                &child, static_cast<int>(anchors[row]), split_hook);
        refinement.refine_coloring(session.internal, &child, individualized, -1,
                                   &split_hook, &worklist_hook);
      }
      first_refinement.reset();
      current_singletons = nullptr;
      if (interrupted) {
        release(result);
        result.status = Status::Interrupted;
        return result.status;
      }
      if (exhausted) {
        result.anchor_count = anchor_count;
        result.domain_size = returned_prefix_size;
        result.ticks = charged;
        result.status = Status::Incomplete;
        return result.status;
      }
      std::sort(touched_roots.begin(), touched_roots.end());
      if (!row)
        reference_order = discovered_singletons;
      // Use stable child color positions to align the full singleton image.
      // The hook's discovery trace is retained separately as the preferred
      // semantic column order; it is not itself an image certificate.
      for (const int root_color : touched_roots) {
       const int root_end = root_color + session.root.ptn[root_color] + 1;
       for (int color = root_color; color < root_end;) {
        const int size = child.ptn[color] + 1;
        const int first = child.lab[color];
        const int original_color = session.root.vertex_to_col[first];
        const bool new_singleton =
            size == 1 && session.root.ptn[original_color] > 0;
        if (new_singleton && first < static_cast<int>(returned_prefix_size)) {
          if (!row)
            reference_cells[color] = static_cast<uint32_t>(first);
          const uint32_t source = reference_cells[color];
          if (source != UINT32_MAX)
            result.images[static_cast<uint64_t>(row) * returned_prefix_size +
                          source] = static_cast<uint32_t>(first);
        }
        color += size;
       }
      }

      // Satsuma's row detector also retains one equal remainder block from a
      // touched root cell when that cell partitions evenly across the anchor
      // orbit. These blocks contain the columns which do not become
      // singletons under one individualization. Align a canonical member
      // order here; the caller still verifies every completed row
      // transposition against the complete formula before applying a rule.
      for (const int root_color : touched_roots) {
        const int root_size = session.root.ptn[root_color] + 1;
        bool selected = false;
        for (int color = root_color;
             color < root_color + root_size && !selected;) {
          const int size = child.ptn[color] + 1;
          const bool equal_remainder =
              size > 1 &&
              static_cast<uint64_t>(size) * anchor_count ==
                  static_cast<uint64_t>(root_size);
          if (equal_remainder) {
            std::vector<uint32_t> members;
            members.reserve(size);
            bool inside_prefix = true;
            for (int offset = 0; offset < size; ++offset) {
              const int vertex = child.lab[color + offset];
              inside_prefix &=
                  vertex < static_cast<int>(returned_prefix_size);
              members.push_back(static_cast<uint32_t>(vertex));
            }
            if (inside_prefix) {
              std::sort(members.begin(), members.end());
              if (!row) {
                reference_blocks[root_color] = members;
                reference_order.insert(reference_order.end(), members.begin(),
                                       members.end());
              }
              const std::vector<uint32_t> &sources =
                  reference_blocks[root_color];
              if (sources.size() == members.size())
                for (size_t offset = 0; offset < sources.size(); ++offset)
                  result.images[static_cast<uint64_t>(row) *
                                    returned_prefix_size +
                                sources[offset]] = members[offset];
              selected = true;
            }
          }
          color += size;
        }
      }
      if (feedback) {
        const RefinementRowPrefix prefix{row + 1, anchor_count,
                                         returned_prefix_size, result.images};
        const bool keep_going =
            feedback(prefix, charge_feedback, &feedback_meter, feedback_context);
        if (interrupted || exhausted || !keep_going) {
          release(result);
          result.ticks = charged;
          result.status = interrupted ? Status::Interrupted :
              exhausted ? Status::Incomplete : Status::Complete;
          return result.status;
        }
      }
    }
    std::vector<uint8_t> ordered(returned_prefix_size, 0);
    reference_order.erase(
        std::remove_if(reference_order.begin(), reference_order.end(),
                       [&](uint32_t source) {
                         if (source >= returned_prefix_size || ordered[source])
                           return true;
                         ordered[source] = 1;
                         return false;
                       }),
        reference_order.end());
    for (uint32_t source = 0; source < returned_prefix_size; ++source) {
      bool complete = !ordered[source];
      for (uint32_t row = 0; row < anchor_count && complete; ++row)
        complete &= result.images[static_cast<uint64_t>(row) *
                                      returned_prefix_size +
                                  source] != UINT32_MAX;
      if (complete) {
        ordered[source] = 1;
        reference_order.push_back(source);
      }
    }
    result.anchor_count = anchor_count;
    result.domain_size = returned_prefix_size;
    result.ticks = charged;
    result.source_count = static_cast<uint32_t>(reference_order.size());
    if (result.source_count) {
      result.sources = static_cast<uint32_t *>(
          std::malloc(static_cast<size_t>(result.source_count) *
                      sizeof(uint32_t)));
      if (!result.sources) {
        release(result);
        result.status = Status::ResourceExhausted;
        return result.status;
      }
      std::copy(reference_order.begin(), reference_order.end(),
                result.sources);
    }
    result.status = Status::Complete;
    return result.status;
  } catch (const std::bad_alloc &) {
    release(result);
    result.status = Status::ResourceExhausted;
    return result.status;
  } catch (...) {
    release(result);
    result.status = Status::Failed;
    return result.status;
  }
}

Status refine_rows(RefinementSession &session, const uint32_t *anchors,
                   uint32_t anchor_count, uint32_t returned_prefix_size,
                   uint64_t maximum_ticks, StopCallback stop,
                   void *stop_context, RefinementRows &result) noexcept {
  return refine_rows(session, anchors, anchor_count, returned_prefix_size,
                     maximum_ticks, stop, stop_context, result, nullptr, nullptr);
}

Status refine_rows(const Graph &input, const uint32_t *anchors,
                   uint32_t anchor_count, uint32_t returned_prefix_size,
                   uint64_t maximum_ticks, StopCallback stop,
                   void *stop_context, RefinementRows &result) noexcept {
  release(result);
  RefinementSession *session = nullptr;
  uint64_t preparation_ticks = 0;
  const Status prepared = prepare_refinement(input, maximum_ticks, stop,
                                             stop_context, session,
                                             preparation_ticks);
  if (prepared != Status::Complete) {
    result.status = prepared;
    result.ticks = preparation_ticks;
    return result.status;
  }
  const uint64_t remaining = maximum_ticks - preparation_ticks;
  const Status status = refine_rows(*session, anchors, anchor_count,
                                    returned_prefix_size, remaining, stop,
                                    stop_context, result);
  release(session);
  result.ticks = result.ticks > UINT64_MAX - preparation_ticks
                     ? UINT64_MAX
                     : result.ticks + preparation_ticks;
  return status;
}

Status fix_clauses(const GroupInput &input, const GroupSettings &settings,
                   StopCallback stop, void *stop_context,
                   Fixings &result) noexcept {
  release(result);
  const SymmetryTrace trace;
  trace.event("group_begin",
              "domain\t%u\tgenerators\t%llu\tclauses\t%llu\tmode\t%u"
              "\tbase_selection\t%u\tinstance_limit\t%llu\ttick_limit\t%llu",
              input.domain_size, static_cast<unsigned long long>(input.generator_count),
              static_cast<unsigned long long>(input.clause_count),
              static_cast<unsigned>(settings.chain_mode),
              static_cast<unsigned>(settings.base_selection),
              static_cast<unsigned long long>(settings.maximum_instances),
              static_cast<unsigned long long>(settings.maximum_ticks));
  if (!input.domain_size ||
      input.domain_size > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      !settings.error_bound ||
      settings.error_bound > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      settings.chain_mode > 2 || settings.base_selection > 6 ||
      settings.cut_point > 1 ||
      (settings.chain_mode && (input.domain_size & 1u)) ||
      (input.generator_count && !input.generator_images && !input.generator_offsets) ||
      (input.clause_count && !input.clause_offsets) ||
      (input.literal_count && !input.literals) || !input.values ||
      (input.clause_offsets && input.clause_offsets[0] != 0) ||
      (input.clause_offsets &&
       input.clause_offsets[input.clause_count] != input.literal_count)) {
    result.status = Status::Invalid;
    return result.status;
  }
  if (stopped(stop, stop_context)) {
    result.status = Status::Interrupted;
    return result.status;
  }

  try {
    const auto poll = [&]() {
      if (stopped(stop, stop_context))
        throw dejavu::termination_requested();
    };
    if (!input.generator_offsets &&
        (input.generator_count > SIZE_MAX / input.domain_size ||
         input.generator_count * input.domain_size > SIZE_MAX / sizeof(uint32_t))) {
      result.status = Status::Invalid;
      return result.status;
    }
    std::vector<int8_t> values(input.values,
                               input.values + input.domain_size);
    for (uint32_t point = 0; point < input.domain_size; ++point)
      if (values[point] < -1 || values[point] > 1) {
        result.status = Status::Invalid;
        return result.status;
      }
    for (uint64_t clause = 0; clause < input.clause_count; ++clause)
      if (input.clause_offsets[clause] > input.clause_offsets[clause + 1] ||
          input.clause_offsets[clause + 1] > input.literal_count) {
        result.status = Status::Invalid;
        return result.status;
      }
    for (uint64_t at = 0; at < input.literal_count; ++at)
      if (input.literals[at] >= input.domain_size) {
        result.status = Status::Invalid;
        return result.status;
      }
    if (!detail::validate_generator_rows(input, poll)) {
      result.status = Status::Invalid;
      return result.status;
    }

    if (settings.factor_support_components)
      return detail::fix_support_components(input, settings, stop, stop_context,
                                            result);

    uint64_t charged = 0;
    const auto charge = [&](uint64_t amount) {
      if (charged >= settings.maximum_ticks ||
          amount > settings.maximum_ticks - charged) {
        charged = settings.maximum_ticks;
        return false;
      }
      charged += amount;
      return true;
    };

    std::vector<int> base;
    dejavu::groups::random_schreier schreier(
        static_cast<int>(input.domain_size),
        static_cast<int>(settings.error_bound), false,
        std::bit_cast<int32_t>(settings.seed));
    schreier.set_base(base);
    dejavu::groups::automorphism_workspace generator(input.domain_size);
    for (uint64_t row = 0; row < input.generator_count; ++row) {
      poll();
      generator.reset();
      detail::visit_generator_moves(input, row, poll, [&](uint32_t point, uint32_t image) {
        generator.write_single_map(static_cast<int>(point), static_cast<int>(image));
      });
      schreier.sift(generator);
      poll();
    }
    // All later generators are words in the input subgroup. Its exact orbit
    // partition therefore bounds every pointwise-stabilizer transversal.
    // A partial stabilizer orbit would not be a sound upper bound here.
    std::vector<int> point_bounds;
    if (settings.chain_mode == 2) {
      dejavu::groups::orbit initial_orbits(input.domain_size);
      for (uint64_t row = 0; row < input.generator_count; ++row) {
        poll();
        // Preserve the historical domain-sized charge. Identity unions are
        // exact no-ops in DejaVu; only their representation work is skipped.
        if (!charge(input.domain_size)) {
          result.ticks = charged;
          result.status = Status::Incomplete;
          return result.status;
        }
        detail::visit_generator_moves(input, row, poll, [&](uint32_t point, uint32_t image) {
          initial_orbits.combine_orbits(static_cast<int>(point), static_cast<int>(image));
        });
      }
      point_bounds.reserve(input.domain_size);
      for (uint32_t point = 0; point < input.domain_size; ++point) {
        if (!(point & 1023u))
          poll();
        if (!charge(1)) {
          result.ticks = charged;
          result.status = Status::Incomplete;
          return result.status;
        }
        point_bounds.push_back(initial_orbits.orbit_size(static_cast<int>(point)));
      }
    }
    uint64_t accounted_cost = schreier.get_computational_cost();
    bool exhausted = !charge(accounted_cost);
    bool failed = false;
    bool planning_limited = false;

    std::vector<uint8_t> kinds;
    std::vector<uint32_t> levels;
    std::vector<uint32_t> representatives;
    std::vector<uint32_t> others;
    std::vector<std::vector<uint32_t>> permutations;
    dejavu::groups::orbit orbits(input.domain_size);
    dejavu::groups::automorphism_workspace transversal(input.domain_size);
    std::vector<uint32_t> active;
    const auto inverse_transversal = [&](uint32_t level,
                                         uint32_t representative, uint32_t other,
                                         std::vector<uint32_t> &inverse) {
      transversal.reset();
      if (!schreier.get_transversal_element(
              static_cast<int>(level), static_cast<int>(other),
              transversal)) {
        trace.event("transversal_failure",
                    "level\t%u\ttarget\t%u\treason\tmissing", level, other);
        return false;
      }
      inverse.assign(input.domain_size, UINT32_MAX);
      for (uint32_t point = 0; point < input.domain_size; ++point) {
        if (!(point & 1023u))
          poll();
        const int target = transversal[point];
        if (target < 0 || static_cast<uint32_t>(target) >= input.domain_size ||
            inverse[static_cast<uint32_t>(target)] != UINT32_MAX) {
          trace.event("transversal_failure",
                      "level\t%u\ttarget\t%u\treason\tpermutation", level, other);
          return false;
        }
        inverse[static_cast<uint32_t>(target)] = point;
      }
      if (inverse[representative] != other)
        trace.event("transversal_failure",
                    "level\t%u\ttarget\t%u\treason\tdirection\timage\t%u",
                    level, other, inverse[representative]);
      return inverse[representative] == other;
    };
    const auto eligible_clause_candidate =
        [&](uint64_t clause, std::vector<uint32_t> &candidate) {
          candidate.clear();
          bool satisfied = false;
          for (uint64_t at = input.clause_offsets[clause];
               at < input.clause_offsets[clause + 1]; ++at) {
            if (!charge(1)) {
              exhausted = true;
              break;
            }
            const uint32_t literal = input.literals[at];
            if (values[literal] > 0) {
              satisfied = true;
              break;
            }
            if (!values[literal])
              candidate.push_back(literal);
          }
          if (exhausted)
            return false;
          if (settings.first_literal_fix_anchor && !satisfied &&
              values[input.literals[input.clause_offsets[clause]]] < 0)
            return false;
          if (satisfied || candidate.size() < 2 ||
              orbits.orbit_size(static_cast<int>(candidate.front())) == 1)
            return false;
          bool common_orbit = true;
          for (const uint32_t literal : candidate)
            common_orbit &= orbits.are_in_same_orbit(
                static_cast<int>(candidate.front()), static_cast<int>(literal));
          return common_orbit;
        };

    if (!settings.chain_mode) {
      while (!exhausted &&
             !schreier.get_stabilizer_generators(base.size()).empty()) {
        poll();
        if (!charge(1)) {
          exhausted = true;
          break;
        }
        schreier.get_stabilizer_orbit(static_cast<int>(base.size()), orbits);

        uint64_t selected_clause = UINT64_MAX;
        active.clear();
        for (uint64_t clause = 0; clause < input.clause_count; ++clause) {
          std::vector<uint32_t> candidate;
          if (!eligible_clause_candidate(clause, candidate)) {
            if (exhausted)
              break;
            continue;
          }
          selected_clause = clause;
          active = std::move(candidate);
          break;
        }
        if (exhausted || selected_clause == UINT64_MAX)
          break;

        const uint64_t required_actions = active.size();
        trace.event("clausal_select",
                    "depth\t%zu\tclause\t%llu\tactive\t%zu\tpoint\t%u"
                    "\torbit\t%d\tamo\t%u\tstaged\t%zu",
                    base.size(), static_cast<unsigned long long>(selected_clause),
                    active.size(), active.front(),
                    trace.enabled()
                        ? orbits.orbit_size(static_cast<int>(active.front())) : 0,
                    static_cast<unsigned>(input.clause_is_amo &&
                                          input.clause_is_amo[selected_clause]),
                    kinds.size());
        if (trace.enabled()) {
          trace.event("clausal_state", "depth\t%zu\tstored_generators\t%d",
                      base.size(), schreier.get_number_of_generators());
          // One bounded record per literal preserves the complete clause,
          // including literals removed from its active projection. This is
          // diagnostic only; do not consult it for selection or work charging.
          for (uint64_t at = input.clause_offsets[selected_clause];
               at < input.clause_offsets[selected_clause + 1]; ++at) {
            const uint32_t point = input.literals[at];
            trace.event("clausal_literal",
                        "depth\t%zu\tclause\t%llu\tposition\t%llu"
                        "\tpoint\t%u\tvalue\t%d\torbit\t%d",
                        base.size(),
                        static_cast<unsigned long long>(selected_clause),
                        static_cast<unsigned long long>(
                            at - input.clause_offsets[selected_clause]),
                        point, static_cast<int>(values[point]),
                        orbits.orbit_size(static_cast<int>(point)));
          }
        }
        if (kinds.size() > settings.maximum_instances ||
            required_actions > settings.maximum_instances - kinds.size() ||
            !charge(required_actions)) {
          exhausted = true;
          break;
        }

        const uint32_t representative = active.front();
        base.push_back(static_cast<int>(representative));
        // The selected clause uses the orbit of the retained stabilizer
        // generators. Extending the base clears that retained list unless it
        // is re-sifted; probabilistic sampling alone may then omit a known
        // target, especially beside a much larger independent component.
        // Preserve the generators that justified selecting this whole level.
        schreier.set_base(base, true);
        schreier.sift_random();
        poll();
        const uint64_t current_cost = schreier.get_computational_cost();
        if (current_cost < accounted_cost ||
            !charge(current_cost - accounted_cost)) {
          exhausted = true;
          break;
        }
        accounted_cost = current_cost;

        // A single re-sift need not close an orbit under alternating words.
        // Every selected target was already in the retained subgroup's orbit;
        // finish that known orbit deterministically before reading witnesses.
        const uint32_t level = static_cast<uint32_t>(base.size() - 1);
        const bool missing_target = std::any_of(
            active.begin(), active.end(), [&](uint32_t point) {
              return !schreier.is_in_fixed_orbit(static_cast<int>(level),
                                                static_cast<int>(point));
            });
        if (missing_target) {
          trace.event("clausal_orbit_closure", "boundary\tbegin\tlevel\t%u", level);
          schreier.extend();
          trace.event("clausal_orbit_closure", "boundary\tend\tlevel\t%u", level);
          poll();
          const uint64_t closure_cost = schreier.get_computational_cost();
          if (closure_cost < accounted_cost ||
              !charge(closure_cost - accounted_cost)) {
            exhausted = true;
            break;
          }
          accounted_cost = closure_cost;
        }

        std::vector<std::vector<uint32_t>> level_images;
        level_images.reserve(active.size());
        trace.event("transversals_begin", "depth\t%zu\ttargets\t%zu",
                    base.size() - 1, active.size() - 1);
        for (size_t i = 1; i < active.size(); ++i) {
          poll();
          std::vector<uint32_t> inverse;
          if (!inverse_transversal(static_cast<uint32_t>(base.size() - 1),
                                   representative, active[i], inverse)) {
            failed = true;
            break;
          }
          level_images.push_back(std::move(inverse));
        }
        if (failed)
          break;

        // Witness construction is group work too. Charge the complete staged
        // level before publishing any of it, as in the fix-and-cut path.
        const uint64_t witness_cost = schreier.get_computational_cost();
        if (witness_cost < accounted_cost ||
            !charge(witness_cost - accounted_cost)) {
          exhausted = true;
          break;
        }
        accounted_cost = witness_cost;

        trace.event("transversals_end", "depth\t%zu\ttargets\t%zu",
                    base.size() - 1, level_images.size());

        for (size_t i = 1; i < active.size(); ++i) {
          kinds.push_back(0);
          levels.push_back(level);
          representatives.push_back(representative);
          others.push_back(active[i]);
          permutations.push_back(std::move(level_images[i - 1]));
        }
        kinds.push_back(1);
        levels.push_back(level);
        representatives.push_back(representative);
        others.push_back(UINT32_MAX);
        std::vector<uint32_t> identity(input.domain_size);
        for (uint32_t point = 0; point < input.domain_size; ++point)
          identity[point] = point;
        permutations.push_back(std::move(identity));

        values[representative] = 1;
        values[representative ^ 1u] = -1;
        if (input.clause_is_amo && input.clause_is_amo[selected_clause])
          for (size_t i = 1; i < active.size(); ++i) {
            values[active[i]] = -1;
            values[active[i] ^ 1u] = 1;
          }
        orbits.reset();
      }
    } else {
      enum class PlannedOperation : uint8_t { Fix, Cut, None };
      struct PlannedLevel {
        PlannedOperation operation = PlannedOperation::None;
        uint32_t representative = UINT32_MAX;
        std::vector<uint32_t> active;
        bool amo = false;
      };
      std::vector<PlannedLevel> planned;
      const bool first_shortest = settings.base_selection == 6;
      const bool first_amo = settings.base_selection >= 2 && settings.base_selection <= 4;
      const bool ordered_remainder = settings.base_selection == 3 || settings.base_selection == 4;
      bool amo_exhausted = false;
      bool negation_fixing = settings.negation_fixing_first;
      uint64_t clause_frontier = 0;
      const uint32_t variable_count = (input.domain_size + 1u) / 2u;
      std::vector<uint8_t> ordered(variable_count, 0);
      std::vector<uint8_t> marked_clauses(input.clause_count, 0);
      std::vector<std::vector<uint64_t>> variable_clauses;
      if (settings.base_selection) {
        variable_clauses.resize(variable_count);
        for (uint64_t clause = 0; clause < input.clause_count; ++clause) {
          std::vector<uint32_t> variables;
          for (uint64_t at = input.clause_offsets[clause];
               at < input.clause_offsets[clause + 1]; ++at)
            variables.push_back(input.literals[at] >> 1);
          std::sort(variables.begin(), variables.end());
          variables.erase(std::unique(variables.begin(), variables.end()),
                          variables.end());
          for (const uint32_t variable : variables)
            variable_clauses[variable].push_back(clause);
        }
      }

      while (!exhausted &&
             !schreier.get_stabilizer_generators(base.size()).empty()) {
        poll();
        if (settings.planning_tick_limit &&
            charged >= settings.planning_tick_limit) {
          planning_limited = true;
          trace.event("chain_planning_stop", "levels\t%zu\tticks\t%llu",
                      base.size(), static_cast<unsigned long long>(charged));
          break;
        }
        if (!charge(1)) {
          exhausted = true;
          break;
        }
        schreier.get_stabilizer_orbit(static_cast<int>(base.size()), orbits);

        // The owner fixes negation orbits over the whole chain before its
        // fix-and-cut pass sees the group (negation_fixing_schreier).
        uint32_t negation_point = UINT32_MAX;
        if (negation_fixing) {
          // The owner rescans from the first variable at every level: a
          // sifted stabilizer orbit is a lower bound that later sifting can
          // enlarge. Each probe is charged, so the pass stays inside the
          // planner's allowance.
          for (uint32_t point = 0; point < input.domain_size; point += 2) {
            if (!charge(1)) {
              exhausted = true;
              break;
            }
            if (ordered[point >> 1] || values[point] || values[point ^ 1u])
              continue;
            if (orbits.are_in_same_orbit(static_cast<int>(point),
                                         static_cast<int>(point ^ 1u))) {
              // The owner's unit is the variable's negative literal; it is
              // the level's base point, so the published unit sets it true.
              negation_point = point ^ 1u;
              break;
            }
          }
          if (exhausted)
            break;
          negation_fixing = negation_point != UINT32_MAX;
        }
        const bool shortest_first = settings.base_selection == 5 ||
                                    (first_shortest && base.empty());
        const bool rank_common = settings.base_selection &&
            !(ordered_remainder && amo_exhausted) &&
            !(first_shortest && !base.empty());
        const bool forward_scan = settings.base_selection == 4 ||
                                  (first_shortest && !base.empty());
        std::vector<uint32_t> common_score(variable_count, 0);
        if (rank_common) {
          for (uint32_t variable = 0; variable < variable_count; ++variable)
            for (const uint64_t clause : variable_clauses[variable]) {
              if (!charge(1)) {
                exhausted = true;
                break;
              }
              common_score[variable] += marked_clauses[clause] != 0;
            }
        }
        if (exhausted)
          break;

        uint64_t selected_clause = UINT64_MAX;
        uint32_t representative = UINT32_MAX;
        std::vector<uint32_t> selected_active;
        bool selected_amo = false;
        uint32_t selected_score = 0;
        int selected_orbit_size = std::numeric_limits<int>::max();
        const uint64_t scan_begin = negation_point != UINT32_MAX
                                        ? input.clause_count
                                    : forward_scan ? clause_frontier : 0;
        bool frontier_found = false;
        for (uint64_t clause = scan_begin; clause < input.clause_count; ++clause) {
          std::vector<uint32_t> candidate;
          if (!eligible_clause_candidate(clause, candidate)) {
            if (exhausted)
              break;
            continue;
          }

          if (forward_scan && !frontier_found) {
            clause_frontier = clause;
            frontier_found = true;
          }
          uint32_t candidate_representative = candidate.front();
          if (rank_common)
            for (const uint32_t literal : candidate)
              if (common_score[literal >> 1] >
                  common_score[candidate_representative >> 1])
                candidate_representative = literal;
          const bool candidate_amo =
              input.clause_is_amo && input.clause_is_amo[clause];
          if (first_amo && candidate_amo)
            candidate_representative = candidate.front();
          const uint32_t candidate_score =
              common_score[candidate_representative >> 1];
          const int candidate_orbit_size =
              orbits.orbit_size(static_cast<int>(candidate_representative));
          const bool same_length_class = !shortest_first ||
              candidate.size() == selected_active.size();
          const bool better =
              selected_clause == UINT64_MAX ||
              (shortest_first && candidate.size() < selected_active.size()) ||
              (same_length_class && settings.base_selection &&
               (candidate_amo > selected_amo ||
                (candidate_amo == selected_amo &&
                 (candidate_score > selected_score ||
                  (candidate_score == selected_score &&
                   candidate_orbit_size < selected_orbit_size)))));
          if (!better)
            continue;
          selected_clause = clause;
          representative = candidate_representative;
          selected_active = std::move(candidate);
          selected_amo = candidate_amo;
          selected_score = candidate_score;
          selected_orbit_size = candidate_orbit_size;
          if (!rank_common ||
              (first_amo && candidate_amo))
            break;
        }
        if (exhausted)
          break;

        if (ordered_remainder && !selected_amo && negation_point == UINT32_MAX)
          amo_exhausted = true;
        PlannedLevel next;
        if (selected_clause != UINT64_MAX) {
          next.operation = PlannedOperation::Fix;
          next.representative = representative;
          next.active = std::move(selected_active);
          next.amo = selected_amo;
        } else if (negation_point != UINT32_MAX) {
          next.representative = negation_point;
          next.operation = PlannedOperation::Cut;
        } else if (settings.cut_point == 1) {
          const auto open = [&](uint32_t point) {
            return !ordered[point >> 1] && !values[point] &&
                   !values[point ^ 1u] &&
                   orbits.orbit_size(static_cast<int>(point)) > 1;
          };
          int largest = 1;
          for (uint32_t point = input.domain_size & ~1u; point >= 2;) {
            point -= 2;
            if (open(point) &&
                orbits.orbit_size(static_cast<int>(point)) > largest) {
              largest = orbits.orbit_size(static_cast<int>(point));
              next.representative = point;
            }
          }
          if (next.representative == UINT32_MAX)
            break;
          const auto score = [&](uint32_t point) {
            uint32_t count = 0;
            for (const uint64_t clause : variable_clauses[point >> 1])
              count += marked_clauses[clause] != 0;
            return count;
          };
          uint32_t best = settings.base_selection ? score(next.representative) : 0;
          for (uint32_t point = 0;
               settings.base_selection && point < input.domain_size; ++point) {
            if (!open(point))
              continue;
            if (!charge(1)) {
              exhausted = true;
              break;
            }
            const uint32_t candidate = score(point);
            if (candidate > best) {
              best = candidate;
              next.representative = point;
            }
          }
          if (exhausted)
            break;
          next.operation = PlannedOperation::Cut;
        } else {
          for (uint32_t point = 0; point < input.domain_size; point += 2) {
            if (ordered[point >> 1] || values[point] || values[point ^ 1u])
              continue;
            const int orbit_size = orbits.orbit_size(static_cast<int>(point));
            if (orbit_size <= 1)
              continue;
            if (next.representative == UINT32_MAX ||
                (settings.base_selection &&
                 (common_score[point >> 1] >
                      common_score[next.representative >> 1] ||
                  (common_score[point >> 1] ==
                       common_score[next.representative >> 1] &&
                   orbit_size > orbits.orbit_size(
                                    static_cast<int>(next.representative))))))
              next.representative = point;
            if (!settings.base_selection && next.representative != UINT32_MAX)
              break;
          }
          if (next.representative == UINT32_MAX)
            break;
          next.operation = PlannedOperation::Cut;
        }

        if (trace.enabled() && selected_clause != UINT64_MAX) {
          std::string preview;
          const uint64_t begin = input.clause_offsets[selected_clause];
          const uint64_t end = input.clause_offsets[selected_clause + 1];
          for (uint64_t at = begin; at < end && at - begin < 16; ++at) {
            const uint32_t point = input.literals[at];
            const int variable = static_cast<int>(point / 2 + 1);
            if (at != begin)
              preview += ',';
            preview += std::to_string(point & 1u ? -variable : variable);
          }
          trace.event("cut_clause", "depth\t%zu\tclause\t%llu\tsize\t%llu"
                      "\tsigned_local_prefix16\t%s", base.size(),
                      static_cast<unsigned long long>(selected_clause),
                      static_cast<unsigned long long>(end - begin),
                      preview.c_str());
        }
        base.push_back(static_cast<int>(next.representative));
        trace.event("cut_select",
                    "depth\t%zu\tclause\t%llu\tactive\t%zu\tpoint\t%u"
                    "\torbit\t%d\tamo\t%u\toperation\t%u",
                    base.size() - 1,
                    static_cast<unsigned long long>(selected_clause),
                    next.active.size(), next.representative,
                    trace.enabled()
                        ? orbits.orbit_size(static_cast<int>(next.representative)) : 0,
                    static_cast<unsigned>(next.amo),
                    static_cast<unsigned>(next.operation));
        ordered[next.representative >> 1] = 1;
        schreier.set_base(base, settings.chain_mode == 2 || base.size() == 1,
                          point_bounds.empty() ? nullptr : &point_bounds);
        schreier.sift_random();
        poll();
        const uint64_t current_cost = schreier.get_computational_cost();
        if (current_cost < accounted_cost ||
            !charge(current_cost - accounted_cost)) {
          exhausted = true;
          break;
        }
        accounted_cost = current_cost;

        if (next.operation == PlannedOperation::Fix)
          for (const uint32_t literal : next.active)
            if (!schreier.is_in_fixed_orbit(
                    static_cast<int>(base.size() - 1),
                    static_cast<int>(literal))) {
              next.operation = PlannedOperation::None;
              break;
            }
        if (negation_point != UINT32_MAX ||
            next.operation == PlannedOperation::Fix) {
          values[next.representative] = 1;
          values[next.representative ^ 1u] = -1;
          if (next.amo)
            for (const uint32_t literal : next.active)
              if (literal != next.representative) {
                values[literal] = -1;
                values[literal ^ 1u] = 1;
              }
        }
        if (settings.base_selection)
          for (const uint64_t clause :
               variable_clauses[next.representative >> 1])
            marked_clauses[clause] = 1;
        planned.push_back(std::move(next));
        orbits.reset();
      }

      if (!exhausted && !failed) {
        trace.event("extend_begin", "levels\t%zu", base.size());
        schreier.extend();
        trace.event("extend_end", "levels\t%zu", base.size());
        poll();
        const uint64_t current_cost = schreier.get_computational_cost();
        if (current_cost < accounted_cost ||
            !charge(current_cost - accounted_cost))
          exhausted = true;
        accounted_cost = current_cost;
      }

      for (uint32_t level = 0;
           level < planned.size() && !exhausted && !failed; ++level) {
        const PlannedLevel &entry = planned[level];
        if (entry.operation == PlannedOperation::None)
          continue;
        const uint32_t representative = entry.representative;
        std::vector<uint32_t> targets;
        bool negation_unit = false;
        if (entry.operation == PlannedOperation::Fix) {
          targets = entry.active;
          targets.erase(std::remove(targets.begin(), targets.end(),
                                    representative),
                        targets.end());
        } else {
          const std::vector<int> fixed_orbit =
              schreier.get_fixed_orbit(static_cast<int>(level));
          if (std::find(fixed_orbit.begin(), fixed_orbit.end(),
                        static_cast<int>(representative ^ 1u)) !=
              fixed_orbit.end()) {
            negation_unit = true;
            targets.push_back(representative ^ 1u);
          } else
            for (const int point : fixed_orbit)
              if (point >= 0 &&
                  static_cast<uint32_t>(point) != representative)
                targets.push_back(static_cast<uint32_t>(point));
        }

        const uint64_t required_actions =
            entry.operation == PlannedOperation::Fix ? targets.size() + 1
                                                     : targets.size();
        if (kinds.size() > settings.maximum_instances ||
            required_actions > settings.maximum_instances - kinds.size() ||
            !charge(required_actions)) {
          exhausted = true;
          break;
        }

        std::vector<std::vector<uint32_t>> level_images;
        level_images.reserve(required_actions);
        for (const uint32_t target : targets) {
          poll();
          if (negation_unit) {
            transversal.reset();
            if (!schreier.get_transversal_element(
                    static_cast<int>(level), static_cast<int>(target),
                    transversal) ||
                transversal[target] != static_cast<int>(representative) ||
                transversal[representative] != static_cast<int>(target)) {
              failed = true;
              break;
            }
            std::vector<uint32_t> image(input.domain_size);
            for (uint32_t point = 0; point < input.domain_size; ++point) {
              if (!(point & 1023u))
                poll();
              const int mapped = transversal[point];
              if (mapped < 0 ||
                  static_cast<uint32_t>(mapped) >= input.domain_size) {
                failed = true;
                break;
              }
              image[point] = static_cast<uint32_t>(mapped);
            }
            if (!failed)
              level_images.push_back(std::move(image));
          } else {
            std::vector<uint32_t> inverse;
            if (!inverse_transversal(level, representative, target, inverse)) {
              failed = true;
              break;
            }
            level_images.push_back(std::move(inverse));
          }
        }
        if (failed)
          break;
        const uint64_t current_cost = schreier.get_computational_cost();
        if (current_cost < accounted_cost ||
            !charge(current_cost - accounted_cost)) {
          exhausted = true;
          break;
        }
        accounted_cost = current_cost;

        for (size_t i = 0; i < targets.size(); ++i) {
          kinds.push_back(entry.operation == PlannedOperation::Fix
                              ? 0
                              : negation_unit ? 3 : 2);
          levels.push_back(level);
          representatives.push_back(representative);
          others.push_back(targets[i]);
          permutations.push_back(std::move(level_images[i]));
        }
        if (entry.operation == PlannedOperation::Fix) {
          kinds.push_back(1);
          levels.push_back(level);
          representatives.push_back(representative);
          others.push_back(UINT32_MAX);
          std::vector<uint32_t> identity(input.domain_size);
          for (uint32_t point = 0; point < input.domain_size; ++point)
            identity[point] = point;
          permutations.push_back(std::move(identity));
        }
      }
    }

    result.domain_size = input.domain_size;
    trace.event("group_materialized",
                "levels\t%zu\tactions\t%zu\tticks\t%llu\tlimit\t%u\tfailed\t%u",
                base.size(), kinds.size(), static_cast<unsigned long long>(charged),
                static_cast<unsigned>(exhausted), static_cast<unsigned>(failed));
    result.ticks = charged;
    if (failed) {
      result.status = Status::Failed;
      return result.status;
    }
    const uint64_t count = kinds.size();
    if ((input.domain_size && count > UINT64_MAX / input.domain_size) ||
        count > SIZE_MAX / sizeof(uint8_t) ||
        count > SIZE_MAX / sizeof(uint32_t)) {
      result.status = Status::ResourceExhausted;
      return result.status;
    }
    const uint64_t cells = count * input.domain_size;
    if (cells > SIZE_MAX / sizeof(uint32_t)) {
      result.status = Status::ResourceExhausted;
      return result.status;
    }
    if (count) {
      result.kinds =
          static_cast<uint8_t *>(std::malloc(static_cast<size_t>(count)));
      result.levels = static_cast<uint32_t *>(
          std::malloc(static_cast<size_t>(count) * sizeof(uint32_t)));
      result.representatives = static_cast<uint32_t *>(
          std::malloc(static_cast<size_t>(count) * sizeof(uint32_t)));
      result.others = static_cast<uint32_t *>(
          std::malloc(static_cast<size_t>(count) * sizeof(uint32_t)));
      result.images = static_cast<uint32_t *>(
          std::malloc(static_cast<size_t>(cells) * sizeof(uint32_t)));
      if (!result.kinds || !result.levels || !result.representatives ||
          !result.others ||
          !result.images) {
        release(result);
        result.status = Status::ResourceExhausted;
        return result.status;
      }
      std::copy(kinds.begin(), kinds.end(), result.kinds);
      std::copy(levels.begin(), levels.end(), result.levels);
      std::copy(representatives.begin(), representatives.end(),
                result.representatives);
      std::copy(others.begin(), others.end(), result.others);
      uint64_t at = 0;
      for (const auto &permutation : permutations)
        for (const uint32_t image : permutation)
          result.images[at++] = image;
    }
    result.count = count;
    result.status = exhausted || planning_limited ? Status::Incomplete
                                                 : Status::Complete;
    return result.status;
  } catch (const dejavu::termination_requested &) {
    release(result);
    result.status = Status::Interrupted;
    return result.status;
  } catch (const std::bad_alloc &) {
    release(result);
    result.status = Status::ResourceExhausted;
    return result.status;
  } catch (...) {
    release(result);
    result.status = Status::Failed;
    return result.status;
  }
}

} // namespace tactix::symmetry_backend
