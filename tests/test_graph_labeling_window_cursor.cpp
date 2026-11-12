#undef NDEBUG
#include "rules/lucky/graph_labeling_window_cursor.hpp"

#include <cassert>

using namespace tactix::graph_labeling;

static Shape complete_shape(uint32_t n) {
  Shape shape;
  shape.vertices = n;
  shape.separation = 1;
  shape.adjacency.resize(n);
  for (uint32_t a = 0; a < n; ++a)
    for (uint32_t b = a + 1; b < n; ++b) {
      shape.edges.emplace_back(a, b);
      shape.adjacency[a].push_back(b);
      shape.adjacency[b].push_back(a);
    }
  return shape;
}

static WindowCursorStatus finish(WindowCursor &cursor, const Shape &shape,
                                 uint64_t slice, Candidate &candidate) {
  for (uint64_t calls = 0; calls < 100000; ++calls) {
    const auto result = cursor.step(shape, slice, candidate);
    if (result.status != WindowCursorStatus::Pending) return result.status;
  }
  assert(false && "cursor did not terminate");
  return WindowCursorStatus::NoCandidate;
}

int main() {
  const Shape shape = complete_shape(7);
  const WindowLimits limits{100000, 4, 4, 0x51a7e};
  Candidate reference;
  uint64_t reference_work = 0, reference_nodes = 0;
  const auto reference_status = window_search(
      shape, limits,
      [&](uint64_t amount) {
        reference_work += amount;
        return true;
      },
      reference, reference_nodes);
  assert(reference_status == WindowStatus::Found);
  assert(!reference.score.first);

  const uint64_t required = WindowCursor::required_workspace(shape);
  assert(required);
  {
    WindowCursor rejected;
    assert(rejected.initialize(shape, limits, reference_work, required - 1) ==
           WindowCursorAdmission::WorkspaceExhausted);
    assert(!rejected.ready());
    assert(!rejected.work() && !rejected.nodes());
  }

  Candidate one_shot;
  WindowCursor large;
  assert(large.initialize(shape, limits, reference_work, required) ==
         WindowCursorAdmission::Ready);
  const auto large_result = large.step(shape, reference_work, one_shot);
  assert(large_result.status == WindowCursorStatus::Found);
  assert(large.work() == reference_work);
  assert(large.nodes() == reference_nodes);
  assert(one_shot.labels == reference.labels && one_shot.score == reference.score);

  Candidate sliced;
  WindowCursor small;
  assert(small.initialize(shape, limits, reference_work, required) ==
         WindowCursorAdmission::Ready);
  assert(finish(small, shape, 31, sliced) == WindowCursorStatus::Found);
  assert(small.work() == reference_work);
  assert(small.nodes() == reference_nodes);
  assert(sliced.labels == reference.labels && sliced.score == reference.score);

  Candidate prepaid_candidate;
  WindowCursor prepaid;
  const uint64_t setup = WindowCursor::initialization_work(shape);
  assert(setup < reference_work);
  assert(prepaid.initialize_after_charge(shape, limits, reference_work, required) ==
         WindowCursorAdmission::Ready);
  assert(prepaid.work() == setup);
  const auto prepaid_result =
      prepaid.step(shape, reference_work - setup, prepaid_candidate);
  assert(prepaid_result.status == WindowCursorStatus::Found);
  assert(prepaid_result.work == reference_work - setup);
  assert(prepaid.work() == reference_work && prepaid.nodes() == reference_nodes);
  assert(prepaid_candidate.labels == reference.labels);
  assert(prepaid_candidate.score == reference.score);

  // The same slice sequence is a deterministic replay of every frontier and
  // random-stream transition, including exhaustion boundaries.
  WindowCursor replay_a, replay_b;
  Candidate replay_candidate_a, replay_candidate_b;
  assert(replay_a.initialize(shape, limits, reference_work, required) ==
         WindowCursorAdmission::Ready);
  assert(replay_b.initialize(shape, limits, reference_work, required) ==
         WindowCursorAdmission::Ready);
  for (;;) {
    const auto a = replay_a.step(shape, 37, replay_candidate_a);
    const auto b = replay_b.step(shape, 37, replay_candidate_b);
    assert(a.status == b.status && a.work == b.work && a.nodes == b.nodes);
    assert(replay_a.work() == replay_b.work());
    assert(replay_a.nodes() == replay_b.nodes());
    if (a.status != WindowCursorStatus::Pending) break;
  }
  assert(replay_candidate_a.labels == replay_candidate_b.labels);
  assert(replay_candidate_a.score == replay_candidate_b.score);

  // Interruption differs from either per-slice or total-cap exhaustion and
  // leaves the exact next charged mutation available for resumption.
  WindowCursor interrupted;
  Candidate interrupted_candidate;
  assert(interrupted.initialize(shape, limits, reference_work, required) ==
         WindowCursorAdmission::Ready);
  uint64_t polls = 0;
  const auto stopped = interrupted.step(
      shape, reference_work, interrupted_candidate,
      [&] { return polls++ == 20; });
  assert(stopped.status == WindowCursorStatus::Interrupted);
  assert(finish(interrupted, shape, 43, interrupted_candidate) ==
         WindowCursorStatus::Found);
  assert(interrupted.work() == reference_work);
  assert(interrupted.nodes() == reference_nodes);
  assert(interrupted_candidate.labels == reference.labels);

  WindowCursor capped;
  Candidate capped_candidate;
  const uint64_t capped_allowance = reference_work - 1;
  uint64_t reference_capped_work = 0, reference_capped_nodes = 0;
  Candidate reference_capped_candidate;
  const auto reference_capped = window_search(
      shape, limits,
      [&](uint64_t amount) {
        if (amount > capped_allowance - reference_capped_work) return false;
        reference_capped_work += amount;
        return true;
      },
      reference_capped_candidate, reference_capped_nodes);
  assert(reference_capped == WindowStatus::Limited);
  assert(capped.initialize(shape, limits, capped_allowance, required) ==
         WindowCursorAdmission::Ready);
  assert(finish(capped, shape, 29, capped_candidate) ==
         WindowCursorStatus::TotalWorkExhausted);
  assert(capped.work() == reference_capped_work);
  assert(capped.nodes() == reference_capped_nodes);
  assert(capped_candidate.labels == reference_capped_candidate.labels);
  assert(capped_candidate.score == reference_capped_candidate.score);
  assert(capped.terminal());
}
