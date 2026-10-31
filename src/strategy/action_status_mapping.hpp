#pragma once

#include <optional>

namespace tactix::strategy {

template <class Target, class Source>
std::optional<Target> map_standard_action_failure(Source status) {
  switch (status) {
  case Source::Invalid: return Target::Invalid;
  case Source::Inapplicable: return Target::Inapplicable;
  case Source::ResourceExhausted: return Target::ResourceExhausted;
  case Source::Interrupted: return Target::Interrupted;
  case Source::UnsupportedProof: return Target::UnsupportedProof;
  case Source::Stale: return Target::Stale;
  default: return std::nullopt;
  }
}

} // namespace tactix::strategy
