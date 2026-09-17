#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tactix::orbitopal_group {

struct Generator {
  std::vector<size_t> rows;
  std::vector<size_t> columns;
};

enum class Status : uint8_t {
  Found,
  Absent,
  LimitReached,
  Interrupted,
  Invalid,
};

enum class WordKind : uint8_t { Identity, Generator, Compose, Inverse };

struct WordNode {
  WordKind kind = WordKind::Identity;
  // Generator: first encodes 2*generator+inverse. Compose applies first,
  // then second. Compose/Inverse children must precede their parent.
  uint32_t first = 0;
  uint32_t second = 0;
  bool operator==(const WordNode &) const = default;
};

struct GeneratorWord {
  std::vector<WordNode> nodes;
  uint32_t root = 0;
  bool empty() const { return !root; }
  size_t size() const { return nodes.size(); }
  bool operator==(const GeneratorWord &) const = default;
};

struct Result {
  Status status = Status::Invalid;
  GeneratorWord word;
  size_t first = SIZE_MAX;
  size_t second = SIZE_MAX;
  uint64_t ticks = 0;
  uint64_t states = 0;
};

using StopCallback = bool (*)(void *) noexcept;

struct WordEvaluation {
  Status status = Status::Invalid;
  std::vector<uint32_t> image;
  uint64_t ticks = 0;
};

// Encoded generator slots contain forward/inverse permutations in pairs.
// No partial image is returned on malformed input, interruption or exhaustion.
WordEvaluation evaluate_word(
    const GeneratorWord &, size_t domain,
    const std::vector<std::span<const uint32_t>> &encoded_generators,
    uint64_t maximum_ticks, StopCallback stop = nullptr,
    void *stop_context = nullptr);

// Find a row transposition in the kernel of the induced column action. The
// returned expression shares subwords instead of expanding repetitions. The
// routine stores compact row/column actions only; callers lift the returned
// word through their already-certified full-domain generators.
Result find_kernel_transposition(const std::vector<Generator> &generators,
                                 uint64_t maximum_states,
                                 uint64_t maximum_ticks,
                                 StopCallback stop = nullptr,
                                 void *stop_context = nullptr);

} // namespace tactix::orbitopal_group
