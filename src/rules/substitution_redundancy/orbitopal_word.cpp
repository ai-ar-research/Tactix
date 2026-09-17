#include "rules/substitution_redundancy/orbitopal_group.hpp"

#include <algorithm>

namespace tactix::orbitopal_group {

WordEvaluation evaluate_word(
    const GeneratorWord &word, size_t domain,
    const std::vector<std::span<const uint32_t>> &generators,
    uint64_t maximum_ticks, StopCallback stop, void *context) {
  WordEvaluation result;
  const auto poll = [&] {
    if (!stop || !stop(context))
      return true;
    result.status = Status::Interrupted;
    return false;
  };
  const auto charge = [&](uint64_t amount) {
    if (!poll())
      return false;
    if (result.ticks >= maximum_ticks ||
        amount > maximum_ticks - result.ticks) {
      result.ticks = maximum_ticks;
      result.status = Status::LimitReached;
      return false;
    }
    result.ticks += amount;
    return true;
  };
  if (!domain || domain > UINT32_MAX || word.nodes.empty() ||
      word.nodes.size() > UINT32_MAX ||
      word.root >= word.nodes.size() ||
      word.nodes.front().kind != WordKind::Identity || !poll())
    return result;

  std::vector<uint64_t> users(word.nodes.size(), 0);
  ++users[word.root];
  for (size_t at = 0; at < word.nodes.size(); ++at) {
    if (!charge(1))
      return result;
    const auto &node = word.nodes[at];
    switch (node.kind) {
    case WordKind::Identity:
      if (at)
        return result;
      break;
    case WordKind::Generator:
      if (node.first >= generators.size() ||
          generators[node.first].size() != domain)
        return result;
      break;
    case WordKind::Compose:
      if (node.first >= at || node.second >= at)
        return result;
      ++users[node.first];
      ++users[node.second];
      break;
    case WordKind::Inverse:
      if (node.first >= at)
        return result;
      ++users[node.first];
      break;
    default:
      return result;
    }
  }

  std::vector<std::vector<uint32_t>> owned(word.nodes.size());
  std::vector<std::span<const uint32_t>> images(word.nodes.size());
  std::vector<uint8_t> seen(domain);
  const auto release = [&](uint32_t child) {
    if (!--users[child]) {
      images[child] = {};
      std::vector<uint32_t>().swap(owned[child]);
    }
  };
  for (size_t at = 0; at < word.nodes.size(); ++at) {
    if (!poll())
      return result;
    if (!users[at])
      continue;
    if (!charge(domain))
      return result;
    const auto &node = word.nodes[at];
    if (node.kind == WordKind::Generator) {
      const auto source = generators[node.first];
      std::fill(seen.begin(), seen.end(), 0);
      for (size_t point = 0; point < domain; ++point) {
        if (!poll())
          return result;
        const uint32_t target = source[point];
        if (target >= domain || seen[target])
          return result;
        seen[target] = 1;
      }
      images[at] = source;
      continue;
    }
    auto &image = owned[at];
    image.resize(domain);
    for (size_t point = 0; point < domain; ++point) {
      if (!poll())
        return result;
      if (node.kind == WordKind::Identity)
        image[point] = static_cast<uint32_t>(point);
      else if (node.kind == WordKind::Compose)
        image[point] = images[node.second][images[node.first][point]];
      else
        image[images[node.first][point]] = static_cast<uint32_t>(point);
    }
    images[at] = image;
    if (node.kind == WordKind::Compose) {
      release(node.first);
      release(node.second);
    } else if (node.kind == WordKind::Inverse) {
      release(node.first);
    }
  }
  if (!poll())
    return result;
  if (owned[word.root].empty()) {
    if (!charge(domain))
      return result;
    result.image.assign(images[word.root].begin(), images[word.root].end());
  } else {
    result.image = std::move(owned[word.root]);
  }
  result.status = Status::Found;
  return result;
}

} // namespace tactix::orbitopal_group
