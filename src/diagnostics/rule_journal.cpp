#include "diagnostics/rule_journal.hpp"

#ifdef TACTIX_ENABLE_RULE_JOURNAL

#include "kernel/kernel.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
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
  std::string candidate_contains;
  int literal = 0;
  uint64_t clause_id = 0;
  std::string replay_line;
};

JournalState state;

struct TransactionState {
  bool active = false;
  std::string rule;
  std::string candidate;
  std::vector<std::string> deltas;
  std::vector<std::string> antecedents;
  std::vector<std::string> witnesses;
};

thread_local TransactionState transaction;
thread_local RuleJournalMeter effort_meter;

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
  if (const char *candidate_contains =
          std::getenv("TACTIX_RULE_JOURNAL_CANDIDATE_CONTAINS"))
    state.candidate_contains = candidate_contains;
  if (const char *literal =
          std::getenv("TACTIX_RULE_JOURNAL_LITERAL"))
    state.literal = std::atoi(literal);
  if (const char *id = std::getenv("TACTIX_RULE_JOURNAL_CLAUSE_ID"))
    state.clause_id = std::strtoull(id, nullptr, 10);
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
  // Schedule-only diagnostics need conflict, admission, decline, effort, and
  // budget fields but not an O(formula log formula) transaction digest. Keep
  // this shared compile-time arm explicit so large late-boundary probes do not
  // turn into formula-export benchmarks.
  if (std::getenv("SAT_RULE_JOURNAL_SKIP_DIGEST"))
    return 0;
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
  if (k.binary_ids.tracks_keys()) {
    k.binary_ids.for_each([&](uint64_t key, uint64_t id) {
      facts.push_back(mix(mix(UINT64_C(0xB1), key), id));
    });
  } else {
    // DRAT production does not retain an exact-key proof archive. Keep this
    // compile-time diagnostic complete by deriving the same distinct pair set
    // from authoritative watches, without creating a production consumer.
    std::vector<uint64_t> binary_keys;
    binary_keys.reserve(k.binary_ids.size());
    for (Lit lit = 0; lit < 2 * k.nvars; lit++)
      for (const WatchEntry watch : k.watches.entries(lit))
        if (watch.binary() && lit < watch.other())
          binary_keys.push_back(kernel_binary_key(lit, watch.other()));
    std::sort(binary_keys.begin(), binary_keys.end());
    binary_keys.erase(std::unique(binary_keys.begin(), binary_keys.end()),
                      binary_keys.end());
    for (const uint64_t key : binary_keys)
      facts.push_back(mix(mix(UINT64_C(0xB1), key), UINT64_C(1)));
  }
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
    ref += clause_actual_words(clause);
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
  if (!state.candidate_contains.empty() &&
      candidate.find(state.candidate_contains) == std::string::npos)
    return false;
  return true;
}

void rule_journal_set_effort_meter(RuleJournalMeter meter) {
  effort_meter = meter;
}

uint64_t rule_journal_effort(const Kernel &k) {
  return effort_meter ? effort_meter(k) : k.stats.ticks;
}

int rule_journal_selected_literal() {
  open_journal();
  return state.literal;
}

uint64_t rule_journal_selected_clause_id() {
  open_journal();
  return state.clause_id;
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

void rule_journal_transaction_begin(const char *rule,
                                    const std::string &candidate) {
  assert(!transaction.active);
  transaction = TransactionState{};
  transaction.active = true;
  transaction.rule = rule;
  transaction.candidate = candidate;
}

void rule_journal_note_binary_add(const Kernel &k, uint32_t a, uint32_t b,
                                  uint64_t id) {
  if (!transaction.active)
    return;
  if (state.clause_id && id != state.clause_id)
    return;
  const int ea = kernel_externalize(k, a);
  const int eb = kernel_externalize(k, b);
  if (state.literal && ea != state.literal && ea != -state.literal &&
      eb != state.literal && eb != -state.literal)
    return;
  transaction.deltas.push_back("add-binary(id=" + std::to_string(id) +
                               ";lits=" + std::to_string(ea) + ',' +
                               std::to_string(eb) + ')');
}

void rule_journal_note_binary_rewrite(const Kernel &k, uint32_t source_a,
                                      uint32_t source_b, uint32_t target_a,
                                      uint32_t target_b) {
  if (!transaction.active)
    return;
  const int sa = kernel_externalize(k, source_a);
  const int sb = kernel_externalize(k, source_b);
  const int ta = kernel_externalize(k, target_a);
  const int tb = kernel_externalize(k, target_b);
  if (state.literal && std::abs(sa) != std::abs(state.literal) &&
      std::abs(sb) != std::abs(state.literal) &&
      std::abs(ta) != std::abs(state.literal) &&
      std::abs(tb) != std::abs(state.literal))
    return;
  transaction.deltas.push_back(
      "rewrite-binary(source=" + std::to_string(sa) + ',' +
      std::to_string(sb) + ";target=" + std::to_string(ta) + ',' +
      std::to_string(tb) + ')');
}

void rule_journal_note_large_add(const Kernel &k, const uint32_t *lits,
                                 uint32_t size, uint64_t id, bool redundant,
                                 uint32_t glue) {
  bool selected = (!state.literal && !state.clause_id) ||
                  id == state.clause_id;
  std::string external;
  for (uint32_t i = 0; i < size; i++) {
    const int lit = kernel_externalize(k, lits[i]);
    selected |= std::abs(lit) == std::abs(state.literal);
    if (i)
      external.push_back(',');
    external += std::to_string(lit);
  }
  if (!selected)
    return;
  const std::string delta =
      "add-large(id=" + std::to_string(id) +
      ";redundant=" + std::to_string(redundant) +
      ";glue=" + std::to_string(glue) + ";lits=" + external + ')';
  if (transaction.active) {
    transaction.deltas.push_back(delta);
  } else if (state.clause_id == id) {
    // A selected mutation outside a registry bracket is itself actionable:
    // emit it rather than silently losing it, so the missing transaction
    // boundary can be repaired at the actual caller.
    RuleJournalRecord record;
    record.kind = "control";
    record.rule = "Kernel";
    record.event = "unbracketed-add-large";
    record.candidate = "clause-id=" + std::to_string(id);
    record.trigger = "kernel-add-large";
    record.context = "level=" + std::to_string(k.level);
    record.pre_digest = rule_journal_digest(k);
    record.guards = "size-ge-3=1";
    record.antecedents = "caller-unbracketed";
    record.outcome = "applied";
    record.decline = "none";
    record.deltas = delta;
    record.witnesses = "missing-rule-transaction";
    record.post_digest = record.pre_digest;
    rule_journal_emit(k, record);
  }
}

void rule_journal_note_clause_transition(const Kernel &k, const char *event,
                                         const uint32_t *lits, uint32_t size,
                                         uint64_t id) {
  if (!transaction.active)
    return;
  if (state.clause_id && id != state.clause_id)
    return;
  std::string external;
  for (uint32_t i = 0; i < size; i++) {
    if (i)
      external.push_back(',');
    external += std::to_string(kernel_externalize(k, lits[i]));
  }
  transaction.deltas.push_back(std::string(event) + "(id=" +
                               std::to_string(id) + ";lits=" + external +
                               ')');
}

void rule_journal_note_root_assignment(const Kernel &k, uint32_t lit,
                                       uint32_t reason, bool binary) {
  if (!transaction.active)
    return;
  const int external = kernel_externalize(k, lit);
  if (state.literal && std::abs(external) != std::abs(state.literal))
    return;
  transaction.deltas.push_back(
      "root-unit(lit=" + std::to_string(external) +
      ";reason=" + std::to_string(reason) +
      ";binary=" + std::to_string(binary) + ')');
}

void rule_journal_note_variable_status(const Kernel &k, uint32_t var,
                                       const char *status) {
  if (!transaction.active)
    return;
  const int external = kernel_externalize(k, make_lit(var, false));
  if (state.literal && std::abs(external) != std::abs(state.literal))
    return;
  transaction.deltas.push_back("retire-var(var=" + std::to_string(external) +
                               ";status=" + status + ')');
}

void rule_journal_note_reconstruction(const Kernel &k, uint32_t witness,
                                      const uint32_t *rest, uint32_t nrest) {
  if (!transaction.active)
    return;
  const int external_witness = kernel_externalize(k, witness);
  bool selected = !state.literal ||
                  std::abs(external_witness) == std::abs(state.literal);
  std::string external_rest;
  for (uint32_t i = 0; i < nrest; i++) {
    const int lit = kernel_externalize(k, rest[i]);
    selected |= std::abs(lit) == std::abs(state.literal);
    if (i)
      external_rest.push_back(',');
    external_rest += std::to_string(lit);
  }
  if (!selected)
    return;
  transaction.witnesses.push_back(
      "reconstruct(witness=" + std::to_string(external_witness) +
      ";rest=" + external_rest + ')');
}

void rule_journal_note_rup(uint64_t id, const uint64_t *chain,
                           uint32_t nchain) {
  if (!transaction.active)
    return;
  std::string item = "rup(id=" + std::to_string(id) + ";chain=";
  if (!nchain)
    item += "implicit";
  for (uint32_t i = 0; i < nchain; i++) {
    if (i)
      item.push_back(',');
    item += std::to_string(chain[i]);
  }
  item.push_back(')');
  transaction.antecedents.push_back(std::move(item));
}

void rule_journal_note_rat(uint64_t id, const int64_t *hints,
                           uint32_t nhints) {
  if (!transaction.active)
    return;
  std::string item = "rat(id=" + std::to_string(id) + ";hints=";
  if (!nhints)
    item += "implicit";
  for (uint32_t i = 0; i < nhints; i++) {
    if (i)
      item.push_back(',');
    item += std::to_string(hints[i]);
  }
  item.push_back(')');
  transaction.antecedents.push_back(std::move(item));
}

std::string rule_journal_transaction_deltas() {
  if (!transaction.active || transaction.deltas.empty())
    return "none";
  std::string result;
  for (const std::string &delta : transaction.deltas) {
    if (!result.empty())
      result.push_back('|');
    result += delta;
  }
  return result;
}

std::string rule_journal_transaction_antecedents() {
  if (!transaction.active || transaction.antecedents.empty())
    return "none";
  std::string result;
  for (const std::string &antecedent : transaction.antecedents) {
    if (!result.empty())
      result.push_back('|');
    result += antecedent;
  }
  return result;
}

std::string rule_journal_transaction_witnesses() {
  if (!transaction.active || transaction.witnesses.empty())
    return "none";
  std::string result;
  for (const std::string &witness : transaction.witnesses) {
    if (!result.empty())
      result.push_back('|');
    result += witness;
  }
  return result;
}

void rule_journal_transaction_end() { transaction = TransactionState{}; }

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
  transaction = TransactionState{};
}

} // namespace tactix

#endif
