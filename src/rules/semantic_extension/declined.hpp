#pragma once

#include <type_traits>

namespace tactix {

// An early return from a function that builds a plain status record: records
// `status` on `result` and returns a copy. The caller's `result` keeps the
// status, so observers of it see the decline. Records that own containers
// return the local itself instead, so the copy here is only a field copy.
template <class Result, class Status>
Result declined(Result &result, Status status) {
  static_assert(std::is_trivially_copyable_v<Result>);
  result.status = status;
  return result;
}

} // namespace tactix
