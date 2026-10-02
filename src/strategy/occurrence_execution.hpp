#pragma once

#include "kernel/kernel.hpp"

namespace tactix::execution {

enum class OccurrenceAttachment : uint8_t { Build, Retained };

struct OccurrenceSessionRequest {
  OccIndex &index;
  OccurrenceAttachment attachment = OccurrenceAttachment::Build;
  bool track_order = false;
};

// Own only the occurrence attachment, not sparse-watch transitions or policy
// cache validity. A borrower leaves its caller's session alone by not creating
// this scope. close() permits publication and reconnection at native boundaries.
class OccurrenceSession {
public:
  OccurrenceSession(Kernel &k, const OccurrenceSessionRequest &request)
      : kernel_(k), index_(&request.index) {
    if (request.attachment == OccurrenceAttachment::Build)
      kernel_dense_connect(k, request.index, request.track_order);
    else {
      assert(!k.bve_occurrence_count_image);
      k.dense = index_;
    }
  }
  ~OccurrenceSession() { close(); }
  OccurrenceSession(const OccurrenceSession &) = delete;
  OccurrenceSession &operator=(const OccurrenceSession &) = delete;

  void close() {
    if (!index_) return;
    assert(kernel_.dense == index_);
    kernel_dense_disconnect(kernel_);
    index_ = nullptr;
  }

private:
  Kernel &kernel_;
  OccIndex *index_;
};

} // namespace tactix::execution
