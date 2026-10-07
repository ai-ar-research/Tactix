#include "rules/lucky/nol_hypergraph_model.hpp"

#include "rules/lucky/model.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <bit>
#include <random>
#include <vector>

namespace tactix {
namespace {

// LymphoSAT's first trajectory allowance bounds retained trajectories. Public
// doses resume that trajectory; only reaching this limit starts another one.
constexpr uint64_t FIRST_RESTART_STEPS = 2'000'000;
constexpr uint64_t DEFAULT_SEED = 0x9e3779b97f4a7c15ULL;

struct Edge { std::array<uint32_t, 3> vertex{}; };
struct ColoredEdge {
  std::array<uint32_t, 3> group{};
  uint8_t color = 0;
};

struct Shape {
  uint32_t groups = 0;
  std::vector<uint32_t> active_groups;
  std::vector<int32_t> group_to_vertex;
  std::vector<uint8_t> allowed_colors;
  std::vector<Edge> edges;
  std::vector<std::vector<uint32_t>> incident;
  uint64_t validation_work = 0;
  uint64_t seed = DEFAULT_SEED;
};

struct Meter {
  Kernel &kernel;
  uint64_t limit;
  uint64_t ceiling;
  uint64_t used = 0;
  bool exhausted = false;
  bool interrupted = false;

  explicit Meter(Kernel &k)
      : kernel(k), limit(kernel_has_work_budget(k) ? k.work_budget : UINT64_MAX),
        ceiling(limit) {}
  ~Meter() { kernel.stats.nol_hypergraph_model_work += used; }

  bool charge(uint64_t amount) {
    if (exhausted || amount > ceiling - used) {
      if (!exhausted) ++kernel.stats.nol_hypergraph_model_budget_exhausted;
      exhausted = true;
      return false;
    }
    used += amount;
    return true;
  }

  bool reserve_terminal(uint64_t amount) {
    if (limit == UINT64_MAX) return true;
    if (amount > limit || used > limit - amount) {
      if (!exhausted) ++kernel.stats.nol_hypergraph_model_budget_exhausted;
      exhausted = true;
      return false;
    }
    ceiling = limit - amount;
    return true;
  }

  void release_terminal() {
    ceiling = limit;
    exhausted = false;
  }

  bool poll() {
    if (!interrupted &&
        kernel_rule_interrupt(kernel,
                              kernel.stats.nol_hypergraph_model_interrupted))
      interrupted = true;
    return interrupted;
  }
};

bool external_literal(const Kernel &k, Lit literal, uint32_t &group,
                      uint32_t &color) {
  const int dimacs = kernel_externalize(k, literal);
  if (!dimacs) return false;
  const uint64_t variable = dimacs < 0 ? uint64_t(-int64_t(dimacs))
                                       : uint64_t(dimacs);
  if (!variable || variable > k.external_to_internal.size()) return false;
  group = static_cast<uint32_t>((variable - 1) / 3);
  color = static_cast<uint32_t>((variable - 1) % 3);
  return true;
}

bool recognize(Kernel &k, Meter &meter, Shape &shape) {
  const uint64_t external = k.external_to_internal.size();
  if (!external || external % 3 || external / 3 > UINT32_MAX ||
      k.nvars != external)
    return false;
  shape.groups = static_cast<uint32_t>(external / 3);
  if (!meter.charge(4 * uint64_t(shape.groups))) return false;
  shape.group_to_vertex.assign(shape.groups, -1);
  shape.allowed_colors.assign(shape.groups, 7);
  std::vector<uint8_t> active(shape.groups, 0);
  std::vector<ColoredEdge> colored_edges;

  // This constructor is exact for the original three-literal language.  Any
  // binary clause means the live formula has another constraint family (or a
  // transformation has changed the encoding), so decline without guessing.
  for (Lit literal = 0; literal < 2 * k.nvars; ++literal) {
    if (!meter.charge(1)) return false;
    ++shape.validation_work;
    for (const WatchEntry watch : k.watches.entries(literal)) {
      if (!meter.charge(1)) return false;
      ++shape.validation_work;
      if (watch.binary()) return false;
    }
  }

  uint64_t visited = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    if (!meter.charge(1)) return false;
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    shape.validation_work += 1 + (clause->garbage() ? 0 : clause->size);
    if (!clause->garbage() && !clause->redundant()) {
      if (clause->size != 3 || !meter.charge(4)) return false;
      std::array<uint32_t, 3> groups{}, colors{};
      bool all_positive = true, all_negative = true;
      for (uint32_t index = 0; index < 3; ++index) {
        const Lit literal = clause->begin()[index];
        if (!external_literal(k, literal, groups[index], colors[index]))
          return false;
        all_positive &= !negated(literal);
        all_negative &= negated(literal);
        shape.seed ^= uint64_t(kernel_externalize(k, literal) + 1'000'003) +
                      DEFAULT_SEED + (shape.seed << 6) + (shape.seed >> 2);
      }
      if (all_positive) {
        if (groups[0] != groups[1] || groups[0] != groups[2]) return false;
        std::sort(colors.begin(), colors.end());
        if (colors != std::array<uint32_t, 3>{0, 1, 2}) return false;
        active[groups[0]] = 1;
      } else if (all_negative) {
        if (colors[0] != colors[1] || colors[0] != colors[2]) return false;
        std::sort(groups.begin(), groups.end());
        if (groups[0] == groups[1] || groups[1] == groups[2]) return false;
        colored_edges.push_back({groups, static_cast<uint8_t>(colors[0])});
      } else {
        return false;
      }
    }
    ref += words;
    if (!(++visited & ((uint64_t{1} << 18) - 1)) && meter.poll())
      return false;
  }
  if (colored_edges.empty()) return false;

  for (uint32_t group = 0; group < shape.groups; ++group) {
    uint8_t allowed = 7;
    uint32_t true_count = 0, true_color = 0;
    for (uint32_t color = 0; color < 3; ++color) {
      const Lit external_lit = make_lit(3 * group + color, false);
      const int value = kernel_external_value(k, external_lit);
      if (value < 0) allowed &= uint8_t(~(uint8_t{1} << color));
      else if (value > 0) { ++true_count; true_color = color; }
    }
    if (!active[group]) {
      if (true_count) return false;
      shape.allowed_colors[group] = allowed;
      continue;
    }
    if (true_count > 1) return false;
    if (true_count) allowed = uint8_t{1} << true_color;
    if (!allowed) return false;
    shape.allowed_colors[group] = allowed;
    shape.group_to_vertex[group] =
        static_cast<int32_t>(shape.active_groups.size());
    shape.active_groups.push_back(group);
  }
  if (shape.active_groups.empty()) return false;

  if (!meter.charge(uint64_t(colored_edges.size()) *
      (2 * std::bit_width(uint64_t(colored_edges.size())) + 3))) return false;
  std::sort(colored_edges.begin(), colored_edges.end(),
            [](const ColoredEdge &a, const ColoredEdge &b) {
              return a.group != b.group ? a.group < b.group
                                        : a.color < b.color;
            });
  std::vector<Edge> mapped;
  for (size_t begin = 0; begin < colored_edges.size();) {
    size_t end = begin;
    uint8_t mask = 0;
    while (end < colored_edges.size() &&
           colored_edges[end].group == colored_edges[begin].group) {
      mask |= uint8_t{1} << colored_edges[end].color;
      ++end;
    }
    // A noL hyperedge is represented by one negative ternary for each of the
    // three colors.  A partial set is an ordinary CNF constraint, not the
    // theory relation this action optimizes, so conservatively decline it.
    if (mask != 7) return false;
    Edge edge{colored_edges[begin].group};
    for (uint32_t &group : edge.vertex) {
      const int32_t vertex = shape.group_to_vertex[group];
      if (vertex < 0) return false;
      group = static_cast<uint32_t>(vertex);
    }
    mapped.push_back(edge);
    begin = end;
  }
  if (mapped.empty()) return false;
  if (!meter.charge(uint64_t(mapped.size()) *
      (2 * std::bit_width(uint64_t(mapped.size())) + 5))) return false;
  std::sort(mapped.begin(), mapped.end(), [](const Edge &a, const Edge &b) {
    return a.vertex < b.vertex;
  });
  mapped.erase(std::unique(mapped.begin(), mapped.end(), [](const Edge &a,
                                                            const Edge &b) {
                 return a.vertex == b.vertex;
               }), mapped.end());
  shape.edges = std::move(mapped);
  shape.incident.assign(shape.active_groups.size(), {});
  for (uint32_t edge = 0; edge < shape.edges.size(); ++edge)
    for (const uint32_t vertex : shape.edges[edge].vertex)
      shape.incident[vertex].push_back(edge);
  return true;
}

struct Candidate {
  std::vector<uint8_t> colors;
  std::vector<uint8_t> bad;
  std::vector<uint32_t> bad_edges;
  std::vector<int32_t> bad_position;
};

void add_bad(Candidate &candidate, uint32_t edge) {
  if (candidate.bad[edge]) return;
  candidate.bad[edge] = 1;
  candidate.bad_position[edge] = static_cast<int32_t>(candidate.bad_edges.size());
  candidate.bad_edges.push_back(edge);
}

void remove_bad(Candidate &candidate, uint32_t edge) {
  if (!candidate.bad[edge]) return;
  const uint32_t position = static_cast<uint32_t>(candidate.bad_position[edge]);
  const uint32_t last = candidate.bad_edges.back();
  candidate.bad_edges[position] = last;
  candidate.bad_position[last] = static_cast<int32_t>(position);
  candidate.bad_edges.pop_back();
  candidate.bad_position[edge] = -1;
  candidate.bad[edge] = 0;
}

bool edge_bad_after(const Shape &shape, const Candidate &candidate,
                    uint32_t edge, uint32_t changed, uint8_t color) {
  const auto &vertices = shape.edges[edge].vertex;
  const uint8_t first = vertices[0] == changed ? color
                                               : candidate.colors[vertices[0]];
  for (uint32_t index = 1; index < 3; ++index) {
    const uint32_t vertex = vertices[index];
    const uint8_t current = vertex == changed ? color : candidate.colors[vertex];
    if (current != first) return false;
  }
  return true;
}

std::vector<int8_t> signed_assignment(const Kernel &k, const Shape &shape,
                                      const std::vector<uint8_t> &colors) {
  // Zero is an unknown phase in the partial-guidance API.  Only variables in
  // recognized active color triples are theory-backed; leave every inactive
  // or unrelated variable unknown.  The validated full-model path below
  // deliberately completes those unknowns to false.
  std::vector<int8_t> assignment(k.nvars, 0);
  for (uint32_t vertex = 0; vertex < colors.size(); ++vertex) {
    const uint32_t group = shape.active_groups[vertex];
    for (uint32_t color = 0; color < 3; ++color) {
      const Var internal = k.external_to_internal[3 * group + color];
      if (internal != INVALID_VAR && internal < assignment.size())
        assignment[internal] = color == colors[vertex] ? 1 : -1;
    }
  }
  return assignment;
}

} // namespace

struct NolHypergraphWalkSession::State {
  const Kernel *owner = nullptr;
  Shape shape;
  Candidate candidate;
  std::vector<uint32_t> order;
  size_t cursor = 0;
  bool initialized = false;
  std::mt19937_64 random;
  std::vector<uint8_t> best;
  uint64_t best_bad = UINT64_MAX;
  uint64_t published_best_bad = UINT64_MAX;
  uint64_t steps = 0, trajectory_steps = 0, trajectories = 0;
};
NolHypergraphWalkSession::NolHypergraphWalkSession() = default;
NolHypergraphWalkSession::~NolHypergraphWalkSession() = default;
void NolHypergraphWalkSession::clear() { state_.reset(); }
bool NolHypergraphWalkSession::authenticated(const Kernel &kernel) const {
  return state_ && state_->owner == &kernel;
}
bool NolHypergraphWalkSession::initialized() const { return state_ && state_->initialized; }
uint64_t NolHypergraphWalkSession::steps() const { return state_ ? state_->steps : 0; }
uint64_t NolHypergraphWalkSession::trajectories() const { return state_ ? state_->trajectories : 0; }
uint64_t NolHypergraphWalkSession::candidate_fingerprint() const {
  uint64_t hash = DEFAULT_SEED;
  if(state_) for(uint8_t color:state_->candidate.colors)hash=(hash^color)*1099511628211ULL;
  return hash;
}
namespace {
using Walk = NolHypergraphWalkSession::State;
void rebuild_private(const Shape &shape, Candidate &candidate) {
  candidate.bad.assign(shape.edges.size(),0);
  candidate.bad_position.assign(shape.edges.size(),-1);
  candidate.bad_edges.clear();
  for(uint32_t edge=0;edge<shape.edges.size();++edge)
    if(edge_bad_after(shape,candidate,edge,UINT32_MAX,0))add_bad(candidate,edge);
}
bool update_root(Kernel &k,Meter &meter,Walk &walk,bool &changed) {
  const auto &shape=walk.shape;
  if(!meter.charge(3*uint64_t(shape.groups)))return false;
  std::vector<uint8_t> allowed(shape.groups,7);
  for(uint32_t group=0;group<shape.groups;++group) {
    unsigned true_count=0,true_color=0;
    for(uint32_t color=0;color<3;++color) {
      const Lit lit=make_lit(3*group+color,false);
      const Lit internal=kernel_internalize(k,3*group+color+1);
      if(internal==INVALID_LIT||kernel_externalize(k,internal)!=int(3*group+color+1))return false;
      const int value=kernel_external_value(k,lit);
      if(value<0)allowed[group]&=uint8_t(~(1u<<color));
      if(value>0){++true_count;true_color=color;}
    }
    if(true_count>1||(!shape.group_to_vertex.empty()&&shape.group_to_vertex[group]<0&&true_count))return false;
    if(true_count)allowed[group]=uint8_t(1u<<true_color);
    if(shape.group_to_vertex[group]>=0&&!allowed[group])return false;
  }
  if(allowed==walk.shape.allowed_colors)return true;
  const uint64_t work=uint64_t(shape.active_groups.size())*3+uint64_t(shape.edges.size())*3;
  if(!meter.charge(work)||meter.poll())return false;
  walk.shape.allowed_colors=std::move(allowed);
  changed=true;
  // Root restrictions change the private optimization problem, never CNF.
  if(walk.initialized) {
    for(size_t vertex=0;vertex<walk.candidate.colors.size();++vertex) {
      const uint8_t mask=walk.shape.allowed_colors[walk.shape.active_groups[vertex]];
      if(!(mask&(1u<<walk.candidate.colors[vertex])))walk.candidate.colors[vertex]=uint8_t(std::countr_zero(unsigned(mask)));
    }
    rebuild_private(walk.shape,walk.candidate);
    walk.best=walk.candidate.colors;walk.best_bad=walk.candidate.bad_edges.size();
  } else {
    walk.cursor=0;walk.order.clear();walk.candidate.colors.clear();walk.best.clear();walk.best_bad=UINT64_MAX;
  }
  walk.published_best_bad=UINT64_MAX;
  return true;
}
bool initialize_candidate(Kernel &k,Meter &meter,Walk &walk) {
  const Shape &shape=walk.shape;
  const size_t vertices=shape.active_groups.size();
  if(walk.order.empty()) {
    const uint64_t work=vertices*(2*std::bit_width(uint64_t(vertices))+4);
    if(!meter.charge(work)||meter.poll())return false;
    walk.candidate.colors.assign(vertices,3);walk.order.resize(vertices);
    std::iota(walk.order.begin(),walk.order.end(),0);
    std::stable_sort(walk.order.begin(),walk.order.end(),[&](uint32_t a,uint32_t b) {
      const auto fixed=[&](uint32_t v){const auto mask=shape.allowed_colors[shape.active_groups[v]];return !(mask&(mask-1));};
      if(fixed(a)!=fixed(b))return fixed(a);
      return shape.incident[a].size()>shape.incident[b].size();
    });
  }
  while(walk.cursor<walk.order.size()) {
    const uint32_t vertex=walk.order[walk.cursor];
    if(!meter.charge(1+9*uint64_t(shape.incident[vertex].size()))||meter.poll())return false;
    const uint8_t allowed=shape.allowed_colors[shape.active_groups[vertex]];
    uint32_t best_score=UINT32_MAX,count=0;std::array<uint8_t,3> best{};
    for(uint8_t color=0;color<3;++color)if(allowed&(1u<<color)) {
      walk.candidate.colors[vertex]=color;uint32_t score=0;
      for(uint32_t edge:shape.incident[vertex])score+=edge_bad_after(shape,walk.candidate,edge,UINT32_MAX,0);
      if(score<best_score){best_score=score;count=0;}if(score==best_score)best[count++]=color;
    }
    walk.candidate.colors[vertex]=best[walk.random()%count];++walk.cursor;
  }
  if(!meter.charge(3*uint64_t(shape.edges.size())+vertices)||meter.poll())return false;
  rebuild_private(shape,walk.candidate);walk.initialized=true;walk.trajectory_steps=0;
  ++walk.trajectories;++k.stats.nol_hypergraph_model_restarts;
  if(walk.candidate.bad_edges.size()<walk.best_bad){walk.best_bad=walk.candidate.bad_edges.size();walk.best=walk.candidate.colors;}
  return true;
}
enum class WalkProgress { Pending, Found, Complete };
WalkProgress resume_search(Kernel &k,Meter &meter,Walk &walk) {
  if(!walk.initialized&&!initialize_candidate(k,meter,walk))return WalkProgress::Pending;
  const Shape &shape=walk.shape;
  while(walk.trajectory_steps<FIRST_RESTART_STEPS) {
    if(walk.candidate.bad_edges.empty())return WalkProgress::Found;
    if(meter.poll())return WalkProgress::Pending;
    // Stage RNG before selecting an edge. A declined move consumes no RNG,
    // candidate mutation or step; the next public dose resumes exactly.
    if(!meter.charge(2*((sizeof(walk.random)+sizeof(uint64_t)-1)/sizeof(uint64_t))))return WalkProgress::Pending;
    auto random=walk.random;
    const uint32_t edge=walk.candidate.bad_edges[random()%walk.candidate.bad_edges.size()];
    uint64_t work=1;
    for(uint32_t vertex:shape.edges[edge].vertex)work+=10*uint64_t(shape.incident[vertex].size())+40;
    if(!meter.charge(work))return WalkProgress::Pending;
    struct Move{int64_t delta;uint32_t vertex;uint8_t color;uint64_t tie;};
    std::array<Move,6> moves{};uint32_t count=0;
    for(uint32_t vertex:shape.edges[edge].vertex)for(uint8_t color=0;color<3;++color) {
      const uint8_t allowed=shape.allowed_colors[shape.active_groups[vertex]];
      if(color==walk.candidate.colors[vertex]||!(allowed&(1u<<color)))continue;
      int64_t delta=0;for(uint32_t incident:shape.incident[vertex])
        delta+=int64_t(edge_bad_after(shape,walk.candidate,incident,vertex,color))-walk.candidate.bad[incident];
      moves[count++]={delta,vertex,color,random()};
    }
    if(!count) {
      walk.initialized=false;walk.cursor=0;walk.order.clear();
      return WalkProgress::Complete;
    }
    std::sort(moves.begin(),moves.begin()+count,[](const Move&a,const Move&b){return a.delta!=b.delta?a.delta<b.delta:a.tie<b.tie;});
    uint32_t limit=1;const auto roll=random()%1000;
    if(roll<35)limit=count;else if(roll<140)limit=std::min<uint32_t>(4,count);
    const auto move=moves[random()%limit];
    const uint64_t next_bad=uint64_t(int64_t(walk.candidate.bad_edges.size())+move.delta);
    if(next_bad<walk.best_bad&&!meter.charge(walk.candidate.colors.size()))return WalkProgress::Pending;
    // Two color deltas and both flip incidence scans were reserved above.
    for(uint32_t incident:shape.incident[move.vertex])remove_bad(walk.candidate,incident);
    walk.candidate.colors[move.vertex]=move.color;
    for(uint32_t incident:shape.incident[move.vertex])if(edge_bad_after(shape,walk.candidate,incident,UINT32_MAX,0))add_bad(walk.candidate,incident);
    walk.random=random;++walk.steps;++walk.trajectory_steps;++k.stats.nol_hypergraph_model_steps;
    if(walk.candidate.bad_edges.size()<walk.best_bad){walk.best_bad=walk.candidate.bad_edges.size();walk.best=walk.candidate.colors;}
  }
  // Explicit bounded trajectory boundary; initialization resumes separately.
  walk.initialized=false;walk.cursor=0;walk.order.clear();
  return WalkProgress::Complete;
}
} // namespace

RuleEligibility nol_hypergraph_model_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("no-conflict", !k.conflict_valid)
      .fact("no-pending", !k.pending.valid)
      .fact("root-level", k.level == 0)
      .fact("propagated", kernel_propagation_fixpoint(k))
      .fact("sparse-mode", !k.dense)
      .fact("no-probe-transaction", !k.probing.recording)
      .fact("large-watches-connected", k.large_watches_connected);
}

NolHypergraphAuthenticationResult nol_hypergraph_model_authenticate(Kernel &k) {
  assert(nol_hypergraph_model_guard(k));
  Meter meter(k);
  NolHypergraphAuthenticationResult result;
  const auto finish = [&](NolHypergraphAuthenticationStatus status) {
    result.status = status;
    result.work = meter.used;
    return result;
  };
  if (meter.poll())
    return finish(NolHypergraphAuthenticationStatus::Interrupted);
  if (!k.nol_hypergraph_walk_session)
    return finish(NolHypergraphAuthenticationStatus::NoStructure);
  auto &session = *k.nol_hypergraph_walk_session;
  if (session.state_ && session.state_->owner == &k)
    return finish(NolHypergraphAuthenticationStatus::Authenticated);

  auto next = std::make_unique<NolHypergraphWalkSession::State>();
  if (!recognize(k, meter, next->shape)) {
    if (meter.poll())
      return finish(NolHypergraphAuthenticationStatus::Interrupted);
    return finish(meter.exhausted
                      ? NolHypergraphAuthenticationStatus::Exhausted
                      : NolHypergraphAuthenticationStatus::NoStructure);
  }
  if (meter.poll())
    return finish(NolHypergraphAuthenticationStatus::Interrupted);
  next->owner = &k;
  next->random.seed(next->shape.seed);
  k.stats.nol_hypergraph_model_vertices = next->shape.active_groups.size();
  k.stats.nol_hypergraph_model_edges = next->shape.edges.size();
  session.state_ = std::move(next);
  ++k.stats.nol_hypergraph_model_recognized;
  result.recognized = true;
  return finish(NolHypergraphAuthenticationStatus::Authenticated);
}

NolHypergraphSliceResult nol_hypergraph_model_slice(Kernel &k) {
  assert(nol_hypergraph_model_guard(k));
  ++k.stats.nol_hypergraph_model_calls;
  Meter meter(k);
  NolHypergraphSliceResult result;
  const auto finish = [&](NolHypergraphSliceStatus status,
                          ApplyResult applied = ApplyResult::NoOp) {
    result.status = status;
    result.result = applied;
    result.work = meter.used;
    return result;
  };
  if(meter.poll())return finish(NolHypergraphSliceStatus::Interrupted);
  NolHypergraphWalkSession temporary;
  auto &session=k.nol_hypergraph_walk_session?*k.nol_hypergraph_walk_session:temporary;
  // This SAT-only heuristic keeps the authenticated source image once it has
  // recognized it.  Learned clauses, simplification, proof helpers and
  // compaction do not invalidate a coloring trajectory: current external
  // mappings and root values are checked below, and SAT is reported only
  // after validating the completed assignment against the whole live CNF.
  // A stale image can therefore waste a slice but cannot justify a clause,
  // unit, conflict or model.
  const bool compatible=session.state_&&session.state_->owner==&k;
  if(!compatible) {
    auto next=std::make_unique<NolHypergraphWalkSession::State>();
    if(!recognize(k,meter,next->shape)) {
      if(meter.poll())return finish(NolHypergraphSliceStatus::Interrupted);
      return finish(meter.exhausted ? NolHypergraphSliceStatus::Pending
                                    : NolHypergraphSliceStatus::NoStructure);
    }
    if(meter.poll())return finish(NolHypergraphSliceStatus::Interrupted);
    next->owner=&k;
    next->random.seed(next->shape.seed);session.state_=std::move(next);
    ++k.stats.nol_hypergraph_model_recognized;
    result.recognized=true;
  }
  auto &walk=*session.state_;auto &shape=walk.shape;
  result.objective_before=walk.published_best_bad;
  if(!update_root(k,meter,walk,result.root_changed)) {
    if(meter.poll())return finish(NolHypergraphSliceStatus::Interrupted);
    return finish(meter.exhausted ? NolHypergraphSliceStatus::Pending
                                  : NolHypergraphSliceStatus::NoStructure);
  }
  if(result.root_changed)result.objective_before=UINT64_MAX;
  k.stats.nol_hypergraph_model_vertices=shape.active_groups.size();
  k.stats.nol_hypergraph_model_edges=shape.edges.size();
  const auto saturated_add = [](uint64_t a, uint64_t b) {
    return b > UINT64_MAX - a ? UINT64_MAX : a + b;
  };
  const uint64_t variables = k.nvars;
  const uint64_t external = k.external_to_internal.size();
  const uint64_t sat_tail = saturated_add(
      shape.validation_work, saturated_add(variables, variables));
  const uint64_t hint_tail = saturated_add(variables, external);
  const uint64_t terminal_reserve = saturated_add(
      variables, std::max(sat_tail, hint_tail));
  if (!meter.reserve_terminal(terminal_reserve))
    return finish(NolHypergraphSliceStatus::Pending);
  WalkProgress progress = resume_search(k, meter, walk);
  const auto &best=walk.best;const auto best_bad=walk.best_bad;
  result.objective_after=best_bad;
  if (meter.poll()) return finish(NolHypergraphSliceStatus::Interrupted);
  if (best.empty())
    return finish(progress==WalkProgress::Complete
                      ? NolHypergraphSliceStatus::TrajectoryComplete
                      : NolHypergraphSliceStatus::Pending);
  meter.release_terminal();
  if (!meter.charge(variables)) return finish(NolHypergraphSliceStatus::Pending);
  std::vector<int8_t> signed_model = signed_assignment(k, shape, best);
  if (progress==WalkProgress::Found) {
    if (!meter.charge(variables)) return finish(NolHypergraphSliceStatus::Pending);
    std::vector<uint8_t> model(k.nvars);
    for (Var variable = 0; variable < k.nvars; ++variable)
      model[variable] = signed_model[variable] > 0;
    if (lucky_model_valid(k, model, [&](uint64_t amount) {
          return meter.charge(amount);
        })) {
      if (!meter.charge(variables)) return finish(NolHypergraphSliceStatus::Pending);
      if(meter.poll())return finish(NolHypergraphSliceStatus::Interrupted);
      lucky_publish_model(k, model);
      ++k.stats.nol_hypergraph_model_sat;
      result.objective_after=0;
      return finish(NolHypergraphSliceStatus::Sat,ApplyResult::Sat);
    }
    if(meter.exhausted)return finish(NolHypergraphSliceStatus::Pending);
    if(!meter.interrupted) {
      // A complete theory coloring rejected by full CNF is an explicit
      // bounded trajectory boundary, never a SAT certificate or a conflict.
      walk.initialized=false;walk.cursor=0;walk.order.clear();
      progress=WalkProgress::Complete;
    }
  }
  if (!meter.charge(saturated_add(variables, external)))
    return finish(NolHypergraphSliceStatus::Pending);
  if(meter.poll())return finish(NolHypergraphSliceStatus::Interrupted);
  const uint64_t offered = k.stats.local_search_hints_offered;
  kernel_offer_local_search_partial_hint(
      k, strategy::LocalSearchKind::HypergraphColoring, signed_model, best_bad,
      true, {meter.used, strategy::LocalSearchEffortUnit::GraphWork});
  result.hint_published=k.stats.local_search_hints_offered!=offered;
  if(result.hint_published) {
    result.useful_hint=best_bad<walk.published_best_bad;
    if(result.useful_hint)walk.published_best_bad=best_bad;
  }
  return finish(progress==WalkProgress::Complete
                    ? NolHypergraphSliceStatus::TrajectoryComplete
                    : NolHypergraphSliceStatus::Pending,
                result.hint_published ? ApplyResult::Ok : ApplyResult::NoOp);
}

ApplyResult nol_hypergraph_model_apply(Kernel &k) {
  return nol_hypergraph_model_slice(k).result;
}

} // namespace tactix
