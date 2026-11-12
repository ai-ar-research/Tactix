#pragma once
#include "session/failed_core.hpp"
#include <algorithm>

namespace tactix::session {

enum class ResultCaptureStatus : uint8_t {
  Ok, NoActiveQuery, WrongHistory, InvalidResult, InvalidModel, NotReady, Interrupted
};

// Immutable copies bound to one completed original query, independent of the
// mutable Kernel/H and reconstruction buffers. The query owner validates the
// current history state before exposing this view; input mutation invalidates it.
struct QueryResultSnapshot {
  uint64_t query = 0, generation = 0;
  size_t input_words = 0;
  QueryResult result = QueryResult::Unknown;
  std::vector<int8_t> model; // positive external variables, zero based
  FailedCore core;
  std::vector<int32_t> failed; // sorted membership index; preserve core order
  bool failed_literal(int32_t literal) const {
    return result==QueryResult::Unsat && literal && literal!=INT32_MIN &&
        std::binary_search(failed.begin(),failed.end(),literal);
  }
};

} // namespace tactix::session
