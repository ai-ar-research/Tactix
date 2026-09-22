#undef NDEBUG
#include "symmetry_backend/dejavu_backend.hpp"
#include "symmetry_backend/sparse_permutation.hpp"
#include "symmetry_backend/group_input.hpp"
#include "rules/substitution_redundancy/cut_orbit_index.hpp"
#include "diagnostics/symmetry_trace.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <vector>

using namespace tactix::symmetry_backend;

namespace {

void symmetry_trace_levels_separate_campaign_and_generator_detail() {
  unsetenv("TACTIX_SR_TRACE");
  {
    tactix::SymmetryTrace trace;
    assert(!trace.enabled() && !trace.detailed());
  }
  setenv("TACTIX_SR_TRACE", "1", 1);
  {
    tactix::SymmetryTrace trace;
    assert(trace.enabled() && !trace.detailed());
  }
  setenv("TACTIX_SR_TRACE", "2", 1);
  {
    tactix::SymmetryTrace trace;
    assert(trace.enabled() && trace.detailed());
  }
  unsetenv("TACTIX_SR_TRACE");
}

// Matching edges split one root cell; an unrelated cycle and many root
// singletons remain untouched. Images must include exactly the split cell,
// retaining discovery order even across repeated calls and prefix requests.
void row_images_ignore_untouched_root_cells() {
  constexpr uint32_t n = 132;
  std::vector<uint32_t> colors(n), degrees(n), edges;
  const auto add = [&](uint32_t u, uint32_t v) {
    if (u > v) std::swap(u, v);
    edges.push_back(u); edges.push_back(v); ++degrees[u]; ++degrees[v];
  };
  add(0, 3); add(1, 2);
  for (uint32_t v = 4; v < 20; ++v) {
    colors[v] = 1;
    add(v, v == 19 ? 4 : v + 1);
  }
  for (uint32_t v = 20; v < n; ++v) colors[v] = v;
  Graph graph{n, edges.size()/2, colors.data(), degrees.data(), edges.data()};
  RefinementSession *session = nullptr;
  uint64_t preparation_ticks = 0;
  assert(prepare_refinement(graph, UINT64_MAX, nullptr, nullptr, session,
                            preparation_ticks) == Status::Complete);
  const uint32_t anchors[] = {0, 1};
  const uint32_t sources[] = {0, 3, 1, 2};
  const uint32_t second[] = {1, 0, 3, 2};
  uint64_t full_ticks = 0;
  for (uint32_t prefix : {n, 4u, n}) {
    RefinementRows rows;
    assert(refine_rows(*session, anchors, 2, prefix, UINT64_MAX, nullptr,
                       nullptr, rows) == Status::Complete);
    assert(rows.anchor_count == 2 && rows.domain_size == prefix);
    assert(rows.source_count == 4);
    for (uint32_t i = 0; i < 4; ++i) {
      assert(rows.sources[i] == sources[i]);
      assert(rows.images[i] == i);
      assert(rows.images[prefix+i] == second[i]);
    }
    for (uint32_t i = 4; i < prefix; ++i)
      assert(rows.images[i] == UINT32_MAX &&
             rows.images[prefix+i] == UINT32_MAX);
    full_ticks = rows.ticks;
    release(rows);
  }
  for (uint64_t limit = 0; limit <= full_ticks; ++limit) {
    RefinementRows rows;
    const Status status = refine_rows(*session, anchors, 2, n, limit,
                                     nullptr, nullptr, rows);
    assert(rows.ticks <= limit);
    if (status == Status::Complete) {
      assert(rows.source_count == 4);
      for (uint32_t i = 0; i < 4; ++i)
        assert(rows.sources[i] == sources[i]);
    } else {
      assert(status == Status::Incomplete && !rows.source_count);
    }
    release(rows);
  }
  release(session);
}

// Reduction and row completion may share a completed first individualization.
// Compare a warmed session with a cold one at every small work prefix and
// before/during/after interruption, including a different cached anchor.
void reduced_first_row_preserves_prefixes_and_stops() {
  const uint32_t colors[] = {0,0,0,0,0,0,0,0};
  const uint32_t degrees[] = {1,1,1,1,1,1,1,1};
  const uint32_t edges[] = {0,3,1,2,4,7,5,6};
  Graph graph{8,4,colors,degrees,edges};
  RefinementSession *cold=nullptr,*warm=nullptr;
  uint64_t ticks=0;
  assert(prepare_refinement(graph,UINT64_MAX,nullptr,nullptr,cold,ticks)==Status::Complete);
  assert(prepare_refinement(graph,UINT64_MAX,nullptr,nullptr,warm,ticks)==Status::Complete);
  const uint32_t anchors[]={0,1};
  RefinementRows full;
  assert(refine_rows(*cold,anchors,2,8,UINT64_MAX,nullptr,nullptr,full)==Status::Complete);
  const uint64_t maximum=full.ticks+1;release(full);
  struct Stop { int after; unsigned calls=0; };
  const auto stopped=[](void *opaque) noexcept {
    auto &state=*static_cast<Stop *>(opaque);
    return state.after>=0 && state.calls++>=static_cast<unsigned>(state.after);
  };
  unsigned compared=0;
  for(unsigned seed:{0u,1u,2u}) for(uint32_t prefix:{8u,4u,1u})
    for(uint64_t limit=0;limit<=maximum;++limit) for(int at=-1;at<24;++at) {
      const uint32_t vertices[]={seed==1?1u:0u,seed==1?0u:1u,2,3,4,5,6,7};
      RefinementOrbit reduced;
      const auto reduction=reduce_orbit(*warm,vertices,8,8,seed==2?0:UINT64_MAX,nullptr,nullptr,reduced);
      assert(reduction==(seed==2?Status::Incomplete:Status::Complete));
      release(reduced);
      Stop ca{at},wa{at};RefinementRows expected,actual;
      const auto a=refine_rows(*cold,anchors,2,prefix,limit,stopped,&ca,expected);
      const auto b=refine_rows(*warm,anchors,2,prefix,limit,stopped,&wa,actual);
      assert(a==b && expected.status==actual.status && expected.ticks==actual.ticks);
      assert(expected.anchor_count==actual.anchor_count && expected.domain_size==actual.domain_size);
      assert(expected.source_count==actual.source_count);
      assert(bool(expected.images)==bool(actual.images));
      for(uint32_t i=0;i<expected.source_count;++i)assert(expected.sources[i]==actual.sources[i]);
      if(expected.images)for(uint64_t i=0;i<uint64_t(expected.anchor_count)*expected.domain_size;++i)
        assert(expected.images[i]==actual.images[i]);
      release(expected);release(actual);++compared;
    }
  release(cold);release(warm);
  std::printf("first-refinement reuse: %u bounded/stop/prefix comparisons PASS\n",compared);
}

void sparse_group_input_preserves_results_and_budgets() {
  size_t comparisons = 0, productive = 0, incomplete = 0;
  std::vector<uint64_t> budgets;
  for (uint64_t budget = 0; budget <= 1000; ++budget) budgets.push_back(budget);
  budgets.push_back(100000);
  for (unsigned fixture = 0; fixture < 4; ++fixture) {
    const uint32_t domain = fixture == 3 ? 3 : 12;
    std::vector<uint32_t> dense;
    const auto add = [&](const std::vector<std::pair<uint32_t,uint32_t>> &moves) {
      const size_t start = dense.size();
      for (uint32_t p = 0; p < domain; ++p) dense.push_back(p);
      for (auto [a,b] : moves) dense[start+a] = b;
    };
    if (fixture == 3) add({{0,1},{1,2},{2,0}});
    else {
      add({{0,2},{2,0},{1,3},{3,1}});
      if (fixture == 0) add({{2,4},{4,2},{3,5},{5,3}});
      else add({{8,10},{10,8},{9,11},{11,9}});
      if (fixture == 2) add({{0,8},{8,0},{1,9},{9,1},{2,10},{10,2},{3,11},{11,3}});
      add({}); // An explicit identity remains legal.
      add({{0,2},{2,0},{1,3},{3,1}}); // Preserve duplicate generator insertion.
    }
    std::vector<uint64_t> offsets{0};
    std::vector<uint32_t> points, targets;
    for (size_t row = 0; row < dense.size()/domain; ++row) {
      for (uint32_t p = 0; p < domain; ++p)
        if (dense[row*domain+p] != p) { points.push_back(p); targets.push_back(dense[row*domain+p]); }
      offsets.push_back(points.size());
    }
    const std::vector<uint32_t> literals = fixture == 3 ? std::vector<uint32_t>{0,1,2}
        : std::vector<uint32_t>{0,2,4,8,10,0,8,1,3};
    const std::vector<uint64_t> clauses = fixture == 3 ? std::vector<uint64_t>{0,3}
        : std::vector<uint64_t>{0,3,5,7,9};
    std::vector<int8_t> values(domain,0);
    if (fixture == 2) values[4] = -1;
    GroupInput input{domain,dense.size()/domain,dense.data(),clauses.size()-1,
                     clauses.data(),literals.size(),literals.data(),values.data()};
    GroupInput sparse = input;
    sparse.generator_images=targets.data(); sparse.generator_points=points.data();
    sparse.generator_offsets=offsets.data(); sparse.generator_entries=points.size();
    assert(detail::validate_generator_rows(sparse, []{}));
    for (uint64_t row = 0; row < input.generator_count; ++row)
      for (uint32_t p = 0; p < domain; ++p)
        assert(detail::generator_image(sparse,row,p) == dense[row*domain+p]);
    for (uint8_t mode : {0,1,2}) {
      if (fixture == 3 && mode) continue;
      for (uint8_t base : {0,1,6}) for (bool factor : {false,true})
        for (uint64_t budget : budgets)
          for (uint64_t cap : {0ull,1ull,1000ull}) {
            GroupSettings settings;
            settings.chain_mode=mode;settings.base_selection=base;
            settings.factor_support_components=factor;
            settings.maximum_ticks=budget;settings.maximum_instances=cap;
            // Includes a soft-planning boundary as well as hard resource stops.
            settings.planning_tick_limit=base==6?79:0;
            Fixings old_rows,new_rows;
            const auto old_status=fix_clauses(input,settings,nullptr,nullptr,old_rows);
            const auto new_status=fix_clauses(sparse,settings,nullptr,nullptr,new_rows);
            assert(old_status==new_status && old_rows.status==new_rows.status &&
                   old_rows.count==new_rows.count && old_rows.domain_size==new_rows.domain_size &&
                   old_rows.ticks==new_rows.ticks);
            assert(old_status==Status::Complete || old_status==Status::Incomplete);
            for (uint64_t row=0;row<old_rows.count;++row) {
              assert(old_rows.kinds[row]==new_rows.kinds[row] && old_rows.levels[row]==new_rows.levels[row] &&
                     old_rows.representatives[row]==new_rows.representatives[row] &&
                     old_rows.others[row]==new_rows.others[row]);
              std::vector<uint32_t> old_image(domain),new_image(domain);
              assert(materialize_fixing_image(old_rows,row,old_image.data()));
              assert(materialize_fixing_image(new_rows,row,new_image.data()));
              assert(old_image==new_image);
            }
            ++comparisons; productive += old_rows.count!=0;
            incomplete += old_status==Status::Incomplete;
            release(old_rows);release(new_rows);
          }
    }
    for (const GroupInput &stoppable : {input, sparse}) {
    struct PollCounter { size_t calls = 0, stop_at = SIZE_MAX; } counter;
    const auto counted_stop = [](void *opaque) noexcept {
      auto &state = *static_cast<PollCounter *>(opaque);
      return ++state.calls >= state.stop_at;
    };
    GroupSettings resumed_settings;
    resumed_settings.chain_mode = fixture == 3 ? 0 : 2;
    resumed_settings.factor_support_components = true;
    Fixings uninterrupted, interrupted, resumed;
    const Status full_status = fix_clauses(stoppable, resumed_settings, counted_stop,
                                          &counter, uninterrupted);
    assert(counter.calls > 2);
    PollCounter stopping{0, counter.calls / 2};
    assert(fix_clauses(stoppable, resumed_settings, counted_stop, &stopping, interrupted) ==
           Status::Interrupted);
    assert(!interrupted.count && !interrupted.images && !interrupted.image_offsets);
    assert(fix_clauses(stoppable, resumed_settings, nullptr, nullptr, resumed) == full_status);
    assert(resumed.count == uninterrupted.count && resumed.ticks == uninterrupted.ticks);
    for (uint64_t row = 0; row < resumed.count; ++row) {
      assert(resumed.kinds[row] == uninterrupted.kinds[row] &&
             resumed.levels[row] == uninterrupted.levels[row] &&
             resumed.representatives[row] == uninterrupted.representatives[row] &&
             resumed.others[row] == uninterrupted.others[row]);
      std::vector<uint32_t> a(domain), b(domain);
      assert(materialize_fixing_image(resumed,row,a.data()));
      assert(materialize_fixing_image(uninterrupted,row,b.data()));
      assert(a==b);
    }
    release(uninterrupted); release(interrupted); release(resumed);
    }
  }
  // A singleton 0->2 collides with the implicit identity at 2 despite having
  // distinct source and target lists; it must never enter Schreier.
  const int8_t values[] = {0,0,0,0};
  uint64_t offsets[] = {0,1};
  uint32_t points[] = {0}, targets[] = {2};
  GroupInput malformed{4,1,targets,0,nullptr,0,nullptr,values};
  malformed.generator_offsets=offsets;malformed.generator_points=points;malformed.generator_entries=1;
  Fixings result;
  assert(fix_clauses(malformed,GroupSettings{},nullptr,nullptr,result)==Status::Invalid);
  assert(!result.count && !result.images);
  targets[0]=0;
  assert(fix_clauses(malformed,GroupSettings{},nullptr,nullptr,result)==Status::Invalid);
  targets[0]=4;
  assert(fix_clauses(malformed,GroupSettings{},nullptr,nullptr,result)==Status::Invalid);
  targets[0]=2; offsets[1]=2;
  assert(fix_clauses(malformed,GroupSettings{},nullptr,nullptr,result)==Status::Invalid);
  uint64_t triple_offsets[] = {0,3};
  uint32_t triple_points[] = {0,1,2}, triple_targets[] = {1,2,0};
  GroupInput triple{3,1,triple_targets,0,nullptr,0,nullptr,values};
  triple.generator_offsets=triple_offsets;triple.generator_points=triple_points;triple.generator_entries=3;
  assert(detail::validate_generator_rows(triple, []{}));
  for (unsigned defect = 0; defect < 6; ++defect) {
    const std::array<uint32_t,3> saved_points{0,1,2}, saved_targets{1,2,0};
    switch (defect) {
    case 0: triple_points[1]=0; break;
    case 1: std::swap(triple_points[0],triple_points[1]);
            std::swap(triple_targets[0],triple_targets[1]); break;
    case 2: triple_targets[0]=2; break;
    case 3: triple_offsets[0]=1; break;
    case 4: triple.generator_images=nullptr; break;
    case 5: triple_points[2]=3; break;
    }
    assert(!detail::validate_generator_rows(triple, []{}));
    std::copy(saved_points.begin(),saved_points.end(),triple_points);
    std::copy(saved_targets.begin(),saved_targets.end(),triple_targets);
    triple_offsets[0]=0;triple.generator_images=triple_targets;
  }
  uint64_t nonmonotone[] = {0,3,2,3};
  triple.generator_offsets=nonmonotone; triple.generator_count=3;
  assert(!detail::validate_generator_rows(triple, []{}));
  assert(comparisons && productive && incomplete);
  std::printf("sparse group oracle: %zu comparisons, %zu productive, %zu incomplete PASS\n",
              comparisons,productive,incomplete);
}

void sparse_detector_storage_preserves_dense_semantics() {
  using detail::SparsePermutation;
  using detail::capture_permutation_prefix;
  std::array<int, 5> dense = {0, 1, 2, 3, 4};
  size_t captures = 0, comparisons = 0, pair_order_disagreements = 0;
  std::vector<std::array<int, 5>> originals;
  do { originals.push_back(dense); } while (std::next_permutation(dense.begin(), dense.end()));
  for (uint32_t prefix = 0; prefix <= 5; ++prefix) {
    std::vector<SparsePermutation> maps;
    for (const auto &original : originals) {
      std::vector<int> support;
      for (int point = 4; point >= 0; --point)
        if (original[point] != point) support.push_back(point);
      SparsePermutation sparse, fallback, repeated;
      assert(capture_permutation_prefix(5, original.data(), support.size(),
                                        support.data(), prefix, sparse));
      assert(capture_permutation_prefix(5, original.data(), -1, nullptr, prefix, fallback));
      assert(sparse == fallback);
      assert(capture_permutation_prefix(5, original.data(), 5, nullptr, prefix, fallback));
      assert(sparse == fallback);
      for (int point = 0; point < 5; ++point) support.push_back(point);
      assert(capture_permutation_prefix(5, original.data(), support.size(),
                                        support.data(), prefix, repeated));
      assert(sparse == repeated);
      size_t at = 0;
      for (uint32_t point = 0; point < prefix; ++point) {
        uint32_t image = point;
        if (at < sparse.moved.size() && sparse.moved[at].first == point)
          image = sparse.moved[at++].second;
        assert(image == static_cast<uint32_t>(original[point]));
      }
      assert(at == sparse.moved.size());
      maps.push_back(std::move(sparse));
      ++captures;
    }
    for (size_t a = 0; a < maps.size(); ++a)
      for (size_t b = 0; b < maps.size(); ++b) {
        const bool less = std::lexicographical_compare(originals[a].begin(),
            originals[a].begin() + prefix, originals[b].begin(), originals[b].begin() + prefix);
        assert((maps[a] < maps[b]) == less);
        const bool equal = std::equal(originals[a].begin(), originals[a].begin() + prefix,
                                      originals[b].begin());
        assert((maps[a] == maps[b]) == equal);
        pair_order_disagreements += (maps[a].moved < maps[b].moved) != less;
        ++comparisons;
      }
  }
  assert(captures == 720 && comparisons == 86400 && pair_order_disagreements);
  SparsePermutation invalid;
  const int bad_support[] = {5}, negative_support[] = {-1};
  assert(!capture_permutation_prefix(5, dense.data(), 1, bad_support, 5, invalid));
  assert(!capture_permutation_prefix(5, dense.data(), 1, negative_support, 5, invalid));
  assert(!capture_permutation_prefix(5, dense.data(), -2, nullptr, 5, invalid));
  assert(!capture_permutation_prefix(5, dense.data(), -1, nullptr, 6, invalid));
  dense[0] = 5;
  assert(!capture_permutation_prefix(5, dense.data(), -1, nullptr, 5, invalid));
  dense[0] = -1;
  assert(!capture_permutation_prefix(5, dense.data(), -1, nullptr, 5, invalid));

  // A support far beyond a fixed prefix is retained without storing identities.
  std::vector<int> wide(250000);
  for (size_t point = 0; point < wide.size(); ++point) wide[point] = point;
  std::swap(wide[249997], wide[249999]);
  const int moved[] = {249999, 249997};
  assert(capture_permutation_prefix(wide.size(), wide.data(), 2, moved, wide.size(), invalid));
  assert(invalid.moved.size() == 2 && invalid.moved.front().first == 249997);
  assert(capture_permutation_prefix(wide.size(), wide.data(), 2, moved, 249998, invalid));
  assert(invalid.moved.size() == 1 && invalid.moved.front().second == 249999);

  // Decoder checks representation before writing, leaving semantic target
  // validation to the caller so a non-invariant prefix is not silently clipped.
  uint64_t offsets[] = {0, 2};
  uint32_t points[] = {0, 2}, targets[] = {2, 0};
  Permutations rows;
  rows.count = 1; rows.domain_size = 3; rows.images = targets;
  rows.image_offsets = offsets; rows.image_points = points; rows.image_entries = 2;
  std::array<uint32_t, 3> output = {9, 9, 9};
  assert(materialize_permutation_image(rows, 0, output.data()));
  assert((output == std::array<uint32_t, 3>{2, 1, 0}));
  for (unsigned defect = 0; defect < 6; ++defect) {
    output.fill(9);
    const auto old_offsets = std::array<uint64_t, 2>{offsets[0], offsets[1]};
    const auto old_points = std::array<uint32_t, 2>{points[0], points[1]};
    const auto old_targets = std::array<uint32_t, 2>{targets[0], targets[1]};
    switch (defect) {
    case 0: offsets[0] = 1; break;
    case 1: offsets[1] = 3; break;
    case 2: points[0] = 3; break;
    case 3: points[1] = points[0]; targets[1] = 1; break;
    case 4: std::swap(points[0], points[1]); std::swap(targets[0], targets[1]); break;
    case 5: targets[0] = points[0]; break;
    }
    assert(!materialize_permutation_image(rows, 0, output.data()));
    assert((output == std::array<uint32_t, 3>{9, 9, 9}));
    std::copy(old_offsets.begin(), old_offsets.end(), offsets);
    std::copy(old_points.begin(), old_points.end(), points);
    std::copy(old_targets.begin(), old_targets.end(), targets);
  }
  std::printf("sparse detector oracle: %zu captures, %zu comparisons, %zu pair-order negatives PASS\n",
              captures, comparisons, pair_order_disagreements);
}

void sparse_cut_checks_match_the_dense_predicate() {
  using tactix::CutOrbitIndex;
  using tactix::CutOrbitMember;
  using tactix::INVALID_LIT;
  using tactix::Lit;
  using tactix::SparseLiteralPermutation;
  struct Permutation {
    std::array<Lit, 6> dense;
    SparseLiteralPermutation sparse;
  };
  std::vector<Permutation> permutations;
  std::array<Lit, 3> variables = {0, 1, 2};
  do {
    for (unsigned phases = 0; phases < 8; ++phases) {
      Permutation permutation;
      permutation.sparse.domain = 6;
      for (Lit source = 0; source < 6; ++source) {
        const Lit target = 2 * variables[source / 2] ^
            ((phases >> (source / 2)) & 1u) ^ (source & 1u);
        permutation.dense[source] = target;
        if (source != target)
          permutation.sparse.moved.emplace_back(source, target);
      }
      assert(permutation.sparse.validate([](auto) { return false; },
                                         [](auto) { return true; }));
      permutations.push_back(std::move(permutation));
    }
  } while (std::next_permutation(variables.begin(), variables.end()));
  assert(permutations.size() == 48);

  size_t accepted = 0, moved_base = 0, orbit_escape = 0;
  for (unsigned first = 0; first < 64; ++first) {
    if (!(first & 1u)) continue;
    for (unsigned second = 0; second < 64; ++second) {
      if (!(second & 4u)) continue;
      const std::array<Lit, 5> bases = {0, 2, INVALID_LIT, INVALID_LIT, 0};
      std::array<std::unordered_set<Lit>, 5> orbits;
      for (Lit member = 0; member < 6; ++member) {
        if (first & (1u << member)) orbits[0].insert(member);
        if (second & (1u << member)) orbits[1].insert(member);
      }
      orbits[4] = {0, 1, 4, 5};
      std::vector<CutOrbitMember> members;
      for (uint32_t level = 0; level < bases.size(); ++level)
        for (const Lit member : orbits[level])
          members.push_back({level, bases[level], member});
      members.push_back(members.front());
      if (second & 8u) std::reverse(members.begin(), members.end());
      const CutOrbitIndex index(members);
      assert(index.valid());
      for (const auto &permutation : permutations)
        for (uint32_t level = 0; level <= bases.size(); ++level) {
          // Independent full-domain oracle: check every earlier base/member,
          // without consulting sparse support or the new membership index.
          bool base_bad = false, orbit_bad = false;
          for (uint32_t earlier = 0; earlier < level; ++earlier) {
            if (bases[earlier] == INVALID_LIT) continue;
            if (permutation.dense[bases[earlier]] != bases[earlier])
              base_bad = true;
            for (const Lit member : orbits[earlier])
              if (!orbits[earlier].count(permutation.dense[member]))
                orbit_bad = true;
          }
          const bool dense = !base_bad && !orbit_bad;
          assert(index.preserves(permutation.sparse, level) == dense);
          if (dense) ++accepted;
          else if (base_bad) ++moved_base;
          else ++orbit_escape;
        }
    }
  }
  assert(accepted && moved_base && orbit_escape);

  const std::array<CutOrbitMember, 0> none;
  const CutOrbitIndex empty(none);
  for (const auto &permutation : permutations)
    assert(empty.preserves(permutation.sparse, UINT32_MAX));

  const std::array<CutOrbitMember, 2> conflicting = {{{7, 0, 2}, {7, 2, 0}}};
  const CutOrbitIndex invalid(conflicting);
  assert(!invalid.valid());
  for (const auto &permutation : permutations)
    assert(!invalid.preserves(permutation.sparse, 8));

  // Huge, gapped and reversed level identifiers retain numeric ordering without
  // allocating by their value. A cut at the query level has no earlier claim.
  const std::array<CutOrbitMember, 3> large = {{{UINT32_MAX, 2, 4},
      {UINT32_MAX - 1, 0, 2}, {7, 0, 4}}};
  const CutOrbitIndex gapped(large);
  assert(gapped.valid());
  for (const auto &permutation : permutations)
    for (const uint32_t level : {0u, 7u, 8u, UINT32_MAX - 1, UINT32_MAX}) {
      bool dense = true;
      for (const auto &cut : large) {
        if (cut.level >= level) continue;
        const std::array<Lit, 2> orbit = {cut.base, cut.member};
        if (permutation.dense[cut.base] != cut.base) dense = false;
        for (const Lit member : orbit)
          if (std::find(orbit.begin(), orbit.end(), permutation.dense[member]) ==
              orbit.end()) dense = false;
      }
      assert(gapped.preserves(permutation.sparse, level) == dense);
    }

  SparseLiteralPermutation malformed{6, {{0, 2}}};
  assert(!malformed.validate([](auto) { return false; },
                             [](auto) { return true; }));
  malformed.moved = {{0, 0}};
  assert(!malformed.validate([](auto) { return false; },
                             [](auto) { return true; }));
  std::printf("cut-closure oracle: %zu accepts, %zu moved-base rejects, "
              "%zu orbit-escape rejects; conflict and huge-level controls PASS\n",
              accepted, moved_base, orbit_escape);
}

struct StopAfter {
  unsigned calls = 0;
  unsigned limit = 0;
};

bool stop_after(void *opaque) noexcept {
  auto &state = *static_cast<StopAfter *>(opaque);
  return ++state.calls >= state.limit;
}

Graph two_isolated_vertices(uint32_t (&colors)[2],
                            uint32_t (&degrees)[2]) {
  colors[0] = colors[1] = 0;
  degrees[0] = degrees[1] = 0;
  return Graph{2, 0, colors, degrees, nullptr};
}

void detection_owns_results_and_observes_stop() {
  uint32_t colors[2], degrees[2];
  const Graph graph = two_isolated_vertices(colors, degrees);
  DetectionSettings settings;
  settings.search_budget = 64;
  settings.maximum_generators = 8;

  Permutations permutations;
  const Status status = detect(graph, settings, nullptr, nullptr, permutations);
  assert(status == Status::Complete);
  assert(permutations.complete_group && permutations.domain_size == 2);
  assert(permutations.count > 0 && permutations.images);
  release(permutations);
  assert(!permutations.images && !permutations.count);
  release(permutations);

  // The first callback is the public preflight and the next calls occur while
  // the graph is copied and inside DejaVu. Reaching four proves that a request
  // raised after entry unwinds the active backend call.
  StopAfter stopping{0, 4};
  assert(detect(graph, settings, stop_after, &stopping, permutations) ==
         Status::Interrupted);
  assert(stopping.calls >= 4 && !permutations.images);
}

void detector_order_is_effective_reproducible_and_bounded() {
  // Independent color classes exercise multiple callbacks and projection;
  // compare exact maps, not merely generator counts or a nominal option flag.
  bool changed = false;
  for (uint32_t fixture = 0; fixture < 4; ++fixture) {
    uint32_t colors[12] = {}, degrees[12] = {};
    std::vector<uint32_t> edges;
    for (unsigned i = 0; i < 12; ++i) {
      if (fixture == 0) colors[i] = i / 4;
      for (unsigned j = i + 1; j < 12; ++j) {
        const bool edge = fixture == 1 ? (j == i + 1 || (i == 0 && j == 11))
            : fixture == 2 ? (i < 6 && j >= 6)
            : fixture == 3 ? (i / 4 == j / 4) : false;
        if (edge) {
          edges.push_back(i);
          edges.push_back(j);
          ++degrees[i];
          ++degrees[j];
        }
      }
    }
    const Graph graph{12, edges.size() / 2, colors, degrees, edges.data()};
    DetectionSettings settings;
    settings.seed = fixture;
    auto collect = [&](bool preserve, uint64_t limit) {
      settings.preserve_generator_order = preserve;
      settings.maximum_generators = limit;
      Permutations result;
      const auto status = detect(graph, settings, nullptr, nullptr, result);
      assert(status == Status::Complete || status == Status::Incomplete);
      if (limit == UINT64_MAX) assert(result.complete_group);
      assert(result.count <= limit);
      std::vector<std::vector<uint32_t>> maps;
      for (uint64_t i = 0; i < result.count; ++i)
        maps.emplace_back(result.images + i * result.domain_size,
                          result.images + (i + 1) * result.domain_size);
      settings.sparse_output = true;
      Permutations sparse;
      assert(detect(graph, settings, nullptr, nullptr, sparse) == status);
      assert(sparse.image_offsets && sparse.count == result.count &&
             sparse.domain_size == result.domain_size &&
             sparse.complete_group == result.complete_group);
      for (uint64_t row = 0; row < sparse.count; ++row) {
        std::vector<uint32_t> image(sparse.domain_size);
        assert(materialize_permutation_image(sparse, row, image.data()));
        assert(image == maps[row]);
      }
      release(sparse);
      assert(!sparse.images && !sparse.image_points && !sparse.image_offsets &&
             !sparse.count && !sparse.image_entries);
      settings.sparse_output = false;
      release(result);
      return maps;
    };
    const auto canonical = collect(false, UINT64_MAX);
    const auto callback = collect(true, UINT64_MAX);
    assert(canonical.size() > 1 && std::is_sorted(canonical.begin(), canonical.end()));
    assert(callback == collect(true, UINT64_MAX));
    auto sorted_callback = callback;
    std::sort(sorted_callback.begin(), sorted_callback.end());
    assert(std::adjacent_find(sorted_callback.begin(), sorted_callback.end()) ==
           sorted_callback.end());
    assert(sorted_callback == canonical);
    changed |= callback != canonical;
    assert(collect(true, 0).empty());
    assert(collect(false, 0).empty());
    const auto bounded = collect(true, 1);
    assert(bounded.size() == 1 && bounded.front() == callback.front());
    StopAfter stopping{0, 4};
    Permutations interrupted;
    assert(detect(graph, settings, stop_after, &stopping, interrupted) ==
           Status::Interrupted);
    assert(!interrupted.images && !interrupted.count);
    assert(callback == collect(true, UINT64_MAX));
    for (uint32_t prefix : {1u, 5u, 12u}) {
      settings.returned_prefix_size = prefix;
      for (bool preserve : {false, true}) {
        const auto projected = collect(preserve, UINT64_MAX);
        assert(projected == collect(preserve, UINT64_MAX));
        collect(preserve, 2);
      }
    }
  }
  assert(changed); // Negative control: ignoring the setting must fail.
}

void invalid_inputs_release_prior_results() {
  Permutations permutations;
  permutations.images = static_cast<uint32_t *>(std::malloc(sizeof(uint32_t)));
  permutations.count = 1;
  permutations.image_offsets = static_cast<uint64_t *>(std::malloc(2 * sizeof(uint64_t)));
  permutations.image_points = static_cast<uint32_t *>(std::malloc(sizeof(uint32_t)));
  permutations.image_entries = 1;
  DetectionSettings detection;
  assert(detect(Graph{}, detection, nullptr, nullptr, permutations) ==
         Status::Invalid);
  assert(!permutations.images && !permutations.count && !permutations.image_points &&
         !permutations.image_offsets && !permutations.image_entries);

  const uint32_t identity[] = {0, 1};
  const uint64_t bad_offsets[] = {1};
  const int8_t values[] = {0, 0};
  GroupInput invalid{2, 1, identity, 0, bad_offsets, 0, nullptr, values};
  Fixings fixings;
  fixings.kinds = static_cast<uint8_t *>(std::malloc(sizeof(uint8_t)));
  fixings.levels = static_cast<uint32_t *>(std::malloc(sizeof(uint32_t)));
  fixings.representatives =
      static_cast<uint32_t *>(std::malloc(sizeof(uint32_t)));
  fixings.others = static_cast<uint32_t *>(std::malloc(sizeof(uint32_t)));
  fixings.images = static_cast<uint32_t *>(std::malloc(sizeof(uint32_t)));
  fixings.count = 1;
  assert(fix_clauses(invalid, GroupSettings{}, nullptr, nullptr, fixings) ==
         Status::Invalid);
  assert(!fixings.kinds && !fixings.levels && !fixings.representatives &&
         !fixings.others && !fixings.images && !fixings.count);

  // Fix-and-cut interprets adjacent points as a literal/complement pair and
  // therefore rejects a domain that cannot encode complete signed variables.
  const uint32_t odd_identity[] = {0, 1, 2};
  const int8_t odd_values[] = {0, 0, 0};
  const GroupInput odd{3, 1, odd_identity, 0, nullptr, 0, nullptr, odd_values};
  GroupSettings fix_and_cut;
  fix_and_cut.chain_mode = 1;
  assert(fix_clauses(odd, fix_and_cut, nullptr, nullptr, fixings) ==
         Status::Invalid);
}

// DejaVu's partition refinement is defined only for simple graphs; a repeated
// edge corrupts its cell sizes (a SIGFPE in refine_coloring_first).
void parallel_edges_are_invalid() {
  const uint32_t colors[] = {0, 0, 0};
  const uint32_t degrees[] = {3, 2, 1};
  const uint32_t edges[] = {0, 1, 0, 2, 0, 1};
  const Graph graph{3, 3, colors, degrees, edges};
  DetectionSettings settings;
  settings.search_budget = 64;
  Permutations permutations;
  assert(detect(graph, settings, nullptr, nullptr, permutations) ==
         Status::Invalid);
  assert(!permutations.images && !permutations.count);

  RefinementSession *session = nullptr;
  uint64_t ticks = 0;
  assert(prepare_refinement(graph, 1'000, nullptr, nullptr, session, ticks) ==
         Status::Invalid);
  assert(!session);

  // Polls: preflight, vertex copy, edge copy, then the simple-graph scan. A
  // request raised at the scan's first poll wins over the repeated edge.
  StopAfter detecting{0, 4};
  assert(detect(graph, settings, stop_after, &detecting, permutations) ==
         Status::Interrupted);
  assert(detecting.calls == 4 && !permutations.images);
  StopAfter preparing{0, 4};
  assert(prepare_refinement(graph, 1'000, stop_after, &preparing, session,
                            ticks) == Status::Interrupted);
  assert(preparing.calls == 4 && !session);
}

// The owner's chain order (Satsuma negation_fixing_schreier, then fix_and_cut
// with find_non_trivial_orbit): negation orbits are fixed before any cut, and
// a cut takes the largest stabilizer orbit's highest variable.
void owner_chain_order_fixes_negations_first_and_cuts_the_last_largest_orbit() {
  constexpr uint32_t domain = 14; // variables 0..6
  std::vector<uint32_t> generators;
  const auto identity = [&] {
    const size_t offset = generators.size();
    for (uint32_t point = 0; point < domain; ++point)
      generators.push_back(point);
    return offset;
  };
  { // variables 0 and 1 are interchangeable (orbit size two)
    const size_t at = identity();
    for (uint32_t sign = 0; sign < 2; ++sign)
      std::swap(generators[at + 0 + sign], generators[at + 2 + sign]);
  }
  { // variable 3 maps to its own complement
    const size_t at = identity();
    std::swap(generators[at + 6], generators[at + 7]);
  }
  { // variables 4, 5, 6 rotate (the largest orbit)
    const size_t at = identity();
    for (uint32_t sign = 0; sign < 2; ++sign) {
      generators[at + 8 + sign] = 10 + sign;
      generators[at + 10 + sign] = 12 + sign;
      generators[at + 12 + sign] = 8 + sign;
    }
  }
  const uint64_t offsets[] = {0};
  const std::vector<int8_t> values(domain, 0);
  const GroupInput input{domain, generators.size() / domain, generators.data(),
                         0, offsets, 0, nullptr, values.data()};
  const auto run = [&](uint8_t cut_point, bool negation_first,
                       std::vector<std::pair<uint8_t, uint32_t>> &rows) {
    GroupSettings settings;
    settings.seed = 7;
    settings.chain_mode = 1;
    settings.cut_point = cut_point;
    settings.negation_fixing_first = negation_first;
    Fixings fixings;
    assert(fix_clauses(input, settings, nullptr, nullptr, fixings) ==
           Status::Complete);
    rows.clear();
    for (uint64_t row = 0; row < fixings.count; ++row)
      rows.emplace_back(fixings.kinds[row], fixings.representatives[row]);
    release(fixings);
  };
  std::vector<std::pair<uint8_t, uint32_t>> rows;
  run(0, false, rows);
  // Ascending order: the interchangeable pair is cut first, the negation
  // orbit is met only when its variable comes up.
  assert(!rows.empty() && rows.front() == std::make_pair(uint8_t{2}, 0u));
  assert(std::count(rows.begin(), rows.end(),
                    std::make_pair(uint8_t{3}, 6u)) == 1);

  // The owner's negation unit is the variable's negative literal.
  run(0, true, rows);
  assert(!rows.empty() && rows.front() == std::make_pair(uint8_t{3}, 7u));
  assert(rows.size() >= 2 && rows[1] == std::make_pair(uint8_t{2}, 0u));

  run(1, true, rows);
  assert(!rows.empty() && rows.front() == std::make_pair(uint8_t{3}, 7u));
  // Largest stabilizer orbit is {4,5,6}; its highest variable is cut first.
  assert(rows.size() >= 2 && rows[1] == std::make_pair(uint8_t{2}, 12u));

  GroupSettings invalid;
  invalid.chain_mode = 1;
  invalid.cut_point = 2;
  Fixings fixings;
  assert(fix_clauses(input, invalid, nullptr, nullptr, fixings) ==
         Status::Invalid);
}

// A later clausal fix reads the root values its earlier levels publish. The
// negation unit sets its base point true, so a clause holding that point is
// satisfied and only a clause holding its complement shrinks to the pair.
void negation_unit_value_matches_the_published_unit() {
  constexpr uint32_t domain = 8; // variables 0..3
  std::vector<uint32_t> generators;
  for (int generator = 0; generator < 2; ++generator) {
    const size_t at = generators.size();
    for (uint32_t point = 0; point < domain; ++point)
      generators.push_back(point);
    if (!generator)
      for (uint32_t sign = 0; sign < 2; ++sign)
        std::swap(generators[at + 0 + sign], generators[at + 2 + sign]);
    else
      std::swap(generators[at + 6], generators[at + 7]);
  }
  const std::vector<int8_t> values(domain, 0);
  const uint64_t offsets[] = {0, 3};
  const auto run = [&](uint32_t negation_literal, bool anchor = false,
                       bool negation_last = false) {
    uint32_t literals[] = {negation_literal, 0, 2};
    if (negation_last)
      std::swap(literals[0], literals[2]);
    const GroupInput input{domain, 2, generators.data(), 1, offsets, 3,
                           literals, values.data()};
    GroupSettings settings;
    settings.seed = 7;
    settings.chain_mode = 1;
    settings.negation_fixing_first = true;
    settings.first_literal_fix_anchor = anchor;
    Fixings fixings;
    assert(fix_clauses(input, settings, nullptr, nullptr, fixings) ==
           Status::Complete);
    std::vector<std::pair<uint8_t, uint32_t>> rows;
    for (uint64_t row = 0; row < fixings.count; ++row)
      rows.emplace_back(fixings.kinds[row], fixings.representatives[row]);
    release(fixings);
    return rows;
  };
  using Rows = std::vector<std::pair<uint8_t, uint32_t>>;
  // Satisfied by the unit: the pair is only cut.
  assert((run(7) == Rows{{3, 7}, {2, 0}}));
  // Falsified by the unit: the remaining pair is a fixable clause.
  assert((run(6) == Rows{{3, 7}, {0, 0}, {1, 0}}));
  // The owner's anchor is the first stored literal: false there, the clause
  // is left to the cut; stored last, the same clause still fixes. The fix
  // representative is the first open literal, here point 2.
  assert((run(6, true) == Rows{{3, 7}, {2, 0}}));
  assert((run(6, true, true) == Rows{{3, 7}, {0, 2}, {1, 2}}));
}

void clausal_base_extension_retains_known_generators() {
  constexpr uint32_t domain = 84;
  std::vector<uint32_t> generators;
  const auto swap_variables = [&](uint32_t first, uint32_t second) {
    const size_t offset = generators.size();
    for (uint32_t point = 0; point < domain; ++point)
      generators.push_back(point);
    for (uint32_t sign = 0; sign < 2; ++sign)
      std::swap(generators[offset + first + sign],
                generators[offset + second + sign]);
  };
  swap_variables(0, 2);
  for (uint32_t point = 4; point < domain - 2; point += 2)
    swap_variables(point, domain - 2);
  std::vector<uint32_t> literals{0, 2};
  for (uint32_t point = 4; point < domain; point += 2)
    literals.push_back(point);
  const uint64_t offsets[] = {0, 2, literals.size()};
  const std::vector<int8_t> values(domain, 0);
  const GroupInput input{domain, generators.size() / domain, generators.data(),
                        2, offsets, literals.size(), literals.data(), values.data()};
  for (uint32_t seed = 1; seed <= 32; ++seed) {
    GroupSettings settings;
    settings.seed = seed;
    settings.error_bound = 1;
    Fixings fixings;
    assert(fix_clauses(input, settings, nullptr, nullptr, fixings) == Status::Complete);
    assert(fixings.count == 42);
    // Independent S2 x S40 oracle: images are bijections preserving signs
    // and each component, and every advertised target is actually reached.
    for (uint64_t row = 0; row < fixings.count; ++row) {
      const uint32_t *image = fixings.images + row * domain;
      std::vector<bool> seen(domain);
      for (uint32_t point = 0; point < domain; ++point) {
        assert(image[point] < domain && !seen[image[point]]);
        seen[image[point]] = true;
        assert((point & 1u) == (image[point] & 1u));
        assert(image[point ^ 1u] == (image[point] ^ 1u));
        assert((point < 4) == (image[point] < 4));
      }
      if (fixings.kinds[row] == 0)
        assert(image[fixings.representatives[row]] == fixings.others[row]);
    }
    release(fixings);
  }
}

void clausal_transversals_close_alternating_generator_words() {
  constexpr uint32_t domain = 88;
  std::vector<uint32_t> generators;
  const auto add_swap = [&](uint32_t a, uint32_t b) {
    const size_t start = generators.size();
    for (uint32_t point = 0; point < domain; ++point)
      generators.push_back(point);
    for (uint32_t sign = 0; sign < 2; ++sign)
      std::swap(generators[start + a + sign], generators[start + b + sign]);
  };
  add_swap(0, 2);
  for (uint32_t sign = 0; sign < 2; ++sign)
    std::swap(generators[4 + sign], generators[6 + sign]);
  add_swap(2, 4);
  for (uint32_t point = 8; point < domain - 2; point += 2)
    add_swap(point, domain - 2);
  const uint32_t literals[] = {0, 2, 4, 6};
  const uint64_t offsets[] = {0, 4};
  const std::vector<int8_t> values(domain, 0);
  const GroupInput input{domain, generators.size() / domain, generators.data(),
                        1, offsets, 4, literals, values.data()};
  for (uint32_t seed = 1; seed <= 32; ++seed) {
    GroupSettings settings;
    settings.seed = seed;
    settings.error_bound = 1;
    Fixings fixings;
    assert(fix_clauses(input, settings, nullptr, nullptr, fixings) == Status::Complete);
    assert(fixings.count == 4);
    for (uint64_t row = 0; row < fixings.count; ++row) {
      const uint32_t *image = fixings.images + row * domain;
      std::vector<bool> seen(domain);
      for (uint32_t point = 0; point < domain; ++point) {
        assert(image[point] < domain && !seen[image[point]]);
        seen[image[point]] = true;
        assert((image[point] & 1u) == (point & 1u));
        assert(image[point ^ 1u] == (image[point] ^ 1u));
        assert((image[point] < 8) == (point < 8));
      }
      // The small component is exactly the square group: its permutations
      // preserve the opposite pairs {0,3} and {1,2}, not arbitrary S4 maps.
      for (uint32_t variable = 0; variable < 4; ++variable)
        assert(image[2 * (3 - variable)] / 2 == 3 - image[2 * variable] / 2);
      if (fixings.kinds[row] == 0)
        assert(image[fixings.representatives[row]] == fixings.others[row]);
    }
    release(fixings);
  }
}

void clausal_reverse_star_closes_known_targets() {
  constexpr uint32_t domain = 12;
  std::vector<uint32_t> generators;
  for (uint32_t variable = 5; variable-- > 0;) {
    const size_t start = generators.size();
    for (uint32_t point = 0; point < domain; ++point)
      generators.push_back(point);
    for (uint32_t sign = 0; sign < 2; ++sign)
      std::swap(generators[start + 2 * variable + sign],
                generators[start + 10 + sign]);
  }
  const uint32_t literals[] = {0, 2, 4, 6, 8, 10};
  const uint64_t offsets[] = {0, 6};
  const int8_t values[domain] = {};
  const GroupInput input{domain, 5, generators.data(), 1, offsets, 6, literals, values};
  GroupSettings settings;
  settings.seed = 3;
  settings.error_bound = 1;
  Fixings fixings;
  assert(fix_clauses(input, settings, nullptr, nullptr, fixings) == Status::Complete);
  assert(fixings.count == 6);
  for (uint64_t row = 0; row < fixings.count; ++row) {
    const uint32_t *image = fixings.images + row * domain;
    bool seen[domain] = {};
    for (uint32_t point = 0; point < domain; ++point) {
      assert(image[point] < domain && !seen[image[point]]);
      seen[image[point]] = true;
      assert((point & 1u) == (image[point] & 1u));
      assert(image[point ^ 1u] == (image[point] ^ 1u));
    }
    if (fixings.kinds[row] == 0)
      assert(image[fixings.representatives[row]] == fixings.others[row]);
  }
  const uint64_t ticks = fixings.ticks;
  release(fixings);

  // Stop/budget boundaries through the missing-target closure and witness
  // materialization may return only a complete level, never partial cuts.
  for (uint64_t budget = 0; budget < ticks; ++budget) {
    settings.maximum_ticks = budget;
    assert(fix_clauses(input, settings, nullptr, nullptr, fixings) == Status::Incomplete);
    assert((fixings.count == 0 || fixings.count == 6) && fixings.ticks <= budget);
    if (fixings.count)
      assert(fixings.kinds[5] == 1 && fixings.levels[5] == 0);
    else
      assert(!fixings.images);
    release(fixings);
  }
  settings.maximum_ticks = UINT64_MAX;
  StopAfter count{0, UINT32_MAX};
  assert(fix_clauses(input, settings, stop_after, &count, fixings) == Status::Complete);
  release(fixings);
  for (unsigned at = 1; at <= count.calls; ++at) {
    StopAfter stop{0, at};
    assert(fix_clauses(input, settings, stop_after, &stop, fixings) == Status::Interrupted);
    assert(!fixings.count && !fixings.images);
    release(fixings);
  }
}

void complete_permutations_produce_full_fixings() {
  const uint32_t transposition[] = {1, 0};
  const uint64_t offsets[] = {0, 2};
  const uint32_t literals[] = {0, 1};
  const int8_t values[] = {0, 0};
  const GroupInput input{2, 1, transposition, 1, offsets, 2, literals, values};
  GroupSettings settings;
  settings.maximum_instances = 2;
  settings.maximum_ticks = 100000;
  Fixings fixings;
  assert(fix_clauses(input, settings, nullptr, nullptr, fixings) ==
         Status::Complete);
  assert(fixings.domain_size == 2 && fixings.count == 2);
  assert(fixings.levels[0] == 0 && fixings.levels[1] == 0);
  assert(fixings.kinds[0] == 0 && fixings.kinds[1] == 1);
  assert(fixings.representatives[0] < 2 && fixings.others[0] < 2);
  assert(fixings.representatives[0] != fixings.others[0]);
  assert(fixings.images[fixings.representatives[0]] == fixings.others[0]);
  assert(fixings.representatives[1] == fixings.representatives[0] &&
         fixings.others[1] == UINT32_MAX);
  assert(fixings.images[2] == 0 && fixings.images[3] == 1);
  release(fixings);

  // A budget too small for the whole level returns no partial action prefix.
  settings.maximum_instances = 1;
  assert(fix_clauses(input, settings, nullptr, nullptr, fixings) ==
         Status::Incomplete);
  assert(!fixings.kinds && !fixings.levels && !fixings.representatives &&
         !fixings.others && !fixings.images && !fixings.count);
  settings.maximum_instances = 2;

  StopAfter stopping{0, 5};
  assert(fix_clauses(input, settings, stop_after, &stopping, fixings) ==
         Status::Interrupted);
  assert(stopping.calls >= 5 && !fixings.kinds && !fixings.levels &&
         !fixings.representatives && !fixings.others && !fixings.images);
}

void first_amo_selection_preserves_caller_order() {
  // Two independent orbits: a three-cycle and a two-cycle. Both clauses
  // are AMOs; the first clause has the larger orbit. Global orbit ranking
  // and first-AMO selection must therefore choose different first bases.
  const uint32_t images[] = {2,3,4,5,0,1,6,7,8,9,
                             0,1,2,3,4,5,8,9,6,7};
  const int8_t values[10] = {};
  const uint64_t offsets[] = {0,3,5};
  const uint32_t literals[] = {0,2,4,6,8};
  const uint8_t amos[] = {1,1};
  const GroupInput input{10,2,images,2,offsets,5,literals,values,amos};
  for (const uint8_t mode : {uint8_t{1}, uint8_t{2}, uint8_t{3}, uint8_t{4}}) {
    GroupSettings settings;
    settings.chain_mode = mode == 5 ? 2 : 1;
    settings.base_selection = mode == 5 ? 4 : mode;
    Fixings fixings;
    assert(fix_clauses(input, settings, nullptr, nullptr, fixings) == Status::Complete);
    assert(fixings.count && fixings.levels[0] == 0);
    assert(fixings.representatives[0] == (mode == 1 ? 6u : 0u));
    release(fixings);
  }
  const uint64_t reversed_offsets[] = {0,2,5};
  const uint32_t reversed_literals[] = {6,8,0,2,4};
  const GroupInput reversed{10,2,images,2,reversed_offsets,5,
                            reversed_literals,values,amos};
  GroupSettings settings;
  settings.chain_mode = 1;
  settings.base_selection = 2;
  Fixings fixings;
  assert(fix_clauses(reversed, settings, nullptr, nullptr, fixings) == Status::Complete);
  assert(fixings.count && fixings.representatives[0] == 6u);
  release(fixings);
}

void shortest_clause_precedes_long_amo() {
  const uint32_t images[] = {2,3,4,5,0,1,6,7,8,9,
                             0,1,2,3,4,5,8,9,6,7};
  const int8_t values[10] = {};
  const uint64_t offsets[] = {0,3,5};
  const uint32_t literals[] = {0,2,4,6,8};
  const uint8_t amos[] = {1,0};
  const GroupInput input{10,2,images,2,offsets,5,literals,values,amos};
  for (const uint8_t chain : {uint8_t{1}, uint8_t{2}}) {
    for (const uint8_t mode : {uint8_t{4}, uint8_t{5}, uint8_t{6}}) {
      GroupSettings settings;
      settings.chain_mode = chain;
      settings.base_selection = mode;
      Fixings fixings;
      assert(fix_clauses(input, settings, nullptr, nullptr, fixings) == Status::Complete);
      assert(fixings.count && fixings.levels[0] == 0);
      assert(fixings.representatives[0] == (mode >= 5 ? 6u : 0u));
      release(fixings);
      settings.maximum_instances = 1;
      assert(fix_clauses(input, settings, nullptr, nullptr, fixings) == Status::Incomplete);
      assert(!fixings.count && !fixings.images);
      release(fixings);
    }
  }
}

void first_shortest_retains_forward_remainder() {
  // Independent cyclic orbits: a long AMO, then short and medium clauses.
  // The initial shortest choice must not hide the earlier AMO from the
  // remainder. Repeated shortest ranking instead chooses the medium next.
  const unsigned sizes[] = {4, 2, 3};
  std::vector<uint32_t> images;
  unsigned first = 0;
  for (const unsigned size : sizes) {
    const size_t offset = images.size();
    for (uint32_t literal = 0; literal < 18; ++literal)
      images.push_back(literal);
    for (unsigned variable = 0; variable < size; ++variable)
      for (unsigned sign = 0; sign < 2; ++sign)
        images[offset + 2 * (first + variable) + sign] =
            2 * (first + (variable + 1) % size) + sign;
    first += size;
  }
  const uint64_t offsets[] = {0, 4, 6, 9};
  const uint32_t literals[] = {0, 2, 4, 6, 8, 10, 12, 14, 16};
  const int8_t values[18] = {};
  const uint8_t amos[] = {1, 0, 0};
  const GroupInput input{18, 3, images.data(), 3, offsets, 9, literals, values, amos};
  for (uint8_t chain : {uint8_t{1}, uint8_t{2}})
    for (uint8_t mode : {uint8_t{4}, uint8_t{5}, uint8_t{6}}) {
      GroupSettings settings;
      settings.chain_mode = chain;
      settings.base_selection = mode;
      Fixings fixings;
      assert(fix_clauses(input, settings, nullptr, nullptr, fixings) == Status::Complete);
      std::vector<uint32_t> bases;
      for (uint64_t row = 0; row < fixings.count; ++row)
        if (fixings.levels[row] == bases.size())
          bases.push_back(fixings.representatives[row]);
      const std::vector<uint32_t> expected = mode == 4
          ? std::vector<uint32_t>{0, 8, 12}
          : mode == 5 ? std::vector<uint32_t>{8, 12, 0}
                      : std::vector<uint32_t>{8, 0, 12};
      assert(bases == expected);
      release(fixings);
    }
}

void ordered_remainder_starts_after_first_amo_free_scan() {
  // Independent cyclic orbits of sizes 2,4,2,3. The first is the sole AMO.
  // The first fallback chooses the smaller third orbit; later caller order
  // chooses the size-four second orbit before the size-three fourth orbit.
  const unsigned sizes[] = {2,4,2,3};
  std::vector<uint32_t> images;
  unsigned first = 0;
  for (const unsigned size : sizes) {
    const size_t offset = images.size();
    for (uint32_t literal = 0; literal < 22; ++literal)
      images.push_back(literal);
    for (unsigned variable = 0; variable < size; ++variable)
      for (unsigned sign = 0; sign < 2; ++sign)
        images[offset + 2*(first+variable)+sign] =
            2*(first+(variable+1)%size)+sign;
    first += size;
  }
  const uint64_t offsets[] = {0,2,6,8,11};
  const uint32_t literals[] = {0,2,4,6,8,10,12,14,16,18,20};
  const int8_t values[22] = {};
  const uint8_t amos[] = {1,0,0,0};
  const GroupInput input{22,4,images.data(),4,offsets,11,literals,values,amos};
  for (const uint8_t mode : {uint8_t{2},uint8_t{3},uint8_t{4}}) {
    GroupSettings settings;
    settings.chain_mode = mode == 5 ? 2 : 1;
    settings.base_selection = mode == 5 ? 4 : mode;
    Fixings fixings;
    assert(fix_clauses(input, settings, nullptr, nullptr, fixings) == Status::Complete);
    std::vector<uint32_t> bases;
    for (uint64_t row = 0; row < fixings.count; ++row)
      if (fixings.levels[row] == bases.size())
        bases.push_back(fixings.representatives[row]);
    const std::vector<uint32_t> expected = mode == 2
        ? std::vector<uint32_t>{0,12,16,4}
        : std::vector<uint32_t>{0,12,4,16};
    assert(bases == expected);
    release(fixings);
  }
}

void forward_scan_keeps_a_monotone_clause_frontier() {
  // Independent interchangeable pairs expose a partially built stabilizer.
  // A full rescan may revisit an earlier clause after a random sift reveals
  // its orbit. Forward scanning must keep completed clause choices monotone.
  constexpr uint32_t pairs = 450, domain = 4 * pairs;
  std::vector<uint32_t> images, literals;
  std::vector<uint64_t> offsets{0};
  std::vector<int8_t> values(domain, 0);
  for (uint32_t pair = 0; pair < pairs; ++pair) {
    const size_t start = images.size();
    for (uint32_t point = 0; point < domain; ++point)
      images.push_back(point);
    for (uint32_t point = 4 * pair; point < 4 * pair + 4; ++point)
      images[start + point] = point ^ 2u;
    literals.push_back(4 * pair);
    literals.push_back(4 * pair + 2);
    offsets.push_back(literals.size());
  }
  // Lexicographic permutation order puts changes to later variables first,
  // matching the native producer's canonical generator traversal.
  for (uint32_t row = 0; row < pairs / 2; ++row)
    std::swap_ranges(images.begin() + row * domain,
                     images.begin() + (row + 1) * domain,
                     images.begin() + (pairs - 1 - row) * domain);
  const GroupInput input{domain, pairs, images.data(), pairs, offsets.data(),
                          literals.size(), literals.data(), values.data()};
  std::vector<uint32_t> units[3];
  for (uint8_t mode = 3; mode <= 5; ++mode) {
    GroupSettings settings;
    settings.chain_mode = mode == 5 ? 2 : 1;
    settings.base_selection = mode == 5 ? 4 : mode;
    Fixings fixings;
    assert(fix_clauses(input, settings, nullptr, nullptr, fixings) == Status::Complete);
    for (uint64_t row = 0; row < fixings.count; ++row) {
      if (fixings.kinds[row] == 1)
        units[mode - 3].push_back(fixings.representatives[row]);
      const uint32_t *image = fixings.images + row * domain;
      for (uint32_t point = 0; point < domain; ++point) {
        assert(image[point] < domain);
        assert(image[point ^ 1u] == (image[point] ^ 1u));
      }
    }
    release(fixings);
    StopAfter interrupt{0, 10};
    assert(fix_clauses(input, settings, stop_after, &interrupt, fixings) ==
           Status::Interrupted);
    assert(!fixings.count && !fixings.images);
  }
  assert(!units[0].empty() && !units[1].empty());
  assert(!std::is_sorted(units[0].begin(), units[0].end()));
  assert(std::is_sorted(units[1].begin(), units[1].end()));
  assert(units[0] != units[1]);
  // Retaining the known independent generators closes every pair without
  // waiting for a later random sample to rediscover its stabilizer orbit.
  assert(units[2].size() == pairs);
  for (uint32_t pair = 0; pair < pairs; ++pair)
    assert(units[2][pair] == 4 * pair);
}

void fix_and_cut_emits_complete_orbit_and_negation_cuts() {
  for (uint8_t chain_mode : {uint8_t{1}, uint8_t{2}}) {
  const uint64_t offsets[] = {0};
  {
    // Swap two variables while preserving polarity. With no fixable clause,
    // the complete chain emits the single binary orbit cut.
    const uint32_t swap_variables[] = {2, 3, 0, 1};
    const int8_t values[] = {0, 0, 0, 0};
    const GroupInput input{4, 1, swap_variables, 0, offsets, 0, nullptr,
                           values};
    GroupSettings settings;
    settings.maximum_instances = 4;
    settings.maximum_ticks = 100000;
    settings.chain_mode = chain_mode;
    settings.base_selection = 1;
    Fixings cuts;
    assert(fix_clauses(input, settings, nullptr, nullptr, cuts) ==
           Status::Complete);
    assert(cuts.count == 1 && cuts.kinds[0] == 2 && cuts.levels[0] == 0);
    const uint32_t representative = cuts.representatives[0];
    const uint32_t other = cuts.others[0];
    assert((representative & 1u) == 0 && other != (representative ^ 1u));
    assert(cuts.images[representative] == other);
    release(cuts);

    settings.maximum_instances = 0;
    assert(fix_clauses(input, settings, nullptr, nullptr, cuts) ==
           Status::Incomplete);
    assert(!cuts.count && !cuts.kinds && !cuts.levels && !cuts.images);
  }
  {
    // An order-three variable cycle distinguishes the required inverse
    // transversal direction from the backend's fixing automorphism.
    const uint32_t cycle_variables[] = {2, 3, 4, 5, 0, 1};
    const int8_t values[] = {0, 0, 0, 0, 0, 0};
    const GroupInput input{6, 1, cycle_variables, 0, offsets, 0, nullptr,
                           values};
    GroupSettings settings;
    settings.maximum_instances = 2;
    settings.maximum_ticks = 100000;
    settings.chain_mode = chain_mode;
    Fixings cuts;
    assert(fix_clauses(input, settings, nullptr, nullptr, cuts) ==
           Status::Complete);
    assert(cuts.count == 2 && cuts.kinds[0] == 2 && cuts.kinds[1] == 2 &&
           cuts.levels[0] == 0 && cuts.levels[1] == 0);
    for (uint64_t row = 0; row < cuts.count; ++row) {
      const uint32_t representative = cuts.representatives[row];
      const uint32_t other = cuts.others[row];
      const uint32_t *image = cuts.images + row * cuts.domain_size;
      assert(image[representative] == other && image[other] != representative);
    }
    release(cuts);
  }
  {
    // A sign-flip orbit contains the complement of its representative and is
    // therefore closed by one unit rather than contradictory binary cuts.
    const uint32_t flip_sign[] = {1, 0};
    const int8_t values[] = {0, 0};
    const GroupInput input{2, 1, flip_sign, 0, offsets, 0, nullptr, values};
    GroupSettings settings;
    settings.maximum_instances = 1;
    settings.maximum_ticks = 100000;
    settings.chain_mode = chain_mode;
    Fixings cuts;
    assert(fix_clauses(input, settings, nullptr, nullptr, cuts) ==
           Status::Complete);
    assert(cuts.count == 1 && cuts.kinds[0] == 3 && cuts.levels[0] == 0);
    assert(cuts.others[0] == (cuts.representatives[0] ^ 1u));
    assert(cuts.images[cuts.representatives[0]] == cuts.others[0]);
    release(cuts);
  }
  }
}

void planning_limit_preserves_a_complete_prefix() {
  for (uint8_t chain_mode : {uint8_t{1}, uint8_t{2}}) {
  // Three independent signed flips need three stabilizer levels. A planning
  // threshold can stop between them while finalization still certifies a
  // nonempty prefix under the independent hard work budget.
  const uint32_t generators[] = {1, 0, 2, 3, 4, 5,
                                 0, 1, 3, 2, 4, 5,
                                 0, 1, 2, 3, 5, 4};
  const uint64_t offsets[] = {0};
  const int8_t values[] = {0, 0, 0, 0, 0, 0};
  const GroupInput input{6, 3, generators, 0, offsets, 0, nullptr, values};
  GroupSettings settings;
  settings.chain_mode = chain_mode;
  Fixings full;
  assert(fix_clauses(input, settings, nullptr, nullptr, full) == Status::Complete);
  assert(full.count == 3);
  const uint64_t full_ticks = full.ticks;
  release(full);
  bool reached = false;
  for (uint64_t limit = 1; limit < full_ticks && !reached; ++limit) {
    settings.planning_tick_limit = limit;
    Fixings prefix;
    const Status status = fix_clauses(input, settings, nullptr, nullptr, prefix);
    if (prefix.count && prefix.count < 3) {
      assert(status == Status::Incomplete);
      // Finalization consumes work beyond the soft planning threshold.
      assert(prefix.ticks > limit);
      for (uint64_t i = 0; i < prefix.count; ++i) {
        assert(prefix.kinds[i] == 3);
        const uint32_t r = prefix.representatives[i];
        const uint32_t *image = prefix.images + i * 6;
        assert(image[r] == (r ^ 1u) && image[r ^ 1u] == r);
        for (uint64_t j = 0; j < i; ++j)
          assert(image[prefix.representatives[j]] == prefix.representatives[j]);
      }
      reached = true;
    }
    release(prefix);
  }
  assert(reached);
  settings.maximum_ticks = 0;
  Fixings stopped;
  assert(fix_clauses(input, settings, nullptr, nullptr, stopped) == Status::Incomplete);
  assert(!stopped.count && !stopped.images);
  settings.maximum_ticks = UINT64_MAX;
  StopAfter interrupt{0, 10};
  assert(fix_clauses(input, settings, stop_after, &interrupt, stopped) == Status::Interrupted);
  assert(!stopped.count && !stopped.images);
  }
}

void bounded_resifting_preserves_overlapping_actions() {
  constexpr uint32_t variables = 7, domain = 2 * variables;
  for (bool reverse : {false, true}) {
    std::vector<uint32_t> images;
    for (uint32_t step = 0; step < variables; ++step) {
      const size_t offset = images.size();
      for (uint32_t point = 0; point < domain; ++point)
        images.push_back(point);
      if (step < variables - 1) {
        const uint32_t v = reverse ? variables - 2 - step : step;
        for (uint32_t sign = 0; sign < 2; ++sign) {
          images[offset + 2 * v + sign] = 2 * (v + 1) + sign;
          images[offset + 2 * (v + 1) + sign] = 2 * v + sign;
        }
      } else {
        for (uint32_t v = 0; v < 3; ++v)
          for (uint32_t sign = 0; sign < 2; ++sign)
            images[offset + 2 * v + sign] = 2 * ((v + 1) % 3) + sign;
      }
    }
    const uint64_t offsets[] = {0};
    std::vector<int8_t> values(domain, 0);
    const GroupInput input{domain, variables, images.data(), 0, offsets, 0,
                            nullptr, values.data()};
    GroupSettings settings;
    settings.chain_mode = 2;
    Fixings cuts;
    assert(fix_clauses(input, settings, nullptr, nullptr, cuts) == Status::Complete);
    assert(cuts.count == variables * (variables - 1) / 2);
    for (uint64_t row = 0; row < cuts.count; ++row) {
      assert(cuts.kinds[row] == 2);
      const uint32_t *image = cuts.images + row * domain;
      assert(image[cuts.representatives[row]] == cuts.others[row]);
      std::vector<bool> seen(domain, false);
      for (uint32_t point = 0; point < domain; ++point) {
        assert(image[point] < domain && !seen[image[point]]);
        seen[image[point]] = true;
        assert((image[point] & 1u) == (point & 1u));
        assert(image[point ^ 1u] == (image[point] ^ 1u));
      }
      for (uint64_t prior = 0; prior < row; ++prior)
        if (cuts.levels[prior] < cuts.levels[row])
          assert(image[cuts.representatives[prior]] == cuts.representatives[prior]);
    }
    release(cuts);
  }
}

void individualized_refinement_aligns_row_companions() {
  const uint32_t colors[] = {0, 0, 0, 0};
  const uint32_t degrees[] = {1, 1, 1, 1};
  const uint32_t edges[] = {0, 2, 1, 3};
  const uint32_t anchors[] = {0, 1};
  const Graph graph{4, 2, colors, degrees, edges};
  RefinementRows rows;
  assert(refine_rows(graph, anchors, 2, 4, 1000, nullptr, nullptr, rows) ==
         Status::Complete);
  assert(rows.anchor_count == 2 && rows.domain_size == 4 && rows.images);
  // Individualizing either anchor exposes its singleton component and an
  // equal-sized remainder block.  Retaining both yields the complete exact
  // transposition between the two disjoint edges.
  const uint32_t first_row[] = {0, 1, 2, 3};
  const auto exact_edge_action = [](const uint32_t *image) {
    bool seen[4] = {};
    for (unsigned vertex = 0; vertex < 4; ++vertex) {
      assert(image[vertex] < 4 && !seen[image[vertex]]);
      seen[image[vertex]] = true;
    }
    const auto edge = [](uint32_t a, uint32_t b) {
      if (a > b)
        std::swap(a, b);
      return (a == 0 && b == 2) || (a == 1 && b == 3);
    };
    assert(edge(image[0], image[2]) && edge(image[1], image[3]));
  };
  assert(std::equal(rows.images, rows.images + 4, first_row));
  assert(rows.images[4] == 1);
  exact_edge_action(rows.images + 4);
  release(rows);

  assert(refine_rows(graph, anchors, 2, 4, 0, nullptr, nullptr, rows) ==
         Status::Incomplete);
  assert(!rows.images && rows.ticks == 0);

  StopAfter stopping{0, 3};
  assert(refine_rows(graph, anchors, 2, 4, 1000, stop_after, &stopping,
                     rows) == Status::Interrupted);
  assert(stopping.calls >= 3 && !rows.images);
}

void prepared_refinement_is_reusable_and_owned() {
  const uint32_t colors[] = {0, 0, 0, 0};
  const uint32_t degrees[] = {1, 1, 1, 1};
  const uint32_t edges[] = {0, 2, 1, 3};
  const Graph graph{4, 2, colors, degrees, edges};
  RefinementSession *session = nullptr;
  uint64_t preparation_ticks = 0;
  assert(prepare_refinement(graph, 1000, nullptr, nullptr, session,
                            preparation_ticks) == Status::Complete);
  assert(session);

  const uint32_t forward[] = {0, 1};
  RefinementRows rows;
  assert(refine_rows(*session, forward, 2, 4, 1000, nullptr, nullptr,
                     rows) == Status::Complete);
  const uint32_t first_forward[] = {0, 1, 2, 3};
  assert(std::equal(rows.images, rows.images + 4, first_forward));
  assert(rows.images[4] == 1);
  release(rows);

  const uint32_t reverse[] = {1, 0};
  assert(refine_rows(*session, reverse, 2, 4, 1000, nullptr, nullptr,
                     rows) == Status::Complete);
  const uint32_t first_reverse[] = {0, 1, 2, 3};
  assert(std::equal(rows.images, rows.images + 4, first_reverse));
  assert(rows.images[5] == 0);
  release(rows);
  release(session);
  assert(!session);
  release(session);

  preparation_ticks = UINT64_MAX;
  assert(prepare_refinement(graph, 0, nullptr, nullptr, session,
                            preparation_ticks) == Status::Incomplete);
  assert(!session && preparation_ticks == 0);
}

void invalid_orbit_vertex_is_rejected() {
  const uint32_t colors[] = {0, 0, 0, 0};
  const uint32_t degrees[] = {2, 2, 2, 2};
  const uint32_t edges[] = {0, 1, 1, 2, 2, 3, 0, 3};
  const Graph graph{4, 4, colors, degrees, edges};
  RefinementSession *session = nullptr;
  uint64_t ticks = 0;
  assert(prepare_refinement(graph, 1000, nullptr, nullptr, session, ticks) ==
         Status::Complete);
  for (const auto vertices : {std::vector<uint32_t>{UINT32_MAX, 0},
                             std::vector<uint32_t>{4, 0},
                             std::vector<uint32_t>{0, UINT32_MAX}}) {
    RefinementOrbit result;
    assert(reduce_orbit(*session, vertices.data(), vertices.size(), 4, 1000,
                       nullptr, nullptr, result) == Status::Invalid);
    assert(!result.vertices && !result.count);
    release(result);
  }
  release(session);
}

void individualized_orbit_reduction_selects_largest_remainder() {
  const uint32_t colors[] = {0, 0, 0, 0};
  const uint32_t degrees[] = {2, 2, 2, 2};
  const uint32_t edges[] = {0, 1, 1, 2, 2, 3, 0, 3};
  const Graph graph{4, 4, colors, degrees, edges};
  RefinementSession *session = nullptr;
  uint64_t preparation_ticks = 0;
  assert(prepare_refinement(graph, 1000, nullptr, nullptr, session,
                            preparation_ticks) == Status::Complete);
  const uint32_t candidates[] = {0, 1, 2, 3};
  RefinementOrbit reduced;
  assert(reduce_orbit(*session, candidates, 4, 4, 1000, nullptr, nullptr,
                      reduced) == Status::Complete);
  assert(reduced.count == 3 && reduced.vertices[0] == 0);
  std::sort(reduced.vertices + 1, reduced.vertices + reduced.count);
  assert(reduced.vertices[1] == 1 && reduced.vertices[2] == 3);
  release(reduced);
  assert(!reduced.vertices && !reduced.count);

  assert(reduce_orbit(*session, candidates, 4, 4, 0, nullptr, nullptr,
                      reduced) == Status::Incomplete);
  assert(!reduced.vertices && reduced.ticks == 0);
  release(session);
}

void individualized_partition_exposes_product_remainders() {
  // A 3x3 incidence grid with differently colored row and column markers.
  // Individualizing cell (0,0) separates the other cells into the expected
  // product-action remainders: 1, 2, 2 and 4.
  uint32_t colors[15] = {};
  uint32_t degrees[15] = {};
  for (uint32_t marker = 9; marker < 12; ++marker)
    colors[marker] = 1;
  for (uint32_t marker = 12; marker < 15; ++marker)
    colors[marker] = 2;
  uint32_t edges[36];
  size_t at = 0;
  for (uint32_t row = 0; row < 3; ++row)
    for (uint32_t column = 0; column < 3; ++column) {
      const uint32_t cell = 3 * row + column;
      edges[at++] = cell;
      edges[at++] = 9 + row;
      edges[at++] = cell;
      edges[at++] = 12 + column;
      degrees[cell] += 2;
      ++degrees[9 + row];
      ++degrees[12 + column];
    }
  const Graph graph{15, 18, colors, degrees, edges};
  RefinementSession *session = nullptr;
  uint64_t preparation_ticks = 0;
  assert(prepare_refinement(graph, 10000, nullptr, nullptr, session,
                            preparation_ticks) == Status::Complete);
  IndividualizedPartition partition;
  assert(individualized_partition(*session, 0, 9, 10000, nullptr, nullptr,
                                  partition) == Status::Complete);
  assert(partition.domain_size == 9 && partition.cells && partition.sizes);
  std::vector<uint32_t> sizes(partition.sizes, partition.sizes + 9);
  std::sort(sizes.begin(), sizes.end());
  const std::vector<uint32_t> expected = {1, 2, 2, 2, 2, 4, 4, 4, 4};
  assert(sizes == expected);
  assert(partition.sizes[0] == 1);
  release(partition);
  assert(!partition.cells && !partition.sizes);

  const uint32_t anchors[] = {0, 4};
  assert(individualized_partition(*session, anchors, 2, 9, 10000, nullptr,
                                  nullptr, partition) == Status::Complete);
  assert(partition.domain_size == 9 && partition.cells && partition.sizes &&
         partition.sizes[0] == 1 && partition.sizes[4] == 1);
  release(partition);

  RefinementBranch *branch = nullptr;
  assert(prepare_refinement_branch(*session, branch) == Status::Complete);
  assert(branch);
  assert(individualized_partition(*branch, 0, 9, 10000, nullptr, nullptr,
                                  partition) == Status::Complete);
  assert(partition.sizes[0] == 1 && partition.sizes[4] == 4);
  release(partition);
  assert(individualized_partition(*branch, 4, 9, 10000, nullptr, nullptr,
                                  partition) == Status::Complete);
  assert(partition.sizes[0] == 1 && partition.sizes[4] == 1);
  release(partition);
  release(branch);
  assert(!branch);

  assert(individualized_partition(*session, 0, 9, 0, nullptr, nullptr,
                                  partition) == Status::Incomplete);
  assert(!partition.cells && !partition.sizes && partition.ticks == 0);
  release(session);
}

void support_components_preserve_group_and_clause_boundaries() {
  constexpr uint32_t domain = 12;
  std::vector<uint32_t> generators(2 * domain);
  for (uint32_t g = 0; g < 2; ++g)
    for (uint32_t p = 0; p < domain; ++p)
      generators[g * domain + p] = p >= 4 * g && p < 4 * g + 4 ? p ^ 2u : p;
  int8_t values[domain] = {};
  const uint64_t offsets[] = {0, 2, 4};
  const uint32_t literals[] = {1, 3, 5, 7};
  GroupInput input{domain, 2, generators.data(), 2, offsets, 4, literals, values};
  for (uint8_t mode = 0; mode < 3; ++mode) {
    GroupSettings settings;
    settings.chain_mode = mode;
    settings.factor_support_components = true;
    Fixings fixed;
    assert(fix_clauses(input, settings, nullptr, nullptr, fixed) == Status::Complete);
    assert(fixed.count == 4 && fixed.domain_size == domain);
    for (uint64_t row = 0; row < fixed.count; ++row) {
      std::vector<uint32_t> materialized(domain);
      assert(materialize_fixing_image(fixed, row, materialized.data()));
      const auto *map = materialized.data();
      bool member = false;
      // Exhaust the actual four-element input subgroup, independently of DejaVu.
      for (uint32_t choice = 0; choice < 4; ++choice) {
        bool equal = true;
        for (uint32_t p = 0; p < domain; ++p) {
          uint32_t image = p;
          for (uint32_t g = 0; g < 2; ++g)
            if (choice & (1u << g)) image = generators[g * domain + image];
          equal &= map[p] == image;
        }
        member |= equal;
      }
      assert(member);
      for (uint32_t p = 0; p < domain; ++p) assert(map[p ^ 1u] == (map[p] ^ 1u));
      if (fixed.kinds[row] != 1)
        assert(map[fixed.representatives[row]] == fixed.others[row]);
      for (uint64_t prior = 0; prior < row; ++prior)
        if (fixed.levels[prior] < fixed.levels[row])
          assert(map[fixed.representatives[prior]] == fixed.representatives[prior]);
    }
    assert(fixed.representatives[0] == 1 && fixed.representatives[2] == 5);
    const uint64_t total = fixed.ticks;
    release(fixed);
    bool saw_prefix = false;
    for (uint64_t budget = 0; budget < total; budget += std::max(uint64_t{1}, total / 30)) {
      settings.maximum_ticks = budget;
      assert(fix_clauses(input, settings, nullptr, nullptr, fixed) == Status::Incomplete);
      assert(fixed.ticks <= budget && fixed.count < 4 && !(fixed.count % 2));
      saw_prefix |= fixed.count == 2;
      release(fixed);
    }
    assert(saw_prefix);
    settings.maximum_ticks = UINT64_MAX;
    settings.maximum_instances = 2;
    assert(fix_clauses(input, settings, nullptr, nullptr, fixed) == Status::Incomplete);
    assert(fixed.count == 2 && fixed.kinds[1] == 1);
    release(fixed);
    settings.maximum_instances = UINT64_MAX;
    if (mode) {
      settings.planning_tick_limit = 1;
      assert(fix_clauses(input, settings, nullptr, nullptr, fixed) == Status::Incomplete);
      assert(!fixed.count);
      release(fixed);
      settings.planning_tick_limit = 0;
    }
    // Exercise cancellation inside decomposition, child planning and export.
    StopAfter count{0, UINT32_MAX};
    assert(fix_clauses(input, settings, stop_after, &count, fixed) == Status::Complete);
    release(fixed);
    for (unsigned at : {1u, count.calls / 2, count.calls - 1}) {
      StopAfter stopping{0, at};
      assert(fix_clauses(input, settings, stop_after, &stopping, fixed) == Status::Interrupted);
      assert(!fixed.count && !fixed.images);
    }
  }

  GroupSettings settings;
  settings.factor_support_components = true;
  const uint64_t cross_offsets[] = {0, 3, 7};
  const uint32_t cross_literals[] = {1, 3, 9, 1, 3, 5, 7};
  input.clause_offsets = cross_offsets;
  input.literals = cross_literals;
  input.literal_count = 7;
  Fixings fixed;
  assert(fix_clauses(input, settings, nullptr, nullptr, fixed) == Status::Complete);
  assert(!fixed.count); // Neither unknown fixed points nor mixed components disappear.
  release(fixed);
  values[9] = -1;
  values[8] = 1;
  assert(fix_clauses(input, settings, nullptr, nullptr, fixed) == Status::Complete);
  assert(fixed.count == 2 && fixed.representatives[0] == 1 && fixed.kinds[1] == 1);
  release(fixed);
  values[9] = 1;
  values[8] = -1;
  assert(fix_clauses(input, settings, nullptr, nullptr, fixed) == Status::Complete);
  assert(!fixed.count); // A satisfied clause cannot justify fixing.
  release(fixed);

  // Coupled cycles form a single support component. Its complete output must
  // equal the unfactored path; projecting its cycles would invent generators.
  for (uint32_t p = 4; p < 8; ++p) generators[p] = p ^ 2u;
  input.generator_count = 1;
  input.clause_offsets = offsets;
  input.literals = literals;
  input.literal_count = 4;
  for (uint8_t mode = 0; mode < 3; ++mode) {
    settings.chain_mode = mode;
    Fixings reference;
    settings.factor_support_components = false;
    assert(fix_clauses(input, settings, nullptr, nullptr, reference) == Status::Complete);
    settings.factor_support_components = true;
    assert(fix_clauses(input, settings, nullptr, nullptr, fixed) == Status::Complete);
    assert(fixed.count == reference.count);
    for (uint64_t row = 0; row < fixed.count; ++row) {
      assert(fixed.kinds[row] == reference.kinds[row]);
      assert(fixed.levels[row] == reference.levels[row]);
      assert(fixed.representatives[row] == reference.representatives[row]);
      assert(fixed.others[row] == reference.others[row]);
    }
    assert(std::equal(fixed.images, fixed.images + fixed.count * domain, reference.images));
    release(fixed);
    release(reference);
  }
}

void compact_component_images_scale_with_support() {
  const uint32_t domain = 20000;
  std::vector<uint32_t> generators(2 * domain);
  for (uint32_t p = 0; p < domain; ++p) {
    generators[p] = p < 4 ? p ^ 2u : p;
    generators[domain + p] = p >= 4 && p < 8 ? p ^ 2u : p;
  }
  std::vector<int8_t> values(domain, 0);
  const uint64_t offsets[] = {0, 2, 4};
  const uint32_t literals[] = {1, 3, 5, 7};
  GroupInput input{domain, 2, generators.data(), 2, offsets, 4, literals, values.data()};
  GroupSettings settings;
  settings.factor_support_components = true;
  Fixings rows;
  assert(fix_clauses(input, settings, nullptr, nullptr, rows) == Status::Complete);
  assert(rows.count == 4 && rows.image_offsets && rows.image_entries == 8);
  assert(rows.image_offsets[rows.count] == rows.image_entries);
  std::vector<uint32_t> image(domain);
  for (uint64_t row = 0; row < rows.count; ++row) {
    assert(materialize_fixing_image(rows, row, image.data()));
    for (uint32_t point = 8; point < domain; ++point)
      assert(image[point] == point);
    if (rows.kinds[row] != 1)
      assert(image[rows.representatives[row]] == rows.others[row]);
  }
  // Rejected compact rows leave caller scratch untouched.
  const auto before_bad = image;
  for (unsigned fault = 0; fault < 6; ++fault) {
    const auto point = rows.image_points[0], target = rows.images[0];
    const auto offset = rows.image_offsets[0], final = rows.image_offsets[rows.count];
    const auto second_point = rows.image_points[1];
    switch (fault) {
    case 0: rows.image_points[0] = domain; break;
    case 1: rows.images[0] = domain; break;
    case 2: rows.images[0] = rows.image_points[0]; break;
    case 3: rows.image_points[1] = rows.image_points[0]; break;
    case 4: rows.image_offsets[0] = 1; break;
    case 5: rows.image_offsets[rows.count] = rows.image_entries + 1; break;
    }
    assert(!materialize_fixing_image(rows, 0, image.data()));
    assert(image == before_bad);
    rows.image_points[0] = point; rows.images[0] = target;
    rows.image_points[1] = second_point;
    rows.image_offsets[0] = offset; rows.image_offsets[rows.count] = final;
  }
  // Layout failures cannot create an unchecked full-domain map.
  const auto first = rows.image_points[0];
  rows.image_points[0] = domain;
  assert(!materialize_fixing_image(rows, 0, image.data()));
  rows.image_points[0] = first;
  const auto end = rows.image_offsets[1];
  rows.image_offsets[1] = rows.image_entries + 1;
  assert(!materialize_fixing_image(rows, 0, image.data()));
  rows.image_offsets[1] = end;
  assert(!materialize_fixing_image(rows, rows.count, image.data()));
  release(rows);
  assert(!rows.images && !rows.image_points && !rows.image_offsets && !rows.image_entries);
  // Compact identity rows need no source/target allocation, and release owns
  // their offset buffer even when there are zero moved entries.
  rows.domain_size = domain; rows.count = 1;
  rows.image_offsets = static_cast<uint64_t *>(std::calloc(2, sizeof(uint64_t)));
  assert(rows.image_offsets);
  assert(materialize_fixing_image(rows, 0, image.data()));
  for (uint32_t p = 0; p < domain; ++p) assert(image[p] == p);
  release(rows);
  assert(!rows.image_offsets && !rows.count && !rows.domain_size);
}

void fixing_images_have_sr_direction_for_non_involutions() {
  // DejaVu's transversal maps the requested orbit point to the new base. That
  // is the direction used by Tactix's negative-fixed-literal SR witness. A
  // transposition cannot distinguish it from its inverse, hence this explicit
  // order-three generator.
  const uint32_t cycle[] = {1, 2, 0};
  const uint64_t offsets[] = {0, 3};
  const uint32_t literals[] = {0, 1, 2};
  const int8_t values[] = {0, 0, 0};
  const GroupInput input{3, 1, cycle, 1, offsets, 3, literals, values};
  GroupSettings settings;
  settings.maximum_instances = 3;
  settings.maximum_ticks = 100000;
  Fixings fixings;
  const Status status =
      fix_clauses(input, settings, nullptr, nullptr, fixings);
  if (status != Status::Complete)
    std::fprintf(stderr, "non-involution fixing status %u, ticks %llu\n",
                 static_cast<unsigned>(status),
                 static_cast<unsigned long long>(fixings.ticks));
  assert(status == Status::Complete);
  assert(fixings.domain_size == 3 && fixings.count == 3);
  assert(fixings.kinds[0] == 0 && fixings.kinds[1] == 0 &&
         fixings.kinds[2] == 1);
  const uint32_t representative = fixings.representatives[0];
  const uint32_t other = fixings.others[0];
  const uint32_t *image = fixings.images;
  assert(representative < 3 && other < 3 && representative != other);
  assert(image[representative] == other);
  // This order-three case proves that the inverse transversal was returned;
  // a transposition would make the direction test vacuous.
  assert(image[other] != representative);
  release(fixings);
}

} // namespace

int main() {
  symmetry_trace_levels_separate_campaign_and_generator_detail();
  row_images_ignore_untouched_root_cells();
  reduced_first_row_preserves_prefixes_and_stops();
  sparse_group_input_preserves_results_and_budgets();
  sparse_detector_storage_preserves_dense_semantics();
  sparse_cut_checks_match_the_dense_predicate();
  clausal_reverse_star_closes_known_targets();
  clausal_transversals_close_alternating_generator_words();
  clausal_base_extension_retains_known_generators();
  owner_chain_order_fixes_negations_first_and_cuts_the_last_largest_orbit();
  negation_unit_value_matches_the_published_unit();
  compact_component_images_scale_with_support();
  detection_owns_results_and_observes_stop();
  detector_order_is_effective_reproducible_and_bounded();
  invalid_inputs_release_prior_results();
  parallel_edges_are_invalid();
  individualized_refinement_aligns_row_companions();
  prepared_refinement_is_reusable_and_owned();
  invalid_orbit_vertex_is_rejected();
  individualized_orbit_reduction_selects_largest_remainder();
  individualized_partition_exposes_product_remainders();
  complete_permutations_produce_full_fixings();
  first_amo_selection_preserves_caller_order();
  shortest_clause_precedes_long_amo();
  first_shortest_retains_forward_remainder();
  ordered_remainder_starts_after_first_amo_free_scan();
  forward_scan_keeps_a_monotone_clause_frontier();
  fix_and_cut_emits_complete_orbit_and_negation_cuts();
  planning_limit_preserves_a_complete_prefix();
  bounded_resifting_preserves_overlapping_actions();
  fixing_images_have_sr_direction_for_non_involutions();
  support_components_preserve_group_and_clause_boundaries();
  std::puts("DejaVu backend boundary, ownership and fixing PASS");
}
