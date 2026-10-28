#pragma once

#include "rules/lucky/graph_labeling_search.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <utility>
#include <vector>

namespace tactix::graph_labeling {

enum class WindowCursorAdmission : uint8_t {
  Ready,
  Invalid,
  WorkspaceExhausted,
};

enum class WindowCursorStatus : uint8_t {
  Pending,
  Found,
  NoCandidate,
  SearchLimit,
  TotalWorkExhausted,
  Interrupted,
};

struct WindowCursorStepResult {
  WindowCursorStatus status = WindowCursorStatus::Pending;
  uint64_t work = 0;
  uint64_t nodes = 0;
};

// Resumable form of window_search. Every charged mutation has an explicit
// cursor, including frame construction, shuffling and insertion sorting. Thus
// dividing one total allowance into smaller slices preserves the exact search
// frontier, random stream, candidate and logical-work count.
class WindowCursor {
public:
  WindowCursor() = default;
  WindowCursor(const WindowCursor &) = delete;
  WindowCursor &operator=(const WindowCursor &) = delete;
  WindowCursor(WindowCursor &&) = default;
  WindowCursor &operator=(WindowCursor &&) = default;

  static uint64_t required_workspace(const Shape &shape) {
    const uint64_t n = shape.vertices;
    const uint64_t words = (n + 63) / 64;
    uint64_t bytes = sizeof(WindowCursor);
    const auto add = [&](uint64_t count, uint64_t width) {
      if (count && width > (UINT64_MAX - bytes) / count) {
        bytes = UINT64_MAX;
        return;
      }
      bytes += count * width;
    };
    add(n * words, sizeof(uint64_t));       // adjacency bits
    add(n, 2 * sizeof(uint32_t) + 1);       // starts, order, used
    add(n * n, sizeof(uint32_t));           // fixed frame-choice matrix
    add(n, 2 * sizeof(uint32_t));           // frame counts and cursors
    add(words, sizeof(uint64_t));           // forbidden words
    add(n, sizeof(uint64_t));               // remaining degree
    add(n, sizeof(uint32_t));               // complete leaf labels
    return bytes;
  }

  static uint64_t initialization_work(const Shape &shape) {
    const uint64_t n = shape.vertices;
    return n * ((n + 63) / 64) + 3 * n;
  }

  WindowCursorAdmission initialize(const Shape &shape,
                                   const WindowLimits &limits,
                                   uint64_t total_work_allowance,
                                   uint64_t maximum_workspace_bytes) {
    return initialize_impl(shape, limits, total_work_allowance,
                           maximum_workspace_bytes, false);
  }

  // The retained session admits and charges initialization before allocating
  // cursor storage. This entry preserves the legacy total-work count without
  // charging the same setup a second time in step().
  WindowCursorAdmission initialize_after_charge(
      const Shape &shape, const WindowLimits &limits,
      uint64_t total_work_allowance, uint64_t maximum_workspace_bytes) {
    return initialize_impl(shape, limits, total_work_allowance,
                           maximum_workspace_bytes, true);
  }

private:
  WindowCursorAdmission initialize_impl(const Shape &shape,
                                        const WindowLimits &limits,
                                        uint64_t total_work_allowance,
                                        uint64_t maximum_workspace_bytes,
                                        bool initialization_charged) {
    const uint64_t n = shape.vertices;
    if (!n || !shape.separation || shape.adjacency.size() != n ||
        n > SIZE_MAX / n)
      return WindowCursorAdmission::Invalid;
    for (const auto &row : shape.adjacency)
      for (uint32_t other : row)
        if (other >= n) return WindowCursorAdmission::Invalid;
    for (const auto &[a, b] : shape.edges)
      if (a >= n || b >= n) return WindowCursorAdmission::Invalid;
    const uint64_t workspace = required_workspace(shape);
    if (workspace > maximum_workspace_bytes)
      return WindowCursorAdmission::WorkspaceExhausted;

    WindowCursor next;
    next.n_ = static_cast<uint32_t>(n);
    next.separation_ = shape.separation;
    next.words_ = (n + 63) / 64;
    next.limits_ = limits;
    next.total_work_allowance_ = total_work_allowance;
    next.retained_bytes_ = workspace;
    next.adjacent_.assign(static_cast<size_t>(n * next.words_), 0);
    next.starts_.resize(static_cast<size_t>(n));
    for (uint32_t v = 0; v < n; ++v) next.starts_[v] = v;
    next.order_.reserve(static_cast<size_t>(n));
    next.used_.assign(static_cast<size_t>(n), 0);
    next.frame_choices_.resize(static_cast<size_t>(n * n));
    next.frame_counts_.assign(static_cast<size_t>(n), 0);
    next.frame_next_.assign(static_cast<size_t>(n), 0);
    next.forbidden_.resize(static_cast<size_t>(next.words_));
    next.remaining_degree_.resize(static_cast<size_t>(n));
    next.leaf_.labels.resize(static_cast<size_t>(n));
    next.random_.seed(limits.seed);
    const bool search_enabled =
        limits.nodes && limits.starts && limits.early_choices;
    if (initialization_charged && search_enabled) {
      const uint64_t setup = initialization_work(shape);
      if (setup > total_work_allowance) return WindowCursorAdmission::Invalid;
      next.work_ = setup;
      next.phase_ = Phase::BuildAdjacency;
    } else {
      next.phase_ = search_enabled ? Phase::InitialCharge : Phase::SearchLimit;
    }
    *this = std::move(next);
    return WindowCursorAdmission::Ready;
  }

public:

  bool ready() const { return phase_ != Phase::Empty; }
  bool terminal() const {
    return phase_ == Phase::Found || phase_ == Phase::NoCandidate ||
           phase_ == Phase::SearchLimit || phase_ == Phase::TotalExhausted;
  }
  uint64_t retained_bytes() const { return retained_bytes_; }
  uint64_t work() const { return work_; }
  uint64_t nodes() const { return nodes_; }

  WindowCursorStepResult step(const Shape &shape, uint64_t slice_allowance,
                              Candidate &best) {
    return step(shape, slice_allowance, best, [] { return false; });
  }

  template <class Poll>
  WindowCursorStepResult step(const Shape &shape, uint64_t slice_allowance,
                              Candidate &best, Poll &&poll) {
    const uint64_t work_before = work_, nodes_before = nodes_;
    uint64_t slice_remaining = slice_allowance;
    const auto result = [&](WindowCursorStatus status) {
      return WindowCursorStepResult{status, work_ - work_before,
                                    nodes_ - nodes_before};
    };
    if (!ready() || shape.vertices != n_ || shape.separation != separation_ ||
        shape.adjacency.size() != n_)
      return result(WindowCursorStatus::NoCandidate);
    if (phase_ == Phase::Found) return result(WindowCursorStatus::Found);
    if (phase_ == Phase::NoCandidate)
      return result(WindowCursorStatus::NoCandidate);
    if (phase_ == Phase::SearchLimit)
      return result(WindowCursorStatus::SearchLimit);
    if (phase_ == Phase::TotalExhausted)
      return result(WindowCursorStatus::TotalWorkExhausted);

    enum class Spend { Ok, Slice, Total, Interrupted };
    const auto spend = [&](uint64_t amount) {
      if (poll()) return Spend::Interrupted;
      if (amount > total_work_allowance_ - work_) return Spend::Total;
      if (amount > slice_remaining) return Spend::Slice;
      work_ += amount;
      slice_remaining -= amount;
      return Spend::Ok;
    };
    const auto stopped = [&](Spend status) -> WindowCursorStepResult {
      if (status == Spend::Interrupted)
        return result(WindowCursorStatus::Interrupted);
      if (status == Spend::Slice)
        return result(WindowCursorStatus::Pending);
      phase_ = Phase::TotalExhausted;
      return result(WindowCursorStatus::TotalWorkExhausted);
    };

    while (true) {
      switch (phase_) {
      case Phase::InitialCharge: {
        const Spend charged = spend(uint64_t(n_) * words_ + 3 * uint64_t(n_));
        if (charged != Spend::Ok) return stopped(charged);
        phase_ = Phase::BuildAdjacency;
        break;
      }
      case Phase::BuildAdjacency:
        if (prepare_vertex_ == n_) {
          sort_i_ = 1;
          sort_holding_ = false;
          phase_ = Phase::SortStarts;
          break;
        }
        if (prepare_neighbor_ == shape.adjacency[prepare_vertex_].size()) {
          ++prepare_vertex_;
          prepare_neighbor_ = 0;
          break;
        } else {
          const Spend charged = spend(1);
          if (charged != Spend::Ok) return stopped(charged);
          const uint32_t other =
              shape.adjacency[prepare_vertex_][prepare_neighbor_++];
          adjacent_[uint64_t(prepare_vertex_) * words_ + other / 64] |=
              uint64_t{1} << (other % 64);
          break;
        }
      case Phase::SortStarts:
        if (sort_i_ == n_) {
          start_index_ = 0;
          phase_ = Phase::BeginStart;
          break;
        }
        if (!sort_holding_) {
          sort_current_ = starts_[sort_i_];
          sort_j_ = sort_i_;
          sort_holding_ = true;
        }
        if (!sort_j_) {
          starts_[0] = sort_current_;
          ++sort_i_;
          sort_holding_ = false;
          break;
        } else {
          const Spend charged = spend(1);
          if (charged != Spend::Ok) return stopped(charged);
          const uint32_t previous = starts_[sort_j_ - 1];
          const size_t dc = shape.adjacency[sort_current_].size();
          const size_t dp = shape.adjacency[previous].size();
          const bool less = dc == dp ? sort_current_ < previous : dc > dp;
          if (less) {
            starts_[sort_j_] = previous;
            --sort_j_;
          } else {
            starts_[sort_j_] = sort_current_;
            ++sort_i_;
            sort_holding_ = false;
          }
          break;
        }
      case Phase::BeginStart:
        if (start_index_ >= std::min(n_, limits_.starts)) {
          phase_ = Phase::NoCandidate;
          return result(WindowCursorStatus::NoCandidate);
        } else {
          const Spend charged = spend(n_);
          if (charged != Spend::Ok) return stopped(charged);
          std::fill(used_.begin(), used_.end(), 0);
          order_.assign(1, starts_[start_index_]);
          used_[starts_[start_index_]] = 1;
          phase_ = Phase::EnterNode;
          break;
        }
      case Phase::EnterNode:
        if (nodes_ == limits_.nodes) {
          phase_ = Phase::SearchLimit;
          return result(WindowCursorStatus::SearchLimit);
        } else {
          const Spend charged = spend(1);
          if (charged != Spend::Ok) return stopped(charged);
          ++nodes_;
          depth_ = static_cast<uint32_t>(order_.size());
          if (depth_ == n_)
            phase_ = Phase::BuildLeaf;
          else
            phase_ = Phase::ClearForbidden;
          break;
        }
      case Phase::BuildLeaf: {
        const Spend charged = spend(n_);
        if (charged != Spend::Ok) return stopped(charged);
        for (uint32_t i = 0; i < n_; ++i) leaf_.labels[order_[i]] = i;
        phase_ = leaf_.labels[0] + 1 > n_ / 2 ? Phase::ReflectLeaf
                                               : Phase::ScoreLeaf;
        leaf_edge_ = 0;
        leaf_.score = {0, 0};
        break;
      }
      case Phase::ReflectLeaf: {
        const Spend charged = spend(n_);
        if (charged != Spend::Ok) return stopped(charged);
        for (uint32_t &label : leaf_.labels) label = n_ - 1 - label;
        phase_ = Phase::ScoreLeaf;
        break;
      }
      case Phase::ScoreLeaf:
        if (leaf_edge_ == shape.edges.size()) {
          best = leaf_;
          phase_ = Phase::Found;
          return result(WindowCursorStatus::Found);
        } else {
          const Spend charged = spend(1);
          if (charged != Spend::Ok) return stopped(charged);
          const auto [a, b] = shape.edges[leaf_edge_++];
          const uint32_t distance = leaf_.labels[a] > leaf_.labels[b]
                                        ? leaf_.labels[a] - leaf_.labels[b]
                                        : leaf_.labels[b] - leaf_.labels[a];
          if (distance < separation_) {
            ++leaf_.score.first;
            const uint64_t deficit = separation_ - distance;
            leaf_.score.second += deficit * deficit;
          }
          break;
        }
      case Phase::ClearForbidden: {
        const Spend charged = spend(words_);
        if (charged != Spend::Ok) return stopped(charged);
        std::fill(forbidden_.begin(), forbidden_.end(), 0);
        recent_index_ = depth_ >= separation_ ? depth_ - separation_ + 1 : 0;
        recent_word_ = 0;
        phase_ = Phase::BuildForbidden;
        break;
      }
      case Phase::BuildForbidden:
        if (recent_index_ == depth_) {
          choice_vertex_ = 0;
          frame_counts_[depth_ - 1] = 0;
          phase_ = Phase::ScanChoices;
          break;
        } else {
          const Spend charged = spend(1);
          if (charged != Spend::Ok) return stopped(charged);
          forbidden_[recent_word_] |=
              adjacent_[uint64_t(order_[recent_index_]) * words_ + recent_word_];
          if (++recent_word_ == words_) {
            recent_word_ = 0;
            ++recent_index_;
          }
          break;
        }
      case Phase::ScanChoices:
        if (choice_vertex_ == n_) {
          shuffle_i_ = frame_counts_[depth_ - 1];
          phase_ = Phase::ShuffleChoices;
          break;
        } else {
          const Spend charged = spend(1);
          if (charged != Spend::Ok) return stopped(charged);
          if (used_[choice_vertex_] ||
              (forbidden_[choice_vertex_ / 64] &
               (uint64_t{1} << (choice_vertex_ % 64)))) {
            ++choice_vertex_;
          } else {
            remaining_degree_[choice_vertex_] = 0;
            degree_neighbor_ = 0;
            phase_ = Phase::CountDegree;
          }
          break;
        }
      case Phase::CountDegree:
        if (degree_neighbor_ == shape.adjacency[choice_vertex_].size()) {
          const uint64_t offset = uint64_t(depth_ - 1) * n_ +
                                  frame_counts_[depth_ - 1]++;
          frame_choices_[offset] = choice_vertex_++;
          phase_ = Phase::ScanChoices;
          break;
        } else {
          const Spend charged = spend(1);
          if (charged != Spend::Ok) return stopped(charged);
          const uint32_t other =
              shape.adjacency[choice_vertex_][degree_neighbor_++];
          remaining_degree_[choice_vertex_] += !used_[other];
          break;
        }
      case Phase::ShuffleChoices:
        if (shuffle_i_ <= 1) {
          sort_i_ = 1;
          sort_holding_ = false;
          phase_ = Phase::SortChoices;
          break;
        } else {
          const Spend charged = spend(1);
          if (charged != Spend::Ok) return stopped(charged);
          const uint64_t base = uint64_t(depth_ - 1) * n_;
          std::swap(frame_choices_[base + shuffle_i_ - 1],
                    frame_choices_[base + random_() % shuffle_i_]);
          --shuffle_i_;
          break;
        }
      case Phase::SortChoices: {
        const uint32_t count = frame_counts_[depth_ - 1];
        const uint64_t base = uint64_t(depth_ - 1) * n_;
        if (sort_i_ >= count) {
          phase_ = Phase::FinishFrame;
          break;
        }
        if (!sort_holding_) {
          sort_current_ = frame_choices_[base + sort_i_];
          sort_j_ = sort_i_;
          sort_holding_ = true;
        }
        if (!sort_j_) {
          frame_choices_[base] = sort_current_;
          ++sort_i_;
          sort_holding_ = false;
          break;
        }
        const Spend charged = spend(1);
        if (charged != Spend::Ok) return stopped(charged);
        const uint32_t previous = frame_choices_[base + sort_j_ - 1];
        const uint32_t recent =
            std::min(depth_, separation_ >= 2 ? separation_ - 2 : 0);
        const uint64_t current_rank = remaining_degree_[sort_current_] * recent;
        const uint64_t previous_rank = remaining_degree_[previous] * recent;
        const bool less = current_rank != previous_rank
                              ? current_rank < previous_rank
                              : shape.adjacency[sort_current_].size() >
                                    shape.adjacency[previous].size();
        if (less) {
          frame_choices_[base + sort_j_] = previous;
          --sort_j_;
        } else {
          frame_choices_[base + sort_j_] = sort_current_;
          ++sort_i_;
          sort_holding_ = false;
        }
        break;
      }
      case Phase::FinishFrame:
        if (depth_ < n_ / 3 &&
            frame_counts_[depth_ - 1] > limits_.early_choices)
          frame_counts_[depth_ - 1] = limits_.early_choices;
        frame_next_[depth_ - 1] = 0;
        phase_ = Phase::Traverse;
        break;
      case Phase::Traverse: {
        const uint32_t row = static_cast<uint32_t>(order_.size() - 1);
        if (frame_next_[row] < frame_counts_[row]) {
          const uint32_t vertex =
              frame_choices_[uint64_t(row) * n_ + frame_next_[row]++];
          order_.push_back(vertex);
          used_[vertex] = 1;
          phase_ = Phase::EnterNode;
        } else {
          used_[order_.back()] = 0;
          order_.pop_back();
          if (order_.empty()) {
            ++start_index_;
            phase_ = Phase::BeginStart;
          }
        }
        break;
      }
      case Phase::Found:
        return result(WindowCursorStatus::Found);
      case Phase::NoCandidate:
        return result(WindowCursorStatus::NoCandidate);
      case Phase::SearchLimit:
        return result(WindowCursorStatus::SearchLimit);
      case Phase::TotalExhausted:
        return result(WindowCursorStatus::TotalWorkExhausted);
      case Phase::Empty:
        return result(WindowCursorStatus::NoCandidate);
      }
    }
  }

private:
  enum class Phase : uint8_t {
    Empty,
    InitialCharge,
    BuildAdjacency,
    SortStarts,
    BeginStart,
    EnterNode,
    BuildLeaf,
    ReflectLeaf,
    ScoreLeaf,
    ClearForbidden,
    BuildForbidden,
    ScanChoices,
    CountDegree,
    ShuffleChoices,
    SortChoices,
    FinishFrame,
    Traverse,
    Found,
    NoCandidate,
    SearchLimit,
    TotalExhausted,
  };

  Phase phase_ = Phase::Empty;
  uint32_t n_ = 0, separation_ = 0;
  uint64_t words_ = 0;
  WindowLimits limits_{};
  uint64_t total_work_allowance_ = 0;
  uint64_t retained_bytes_ = 0;
  uint64_t work_ = 0, nodes_ = 0;
  std::vector<uint64_t> adjacent_;
  std::vector<uint32_t> starts_, order_;
  std::vector<uint8_t> used_;
  std::vector<uint32_t> frame_choices_, frame_counts_, frame_next_;
  std::vector<uint64_t> forbidden_, remaining_degree_;
  Candidate leaf_;
  std::mt19937_64 random_;

  uint32_t prepare_vertex_ = 0;
  size_t prepare_neighbor_ = 0;
  uint32_t start_index_ = 0, depth_ = 0;
  uint32_t recent_index_ = 0;
  uint64_t recent_word_ = 0;
  uint32_t choice_vertex_ = 0;
  size_t degree_neighbor_ = 0;
  uint32_t shuffle_i_ = 0;
  uint32_t sort_i_ = 0, sort_j_ = 0, sort_current_ = 0;
  bool sort_holding_ = false;
  size_t leaf_edge_ = 0;
};

} // namespace tactix::graph_labeling
