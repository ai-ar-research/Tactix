#undef NDEBUG
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/exact_cover_definition.hpp"
#include "rules/semantic_extension/exact_cover_recognition.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

static std::vector<exact_cover_semantic::Premise> premises(bool missing_edge = false,
                                                            bool bad_cover = false) {
  std::vector<exact_cover_semantic::Premise> result;
  uint64_t occurrence = 0;
  constexpr int blocks = 3, arity = 3;
  for (int block = 0; block < blocks; ++block)
    for (int first = 0; first < arity; ++first)
      for (int second = first + 1; second < arity; ++second) {
        if (missing_edge && block == 1 && first == 0 && second == 2) continue;
        result.push_back({++occurrence,
            {-(block * arity + first + 1), -(block * arity + second + 1)}});
      }
  // Three position columns and a distinct Latin-square partition.  Each item
  // occurs in exactly one clause of each partition.
  for (int position = 0; position < arity; ++position) {
    std::vector<int32_t> clause;
    for (int block = 0; block < blocks; ++block)
      clause.push_back(block * arity + position + 1);
    result.push_back({++occurrence, std::move(clause)});
  }
  for (int diagonal = 0; diagonal < arity; ++diagonal) {
    std::vector<int32_t> clause;
    for (int block = 0; block < blocks; ++block)
      clause.push_back(block * arity + (block + diagonal) % arity + 1);
    result.push_back({++occurrence, std::move(clause)});
  }
  if (bad_cover) result.back().literals.back() = result.back().literals.front();
  return result;
}

static std::string dimacs(const std::vector<exact_cover_semantic::Premise> &source,
                          uint32_t variables = 9,
                          const std::vector<std::vector<int32_t>> &extra = {}) {
  std::ostringstream out;
  out << "p cnf " << variables << ' ' << source.size() + extra.size() << '\n';
  for (const auto &premise : source) {
    for (int32_t literal : premise.literals) out << literal << ' ';
    out << "0\n";
  }
  for (const auto &clause : extra) {
    for (int32_t literal : clause) out << literal << ' ';
    out << "0\n";
  }
  return out.str();
}

static Kernel parse(const std::vector<exact_cover_semantic::Premise> &source) {
  const std::string text = dimacs(source);
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  return kernel;
}

static void exact_recognition_and_provenance() {
  exact_cover_semantic::Image image;
  uint64_t work = 0;
  auto charge = [&](uint64_t amount) { work += amount; return true; };
  const auto source = premises();
  assert(exact_cover_semantic::recognize(source, 3, 9, charge, image));
  assert(image.arity == 3 && image.blocks.size() == 3);
  assert(image.covers.size() == 6 && image.items.size() == 9);
  for (const auto &block : image.blocks) {
    assert(block.items.size() == 3 && block.exclusions.size() == 9);
    for (uint32_t first = 0; first < 3; ++first)
      for (uint32_t second = first + 1; second < 3; ++second)
        assert(block.exclusions[first * 3 + second]);
  }
  for (const auto &item : image.items)
    assert(item.covers[0] != item.covers[1]);
  assert(work > source.size());
}

static void near_misses_and_budget_are_pure() {
  exact_cover_semantic::Image sentinel;
  sentinel.arity = 17;
  auto unlimited = [](uint64_t) { return true; };
  assert(!exact_cover_semantic::recognize(premises(true), 3, 9, unlimited,
                                           sentinel));
  assert(sentinel.arity == 17 && sentinel.blocks.empty());
  assert(!exact_cover_semantic::recognize(premises(false, true), 3, 9,
                                           unlimited, sentinel));
  assert(sentinel.arity == 17 && sentinel.blocks.empty());
  uint64_t left = 7;
  auto bounded = [&](uint64_t amount) {
    if (amount > left) return false;
    left -= amount;
    return true;
  };
  assert(!exact_cover_semantic::recognize(premises(), 3, 9, bounded, sentinel));
  assert(sentinel.arity == 17 && sentinel.blocks.empty());
}

static ExactCoverDefinitionRequest request(FILE *records) {
  ExactCoverDefinitionRequest result;
  result.maximum_clauses = 100;
  result.maximum_literals = 1000;
  result.recognition_work = 100000;
  result.maximum_items = 100;
  result.block_arity = 3;
  result.block = 1;
  result.maximum_external_extent = 100;
  result.maximum_additions = 4;
  result.action_id = 41;
  result.semantic_id = 73;
  result.records = records;
  return result;
}

static void conservative_live_definition() {
  Kernel kernel = parse(premises());
  FILE *records = std::tmpfile();
  assert(records);
  const uint32_t old_variables = kernel.nvars;
  const uint32_t old_output = kernel_num_output_vars(kernel);
  const auto result = define_exact_cover_block_occupancy(kernel, request(records));
  assert(result.result == ApplyResult::Ok && result.blocks == 3);
  assert(result.covers == 6 && result.items == 9);
  assert(result.output == 10 && result.additions == 4);
  assert(kernel.nvars == old_variables + 1);
  assert(kernel_num_output_vars(kernel) == old_output);

  // The chosen block is {4,5,6}.  The new literal is a real live Boolean
  // definition: one selected item forces occupancy, while false occupancy
  // propagates every signed item false.
  kernel_assign_decision(kernel, kernel_internalize(kernel, 4));
  assert(propagate_service(kernel) == ApplyResult::Ok);
  assert(kernel.value(kernel_internalize(kernel, 10)) > 0);
  std::rewind(records);
  std::string journal;
  char buffer[1024];
  while (const size_t count = std::fread(buffer, 1, sizeof buffer, records))
    journal.append(buffer, count);
  assert(journal.find("exact-cover-image") != std::string::npos);
  assert(journal.find("exact-cover-block") != std::string::npos);
  assert(journal.find("exact-cover-cover") != std::string::npos);
  assert(journal.find("omit-fresh-output") != std::string::npos);
  std::fclose(records);

  Kernel negative = parse(premises());
  records = std::tmpfile();
  assert(records);
  const auto second = define_exact_cover_block_occupancy(negative, request(records));
  assert(second.result == ApplyResult::Ok);
  kernel_assign_decision(negative, kernel_internalize(negative, -10));
  assert(propagate_service(negative) == ApplyResult::Ok);
  for (int item : {-4, -5, -6})
    assert(negative.value(kernel_internalize(negative, item)) > 0);
  std::fclose(records);
}

static void failed_application_has_no_effect() {
  for (bool structural : {false, true}) {
    Kernel kernel = parse(structural ? premises(true) : premises());
    FILE *records = std::tmpfile();
    assert(records);
    auto limited = request(records);
    if (!structural) limited.recognition_work = 3;
    const auto mapping = kernel.external_to_internal;
    const uint64_t id = kernel.next_clause_id;
    const size_t arena = kernel.arena.size();
    assert(define_exact_cover_block_occupancy(kernel, limited).result ==
           ApplyResult::NoOp);
    assert(kernel.external_to_internal == mapping && kernel.next_clause_id == id);
    assert(kernel.arena.size() == arena && std::ftell(records) == 0);
    std::fclose(records);
  }
}

static void write_certificate(const char *cnf_path, const char *proof_path,
                              ProofFormat format) {
  const auto source = premises();
  // This four-clause gadget is not part of the recognized kernel snapshot.
  // It gives the independent checker a short refutation after it has checked
  // every extension line against the exact-cover source prefix.
  const std::vector<std::vector<int32_t>> gadget = {
      {11, 12}, {-11, 12}, {11, -12}, {-11, -12}};
  const std::string checker_cnf = dimacs(source, 12, gadget);
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  assert(std::fwrite(checker_cnf.data(), 1, checker_cnf.size(), cnf) ==
         checker_cnf.size());
  std::fclose(cnf);

  Kernel kernel = parse(source);
  kernel.next_clause_id = source.size() + gadget.size();
  Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *records = std::tmpfile();
  assert(records);
  const auto result = define_exact_cover_block_occupancy(kernel, request(records));
  assert(result.result == ApplyResult::Ok && result.output == 10);
  const int32_t unit = 11;
  assert(proof.add_external_rup(++kernel.next_clause_id, &unit, 1));
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

int main(int argc, char **argv) {
  exact_recognition_and_provenance();
  near_misses_and_budget_are_pure();
  conservative_live_definition();
  failed_application_has_no_effect();
  if (argc == 4) {
    write_certificate(argv[1], argv[2], ProofFormat::DsrAscii);
    write_certificate(argv[1], argv[3], ProofFormat::DratAscii);
  }
  std::puts("test_exact_cover_semantic_extension: passed");
}
