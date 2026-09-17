#pragma once

#include "strategy/rule_invocation.hpp"

#include <memory>

namespace tactix::strategy {

// A complete pure-BVE pass may span many public advance calls. Retain its
// ordinary occurrence image without leaving Kernel::dense attached between
// calls (Lucky and other public actions require the sparse boundary).
class BveOccurrenceCache {
  struct Image {
    OccIndex occurrences;
    const Kernel *owner = nullptr;
    StateVersion version;
    bool usable = true;
  };
  std::shared_ptr<Image> image_;

public:
  void reset() { image_.reset(); }

  class Scope {
    Kernel &kernel_;
    Image *attached_ = nullptr;

  public:
    Scope(BveOccurrenceCache &cache, Kernel &k) : kernel_(k) {
      // Caller-owned dense mode has its own ordering and lifecycle. Never
      // replace it or remember it as an executor-owned snapshot.
      if (k.dense) {
        cache.reset();
        return;
      }
      const StateVersion current = state_version(k);
      const auto &image = cache.image_;
      if (!image || !image.unique() || image->owner != &k ||
          image->version.arena_generation != current.arena_generation ||
          image->version.touch_epoch != current.touch_epoch ||
          image->version.strategy_epoch != current.strategy_epoch) {
        cache.image_ = std::make_shared<Image>();
        // Ordinary dense images exclude redundant clauses. Sparse BVE also
        // deletes those on its pivot, so retain that path when any are live.
        // Test once per image, not once per candidate in the retained pass.
        for (Ref ref = 0; ref < k.arena.size();) {
          const Clause *c = clause_at(k.arena, ref);
          if (!c->garbage() && c->redundant()) {
            cache.image_->usable = false;
            break;
          }
          ref += clause_actual_words(c);
        }
        if (cache.image_->usable)
          kernel_dense_connect(k, cache.image_->occurrences, false);
      } else if (image->usable) {
        assert(!k.bve_occurrence_count_image);
        k.dense = &image->occurrences;
      }
      attached_ = cache.image_.get();
    }

    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

    ~Scope() {
      if (!attached_)
        return;
      assert(kernel_.dense == (attached_->usable ? &attached_->occurrences
                                                : nullptr));
      attached_->owner = &kernel_;
      attached_->version = state_version(kernel_);
      if (attached_->usable)
        kernel_dense_disconnect(kernel_);
    }
  };
};

} // namespace tactix::strategy
