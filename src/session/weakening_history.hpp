#pragma once

#include "population/population_observer.hpp"

#include <span>
#include <array>
#include <unordered_map>

namespace tactix::session {

class QueryKernel;
class WeakeningHistory;

// Caller storage allowances, never rule guards. Exhaustion makes capture
// incomplete; it does not decline an action or change its proof/search path.
struct WeakeningLimits {
  size_t records = 0; // saved physical sources, including duplicate copies
  size_t literals = 0;
  size_t live_binary_copies = 0;
};

struct WeakeningSource {
  uint64_t occurrence = 0;
  uint64_t proof_id = 0; // zero only for an independently admitted repair lemma
};

struct WeakeningRecord {
  uint64_t order = 0;
  WeakeningFamily family = WeakeningFamily::Bve;
  std::vector<int32_t> clause; // full saved clause, not its root residual
  std::vector<int32_t> witness;
  std::vector<WeakeningSource> sources; // physical copies, or one derived repair target
  uint64_t reconstruction_words = 0;
  // Cube/multi-prefix authentication image. Single native BVE/BCE records use
  // their existing source/witness check and do not duplicate their rho storage.
  std::vector<Lit> repair;
  bool derived = false;
};

enum class RestorationPlanStatus : uint8_t { Ready, Incomplete, InvalidLiteral, Exhausted };

// Immutable, owner/version-bound selection. Planning performs the reference's
// single chronological scan; it changes neither the kernel nor the history.
class RestorationPlan {
public:
  RestorationPlanStatus status() const { return status_; }
  std::span<const WeakeningRecord> selected() const { return selected_; }
  size_t kept() const { return kept_; }
private:
  friend class WeakeningHistory;
  friend class QueryKernel;
  RestorationPlanStatus status_ = RestorationPlanStatus::Incomplete;
  const WeakeningHistory *owner_ = nullptr;
  uint64_t revision_ = 0;
  size_t kept_ = 0;
  std::vector<size_t> selected_indices_;
  std::vector<WeakeningRecord> selected_;
  std::vector<Lit> before_, kept_rho_;
};

// Optional native-session evidence owner. Bind before input through the
// population-observer seam. It observes committed weakening/repair groups and owns
// stable external payloads and full-width IDs across compaction/collection.
// It neither restores clauses nor composes certificates. The caller must check
// capture_complete() at settled action boundaries, and must separately license SR and
// other restriction dependencies. Unsupported reconstruction or missing input
// provenance makes capture_complete() false rather than silently promising restoration.
class WeakeningHistory final : public PopulationObserver {
public:
  WeakeningHistory(const Kernel &, WeakeningLimits);
  WeakeningHistory(const WeakeningHistory &) = delete;
  WeakeningHistory &operator=(const WeakeningHistory &) = delete;
  WeakeningHistory(WeakeningHistory &&) = delete;
  WeakeningHistory &operator=(WeakeningHistory &&) = delete;
  void observe_binding(const Kernel &, bool bound) noexcept override;
  bool records_weakening() const noexcept override { return true; }
  void observe(const SolverPopulationEvent &) noexcept override;
  void observe(const WeakeningPopulationEvent &) noexcept override;
  bool capture_complete() const noexcept;
  RestorationPlan plan_restoration(std::span<const int32_t> used_literals) const noexcept;
  // Borrowed until the next observed mutation; the owner itself cannot move.
  std::span<const WeakeningRecord> records() const { return records_; }
  size_t pending() const { return pending_.size(); }
  size_t live_binary_copies() const { return live_binary_copies_; }

private:
  friend class QueryKernel;
  struct BinaryCopy { WeakeningSource source; uint64_t reserved = 0; };
  struct BinaryPopulation { std::vector<BinaryCopy> copies; size_t reserved = 0; };
  struct Pending {
    WeakeningRecord record;
    uint64_t begin = 0, end = 0;
    size_t deleted = 0;
    bool binary = false;
  };
  struct BinaryDelta {
    SolverPopulationEventKind kind;
    uint64_t id;
    uint32_t copies;
    std::array<int32_t, 2> clause;
  };
  const Kernel &kernel_;
  WeakeningLimits limits_;
  bool intact_ = true, binding_seen_ = false;
  uint64_t occurrence_ = 0, reconstruction_end_ = 0;
  uint64_t revision_ = 0;
  size_t literals_ = 0, live_binary_copies_ = 0, source_copies_ = 0;
  size_t staged_literals_ = 0;
  std::unordered_map<uint64_t, BinaryPopulation> binaries_;
  std::unordered_map<uint64_t, Pending> pending_;
  std::unordered_map<uint64_t, uint64_t> large_pending_;
  std::unordered_map<uint64_t, std::vector<BinaryDelta>> transactions_;
  std::vector<WeakeningRecord> records_;

  void observe_solver(const SolverPopulationEvent &);
  void observe_weakening(const WeakeningPopulationEvent &);
  bool reconstruction_matches(const Pending &) const;
  void release(uint64_t token) noexcept;
  bool current(const RestorationPlan &) const noexcept;
  void consume_restoration(Kernel &, const RestorationPlan &);
};

} // namespace tactix::session
