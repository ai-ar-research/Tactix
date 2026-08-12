#include "diagnostics/rule_journal.hpp"

#ifdef TACTIX_ENABLE_RULE_JOURNAL

#include "kernel/kernel.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace tactix {
namespace {

struct JournalState {
  bool initialized = false;
  FILE *output = nullptr;
  FILE *replay = nullptr;
  uint64_t sequence = 0;
  uint64_t emitted = 0;
  uint64_t limit = UINT64_MAX;
  uint64_t conflict_from = 0;
  uint64_t conflict_to = UINT64_MAX;
  std::string rule;
  std::string candidate;
  std::string replay_line;
};

JournalState state;

static uint64_t mix(uint64_t hash, uint64_t value) {
  return (hash ^ value) * UINT64_C(1099511628211);
}

static std::string escape(const std::string &input) {
  std::string out;
  for (const unsigned char c : input) {
    if (c == '%' || c == '\t' || c == '\n' || c == '\r') {
      char encoded[4];
      std::snprintf(encoded, sizeof encoded, "%%%02X", c);
      out += encoded;
    } else {
      out += static_cast<char>(c);
    }
  }
  return out;
}

static void open_journal() {
  if (state.initialized)
    return;
  state.initialized = true;
  if (const char *limit = std::getenv("TACTIX_RULE_JOURNAL_LIMIT"))
    state.limit = std::strtoull(limit, nullptr, 10);
  if (const char *from =
          std::getenv("TACTIX_RULE_JOURNAL_CONFLICT_FROM"))
    state.conflict_from = std::strtoull(from, nullptr, 10);
  if (const char *to = std::getenv("TACTIX_RULE_JOURNAL_CONFLICT_TO"))
    state.conflict_to = std::strtoull(to, nullptr, 10);
  if (const char *rule = std::getenv("TACTIX_RULE_JOURNAL_RULE"))
    state.rule = rule;
  if (const char *candidate =
          std::getenv("TACTIX_RULE_JOURNAL_CANDIDATE"))
    state.candidate = candidate;
  if (const char *path = std::getenv("TACTIX_RULE_JOURNAL")) {
    state.output = std::fopen(path, "w");
    if (!state.output) {
      std::fprintf(stderr, "tactix: can not open rule journal '%s': %s\n", path,
                   std::strerror(errno));
      std::abort();
    }
    std::fputs("# tactix-rule-journal\t1\n", state.output);
    std::fputs("# solver\tseq\tkind\trule\tevent\tconflicts\tcandidate\t"
               "trigger\tcontext\tformula_epoch\troot_epoch\tpre_digest\t"
               "guards\tantecedents\toutcome\tdecline\tdeltas\twitnesses\t"
               "effort\tbudget\tpost_digest\n",
               state.output);
  }
  if (const char *path = std::getenv("TACTIX_RULE_JOURNAL_REPLAY")) {
    state.replay = std::fopen(path, "r");
    if (!state.replay) {
      std::fprintf(stderr, "tactix: can not open replay journal '%s': %s\n",
                   path, std::strerror(errno));
      std::abort();
    }
  }
}

static bool next_replay_line(std::string &line) {
  char *buffer = nullptr;
  size_t capacity = 0;
  while (true) {
    const ssize_t size = getline(&buffer, &capacity, state.replay);
    if (size < 0) {
      std::free(buffer);
      return false;
    }
    line.assign(buffer, static_cast<size_t>(size));
    std::free(buffer);
    if (!line.empty() && line.back() == '\n')
      line.pop_back();
    if (!line.empty() && line[0] != '#')
      return true;
    buffer = nullptr;
    capacity = 0;
  }
}

static std::vector<std::string> fields(const std::string &line) {
  std::vector<std::string> result;
  size_t begin = 0;
  while (true) {
    const size_t end = line.find('\t', begin);
    result.push_back(line.substr(begin, end - begin));
    if (end == std::string::npos)
      return result;
    begin = end + 1;
  }
}

static void verify_line(const std::string &actual) {
  std::string expected;
  if (!next_replay_line(expected)) {
    std::fprintf(stderr,
                 "rule-journal replay mismatch at record %llu: unexpected "
                 "actual record\n  actual: %s\n",
                 static_cast<unsigned long long>(state.emitted), actual.c_str());
    std::abort();
  }
  if (expected == actual)
    return;
  static const char *names[] = {
      "solver",       "seq",          "kind",       "rule",
      "event",        "conflicts",    "candidate",  "trigger",
      "context",      "formula_epoch", "root_epoch", "pre_digest",
      "guards",       "antecedents",  "outcome",    "decline",
      "deltas",       "witnesses",    "effort",     "budget",
      "post_digest"};
  const std::vector<std::string> want = fields(expected);
  const std::vector<std::string> got = fields(actual);
  size_t differing = 0;
  while (differing < want.size() && differing < got.size() &&
         want[differing] == got[differing])
    differing++;
  const char *name = differing < sizeof names / sizeof *names
                         ? names[differing]
                         : "field-count";
  const char *wanted = differing < want.size() ? want[differing].c_str() : "<missing>";
  const char *actual_value =
      differing < got.size() ? got[differing].c_str() : "<missing>";
  std::fprintf(stderr,
               "rule-journal replay mismatch at record %llu field '%s'\n"
               "  expected: %s\n  actual:   %s\n",
               static_cast<unsigned long long>(state.emitted), name, wanted,
               actual_value);
  std::abort();
}

} // namespace

uint64_t rule_journal_digest(const Kernel &k) {
  uint64_t hash = UINT64_C(1469598103934665603);
  hash = mix(hash, k.inconsistent);
  hash = mix(hash, k.level);
  hash = mix(hash, k.propagate_head);
  hash = mix(hash, k.trail.size());
  std::vector<uint64_t> facts;
  facts.reserve(k.nvars + k.binary_ids.size() + k.arena.size() / 8);
  for (Var v = 0; v < k.nvars; v++) {
    const int external_lit = kernel_externalize(k, make_lit(v, false));
    const uint64_t external = static_cast<uint64_t>(
        external_lit < 0 ? -static_cast<int64_t>(external_lit)
                         : static_cast<int64_t>(external_lit));
    const uint64_t value = static_cast<uint64_t>(k.values[make_lit(v, false)] + 1);
    const uint64_t status = static_cast<uint64_t>(
        v < k.var_status.size() ? k.var_status[v] : VarStatus::Active);
    facts.push_back((external << 4) ^ (status << 2) ^ value);
  }
  for (const auto &entry : k.binary_ids)
    facts.push_back(mix(mix(UINT64_C(0xB1), entry.first), entry.second));
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage()) {
      uint64_t item = mix(UINT64_C(0xC1), clause->id());
      item = mix(item, clause->redundant());
      std::vector<int> lits;
      for (const Lit lit : *clause)
        lits.push_back(kernel_externalize(k, lit));
      std::sort(lits.begin(), lits.end());
      for (const int lit : lits)
        item = mix(item, static_cast<uint64_t>(static_cast<int64_t>(lit)));
      facts.push_back(item);
    }
    ref += clause_words(clause->size);
  }
  std::sort(facts.begin(), facts.end());
  for (const uint64_t fact : facts)
    hash = mix(hash, fact);
  return hash;
}

bool rule_journal_wants(const Kernel &k, const char *rule,
                        const std::string &candidate) {
  open_journal();
  if (!state.output && !state.replay)
    return false;
  if (k.stats.conflicts < state.conflict_from ||
      k.stats.conflicts > state.conflict_to || state.emitted >= state.limit)
    return false;
  if (!state.rule.empty() && state.rule != rule)
    return false;
  if (!state.candidate.empty() && state.candidate != candidate)
    return false;
  return true;
}

void rule_journal_emit(const Kernel &k, const RuleJournalRecord &record) {
  open_journal();
  if (!rule_journal_wants(k, record.rule, record.candidate))
    return;
  const uint64_t sequence = state.sequence++;
  const auto decimal = [](uint64_t value) { return std::to_string(value); };
  const auto hexadecimal = [](uint64_t value) {
    char buffer[17];
    std::snprintf(buffer, sizeof buffer, "%016llx",
                  static_cast<unsigned long long>(value));
    return std::string(buffer);
  };
  const std::string columns[] = {
      "tactix",
      decimal(sequence),
      record.kind,
      record.rule,
      record.event,
      decimal(k.stats.conflicts),
      escape(record.candidate),
      escape(record.trigger),
      escape(record.context),
      decimal(record.formula_epoch),
      decimal(record.root_epoch),
      hexadecimal(record.pre_digest),
      escape(record.guards),
      escape(record.antecedents),
      record.outcome,
      record.decline,
      escape(record.deltas),
      escape(record.witnesses),
      decimal(record.effort),
      decimal(record.budget),
      hexadecimal(record.post_digest),
  };
  std::string line;
  for (const std::string &column : columns) {
    if (!line.empty())
      line.push_back('\t');
    line += column;
  }
  state.emitted++;
  if (state.output) {
    std::fprintf(state.output, "%s\n", line.c_str());
    std::fflush(state.output);
  }
  if (state.replay)
    verify_line(line);
}

void rule_journal_close() {
  if (!state.initialized)
    return;
  if (state.replay) {
    std::string extra;
    if (next_replay_line(extra)) {
      std::fprintf(stderr,
                   "rule-journal replay mismatch after record %llu: expected "
                   "additional record\n  expected: %s\n",
                   static_cast<unsigned long long>(state.emitted), extra.c_str());
      std::abort();
    }
    std::fclose(state.replay);
  }
  if (state.output)
    std::fclose(state.output);
  state = JournalState{};
}

} // namespace tactix

#endif
