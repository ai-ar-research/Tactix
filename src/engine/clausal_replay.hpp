#pragma once
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tactix {
// A private, propagation-only clause database for atomic proof batches.
// Complete occurrence lists authenticate every RAT resolvent; no caller hint
// can omit an opposing-pivot clause. Clause handles survive watch movement.
class ClausalReplay {
public:
  using Clause = std::vector<int32_t>;
  using Handle = uint32_t;
  enum class Status { Ready, Invalid, Exhausted, Interrupted };
  struct Effort {
    uint64_t work = 0, rup_queries = 0, rat_resolvents = 0;
    uint64_t propagated = 0, watch_visits = 0;
  };
  ClausalReplay(uint32_t external_extent, uint64_t maximum_work,
                const volatile sig_atomic_t *stop = nullptr);
  Handle import(const Clause &); // trusted initial formula, 0 on failure
  bool rup(const Clause &);
  bool rat(const Clause &); // first literal is the pivot; empty is invalid
  bool erase(Handle);
  const Clause &clause(Handle h) const { return clauses_[h-1].literals; }
  bool live(Handle h) const { return h && h<=clauses_.size() && clauses_[h-1].live; }
  bool account(uint64_t n=1) { return take(n); }
  Status status() const { return status_; }
  const Effort &effort() const { return effort_; }
private:
  struct Record { Clause literals; bool live = true; };
  uint32_t extent_;
  uint64_t maximum_work_;
  const volatile sig_atomic_t *stop_;
  Status status_ = Status::Ready;
  Effort effort_;
  std::vector<Record> clauses_;
  std::vector<std::vector<Handle>> watches_, occurrences_;
  std::vector<Handle> units_;
  std::vector<int8_t> values_, marks_;
  std::vector<uint32_t> trail_, touched_;
  size_t head_ = 0, root_ = 0;
  uint64_t empty_ = 0;
  bool root_conflict_ = false, rebuild_root_ = false;
  bool take(uint64_t = 1);
  bool normalize(const Clause &, Clause &, bool &tautology);
  bool propagate();
  bool settle();
  bool assign(uint32_t);
  void undo(size_t);
  bool refute(const Clause &);
  static uint32_t literal(int32_t x);
};
} // namespace tactix
