#include "rules/semantic_extension/exact_cover_definition.hpp"

#include "rules/semantic_extension/disjunction.hpp"
#include "rules/semantic_extension/exact_cover_recognition.hpp"

#include <array>
#include <cstdlib>

namespace tactix {

ExactCoverDefinitionResult define_exact_cover_block_occupancy(
    Kernel &k, const ExactCoverDefinitionRequest &request) {
  ExactCoverDefinitionResult result;
  if (!request.records || !request.action_id || !request.semantic_id ||
      request.block_arity < 3 || !request.maximum_clauses ||
      !request.maximum_literals || !request.recognition_work ||
      !request.maximum_items || request.block_arity > request.maximum_items ||
      request.block_arity + 1 > request.maximum_additions ||
      k.inconsistent || k.level || k.conflict_valid || k.pending.valid ||
      !k.trail.empty() || k.dense || k.probing.recording ||
      !k.large_watches_connected || !kernel_propagation_fixpoint(k) ||
      (k.proof && k.proof->pb()) || kernel_terminating(k))
    return result;

  std::vector<exact_cover_semantic::Premise> source;
  uint64_t literal_count = 0;
  auto charge = [&](uint64_t amount) {
    if (kernel_terminating(k) ||
        amount > request.recognition_work - result.recognition_work)
      return false;
    result.recognition_work += amount;
    return true;
  };
  auto take = [&](const Lit *clause, uint32_t size) {
    if (source.size() == request.maximum_clauses ||
        size > request.maximum_literals - literal_count || !charge(size + 1))
      return false;
    exact_cover_semantic::Premise premise;
    premise.occurrence = source.size() + 1;
    premise.literals.reserve(size);
    for (uint32_t i = 0; i < size; ++i) {
      if (kernel_var_retired(k, var_of(clause[i]))) return false;
      premise.literals.push_back(kernel_externalize(k, clause[i]));
    }
    literal_count += size;
    source.push_back(std::move(premise));
    return true;
  };

  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    ref += clause_actual_words(clause);
    if (!charge(1) || (!clause->garbage() &&
                       !take(clause->lits, clause->size)))
      return result;
  }
  for (Lit literal = 0; literal < 2 * k.nvars; ++literal)
    for (const WatchEntry watch : k.watches.entries(literal)) {
      if (!charge(1)) return result;
      if (watch.binary() && literal < watch.other()) {
        const Lit pair[] = {literal, watch.other()};
        if (!take(pair, 2)) return result;
      }
    }

  exact_cover_semantic::Image image;
  if (!exact_cover_semantic::recognize(source, request.block_arity,
                                        request.maximum_items, charge, image) ||
      request.block >= image.blocks.size() || kernel_terminating(k))
    return result;

  // Buffer the generic definition's records until it succeeds, so a decline
  // cannot leave an apparently committed semantic event behind.
  FILE *definition_records = std::tmpfile();
  if (!definition_records) return result;
  DisjunctionDefinitionRequest definition;
  definition.inputs = image.blocks[request.block].items;
  definition.maximum_arity = request.block_arity;
  definition.maximum_external_extent = request.maximum_external_extent;
  definition.maximum_additions = request.maximum_additions;
  definition.action_id = request.action_id;
  definition.semantic_id = request.semantic_id;
  definition.records = definition_records;
  const DisjunctionDefinitionResult defined =
      define_disjunction(k, definition);
  if (defined.result != ApplyResult::Ok) {
    std::fclose(definition_records);
    return result;
  }

  result.result = ApplyResult::Ok;
  result.blocks = static_cast<uint32_t>(image.blocks.size());
  result.covers = static_cast<uint32_t>(image.covers.size());
  result.items = image.items.size();
  result.output = defined.output;
  result.additions = defined.additions;
  std::fprintf(request.records,
      "{\"kind\":\"exact-cover-image\",\"action\":%llu,"
      "\"semantic\":%llu,\"arity\":%u,\"blocks\":%u,\"covers\":%u,"
      "\"items\":%llu,\"selected_block\":%u,"
      "\"meaning\":\"output iff some signed item in selected block\","
      "\"projection\":\"omit-fresh-output\"}\n",
      (unsigned long long)request.action_id,
      (unsigned long long)request.semantic_id, image.arity, result.blocks,
      result.covers, (unsigned long long)result.items, request.block);
  for (uint32_t block = 0; block < image.blocks.size(); ++block) {
    const auto &recognized = image.blocks[block];
    std::fprintf(request.records,
        "{\"kind\":\"exact-cover-block\",\"action\":%llu,"
        "\"semantic\":%llu,\"block\":%u,\"items\":[",
        (unsigned long long)request.action_id,
        (unsigned long long)request.semantic_id, block);
    for (uint32_t i = 0; i < recognized.items.size(); ++i)
      std::fprintf(request.records, "%s%d", i ? "," : "", recognized.items[i]);
    std::fputs("],\"exclusions\":[", request.records);
    bool comma = false;
    for (uint32_t first = 0; first < image.arity; ++first)
      for (uint32_t second = first + 1; second < image.arity; ++second) {
        std::fprintf(request.records, "%s%llu", comma ? "," : "",
            (unsigned long long)recognized.exclusions[
                size_t(first) * image.arity + second]);
        comma = true;
      }
    std::fputs("]}\n", request.records);
  }
  for (uint32_t cover = 0; cover < image.covers.size(); ++cover) {
    const auto &recognized = image.covers[cover];
    std::fprintf(request.records,
        "{\"kind\":\"exact-cover-cover\",\"action\":%llu,"
        "\"semantic\":%llu,\"cover\":%u,\"premise\":%llu,\"items\":[",
        (unsigned long long)request.action_id,
        (unsigned long long)request.semantic_id, cover,
        (unsigned long long)recognized.premise);
    for (uint32_t i = 0; i < recognized.items.size(); ++i)
      std::fprintf(request.records, "%s%d", i ? "," : "", recognized.items[i]);
    std::fputs("]}\n", request.records);
  }
  std::rewind(definition_records);
  std::array<char, 4096> buffer{};
  for (;;) {
    const size_t count = std::fread(buffer.data(), 1, buffer.size(),
                                    definition_records);
    if (count && std::fwrite(buffer.data(), 1, count, request.records) != count)
      std::abort();
    if (count != buffer.size()) {
      if (std::ferror(definition_records)) std::abort();
      break;
    }
  }
  std::fclose(definition_records);
  if (std::fflush(request.records) || std::ferror(request.records)) std::abort();
  return result;
}

} // namespace tactix
