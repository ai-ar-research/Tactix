#include "strategy/feedback_trace.hpp"

#include <charconv>
#include <climits>
#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>

namespace tactix::strategy::controller {
namespace {

std::string quote(const std::string &value) {
  std::string result = "\"";
  static constexpr char hex[] = "0123456789abcdef";
  for (unsigned char ch : value) {
    if (ch == '"' || ch == '\\') { result += '\\'; result += ch; }
    else if (ch < 32) {
      result += "\\u00";
      result += hex[ch >> 4]; result += hex[ch & 15];
    } else result += ch;
  }
  return result + '"';
}

void state(std::ostream &out, const StateVersion &version) {
  out << "{\"arena\":" << version.arena_generation << ",\"touch\":"
      << version.touch_epoch << ",\"strategy\":" << version.strategy_epoch << '}';
}

template <class T> void native_value(std::ostream &out, const T &value) {
  out << value;
}
template <class T, size_t N>
void native_value(std::ostream &out, const T (&value)[N]) {
  out << '[';
  for (size_t i = 0; i < N; ++i) {
    if (i) out << ',';
    native_value(out, value[i]);
  }
  out << ']';
}

void outcome(std::ostream &out, const FeedbackOutcome &value) {
  const auto &r = value.invocation;
  out << "{\"status\":" << unsigned(r.status) << ",\"result\":"
      << unsigned(r.rule_result) << ",\"budget_status\":" << unsigned(r.budget_status)
      << ",\"unit\":" << unsigned(r.work_unit) << ",\"work\":" << r.work
      << ",\"applications\":" << r.applications << ",\"step\":" << r.step_id
      << ",\"instance\":" << r.instance << ",\"progress\":" << r.made_rule_progress
      << ",\"discovery_progress\":" << r.discovery_progress_units
      << ",\"discovery_entry_required\":" << r.discovery_entry_required
      << ",\"discovery_exhausted\":" << r.discovery_exhausted
      << ",\"selection_complete\":" << r.selection_complete
      << ",\"revision\":" << r.configuration_revision << ",\"before\":";
  state(out, r.before);
  out << ",\"after\":"; state(out, r.after);
  out << ",\"composition_overshoot\":" << value.composition_overshoot
      << ",\"resumed_search\":" << value.resumed_search << '}';
}

void request(std::ostream &out, const ActionRequest &value) {
  out << "{\"version\":" << value.version << ",\"sequence\":" << value.sequence
      << ",\"type\":" << unsigned(value.type) << ",\"expected\":";
  state(out, value.expected);
  out << ",\"search\":" << quote(name(value.search))
      << ",\"search_components\":[" << unsigned(value.search.branching) << ','
      << unsigned(value.search.restart) << ',' << unsigned(value.search.phase)
      << "],\"invocation\":";
  OnlineStrategyProgram program;
  if (value.type == ActionType::Rule) program.sequence.push_back(value.invocation);
  out << quote(serialize_online_strategy_program(program));
  if (!value.producer_effort.empty()) {
    out << ",\"producer_effort\":[";
    for (size_t index = 0; index < value.producer_effort.size(); ++index) {
      if (index) out << ',';
      const auto &effort = value.producer_effort[index];
      out << "{\"family\":" << quote(effort.family)
          << ",\"status\":" << unsigned(effort.status)
          << ",\"complete\":" << effort.complete
          << ",\"preparation_work\":" << effort.preparation_work
          << ",\"enumeration_work\":" << effort.enumeration_work
          << ",\"total_work\":" << effort.total_work
          << ",\"candidates\":" << effort.candidates
          << ",\"offers\":" << effort.offers;
      if (effort.have_detector_census)
        out << ",\"detector_census\":{\"candidate_domains\":"
            << effort.candidate_domains << ",\"exact_domains\":"
            << effort.exact_domains << ",\"relation_keys\":"
            << effort.relation_keys << ",\"tuple_occurrences\":"
            << effort.tuple_occurrences << ",\"first_decline_reason\":"
            << unsigned(effort.first_decline_reason) << '}';
      out << '}';
    }
    out << ']';
  }
  out << '}';
}

void allowance(std::ostream &out, const ActionAllowance &value) {
  out << "{\"unit\":" << unsigned(value.unit) << ",\"amount\":" << value.amount
      << ",\"native_mode\":" << unsigned(value.native.mode)
      << ",\"native_amount\":" << value.native.amount << '}';
}

void native_request(std::ostream &out, const NativeActionDescription &value) {
  out << "{\"version\":" << value.version
      << ",\"description\":" << quote(value.description) << ",\"invocation\":";
  if (value.invocation) {
    OnlineStrategyProgram program;
    program.sequence.push_back(*value.invocation);
    out << quote(serialize_online_strategy_program(program));
  } else out << "null";
  out << ",\"currency\":" << quote(value.effort_currency)
      << ",\"unlimited\":" << value.effort_allowance.is_unlimited()
      << ",\"amount\":" << value.effort_allowance.amount()
      << ",\"work\":" << value.work << ",\"overshoot\":" << value.overshoot
      << ",\"accounting_overflow\":" << value.accounting_overflow << '}';
}

std::ostringstream stream() {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::boolalpha << std::setprecision(17);
  return out;
}

template <class T> bool native_equal(const T &left, const T &right) {
  return left == right;
}
template <class T, size_t N>
bool native_equal(const T (&left)[N], const T (&right)[N]) {
  for (size_t i = 0; i < N; ++i)
    if (!native_equal(left[i], right[i])) return false;
  return true;
}

template <class T>
void native_array_delta(std::ostream &out, const T &left, const T &right,
                        size_t &index, bool &first) {
  if (left != right) {
    if (!first) out << ',';
    first = false;
    out << '[' << index << ',';
    native_value(out, right);
    out << ']';
  }
  ++index;
}
template <class T, size_t N>
void native_array_delta(std::ostream &out, const T (&left)[N],
                        const T (&right)[N], size_t &index, bool &first) {
  for (size_t i = 0; i < N; ++i)
    native_array_delta(out, left[i], right[i], index, first);
}

std::string serialized_last_action(const std::optional<ActionResult> &value) {
  if (!value) return "null";
  auto out = stream();
  out << "{\"sequence\":" << value->sequence << ",\"type\":"
      << unsigned(value->type) << ",\"before\":";
  state(out, value->before);
  out << ",\"after\":"; state(out, value->after);
  out << ",\"propagation_ticks\":" << value->propagation_ticks
      << ",\"root_assignments\":" << value->root_assignments
      << ",\"outcome\":";
  outcome(out, feedback_outcome(*value));
  out << '}';
  return out.str();
}

std::string serialized_local_candidates(
    const std::vector<LocalSearchCandidateInfo> &values) {
  auto out = stream();
  out << '[';
  bool first = true;
  for (const auto &candidate : values) {
    if (!first) out << ',';
    first = false;
    out << "{\"id\":" << candidate.id
        << ",\"formula_touch_epoch\":" << candidate.formula_touch_epoch
        << ",\"formula_root_assignments\":" << candidate.formula_root_assignments
        << ",\"external_variables\":" << candidate.external_variables
        << ",\"known_phases\":" << candidate.known_phases
        << ",\"objective\":" << candidate.objective
        << ",\"objective_exact\":" << candidate.objective_exact
        << ",\"effort\":" << candidate.effort.amount
        << ",\"effort_unit\":" << unsigned(candidate.effort.unit) << '}';
  }
  out << ']';
  return out.str();
}

std::string serialized_semantic_candidates(
    const std::vector<SemanticVariableCandidateInfo> &values) {
  auto out = stream();
  out << '[';
  bool first = true;
  for (const auto &candidate : values) {
    if (!first) out << ',';
    first = false;
    out << "{\"id\":" << candidate.id
        << ",\"action_id\":" << candidate.action_id
        << ",\"arena_generation\":" << candidate.arena_generation
        << ",\"touch_epoch\":" << candidate.touch_epoch
        << ",\"strategy_epoch\":" << candidate.strategy_epoch
        << ",\"producer\":" << unsigned(candidate.producer)
        << ",\"existing_variables\":" << candidate.existing_variables
        << ",\"introduced_variables\":" << candidate.introduced_variables
        << '}';
  }
  out << ']';
  return out.str();
}

std::string serialize_observation_delta(const Observation &base,
                                        const Observation &value) {
  auto out = stream();
  out << "{\"set\":{";
  bool first = true;
  auto field = [&](const char *name, const auto &serialized) {
    if (!first) out << ',';
    first = false;
    out << '"' << name << "\":" << serialized;
  };
  auto number = [&](const char *name, const auto &before, const auto &after) {
    if (before != after) field(name, after);
  };
  auto boolean = [&](const char *name, bool before, bool after) {
    if (before != after) field(name, after ? "true" : "false");
  };
  if (base.version != value.version) field("version", value.version);
  if (base.state.arena_generation != value.state.arena_generation ||
      base.state.touch_epoch != value.state.touch_epoch ||
      base.state.strategy_epoch != value.state.strategy_epoch) {
    auto encoded = stream(); state(encoded, value.state); field("state", encoded.str());
  }
  number("sequence", base.sequence, value.sequence);
  number("variables", base.variables, value.variables);
  number("unassigned", base.unassigned, value.unassigned);
  number("active_unassigned", base.active_unassigned, value.active_unassigned);
  number("level", base.level, value.level);
  number("trail", base.trail_assignments, value.trail_assignments);
  number("root_population", base.root_population, value.root_population);
  number("irredundant_clauses", base.irredundant_clauses, value.irredundant_clauses);
  boolean("inconsistent", base.inconsistent, value.inconsistent);
  boolean("conflict", base.conflict, value.conflict);
  boolean("fixpoint", base.propagation_fixpoint, value.propagation_fixpoint);
  boolean("searching", base.searching, value.searching);
  boolean("have_configuration", base.have_search_configuration,
          value.have_search_configuration);
  if (name(base.search) != name(value.search))
    field("configuration", quote(name(value.search)));
  number("rotation_intervals", base.rotation_intervals, value.rotation_intervals);
  number("next_restart_conflicts", base.next_restart_conflicts,
         value.next_restart_conflicts);
  if (base.reluctant_u != value.reluctant_u ||
      base.reluctant_v != value.reluctant_v ||
      base.reluctant_wait != value.reluctant_wait) {
    auto encoded = stream();
    encoded << '[' << value.reluctant_u << ',' << value.reluctant_v << ','
            << value.reluctant_wait << ']';
    field("reluctant", encoded.str());
  }
  number("fast_glue", base.fast_glue, value.fast_glue);
  number("slow_glue", base.slow_glue, value.slow_glue);
  const auto base_last = serialized_last_action(base.last_action);
  const auto value_last = serialized_last_action(value.last_action);
  if (base_last != value_last) field("last_action", value_last);
  if (base.native_policy_state != value.native_policy_state &&
      !value.native_policy_state.empty())
    field("native_policy_state", quote(value.native_policy_state));
  const auto base_local = serialized_local_candidates(base.local_search_candidates);
  const auto value_local = serialized_local_candidates(value.local_search_candidates);
  if (base_local != value_local) field("local_search_candidates", value_local);
  number("local_search_last_target_decision_candidate",
         base.local_search_last_target_decision_candidate,
         value.local_search_last_target_decision_candidate);
  boolean("local_search_last_target_decision_matched",
          base.local_search_last_target_decision_matched,
          value.local_search_last_target_decision_matched);
  const auto base_semantic =
      serialized_semantic_candidates(base.semantic_variable_candidates);
  const auto value_semantic =
      serialized_semantic_candidates(value.semantic_variable_candidates);
  if (base_semantic != value_semantic && !value.semantic_variable_candidates.empty())
    field("semantic_variable_candidates", value_semantic);
  out << '}';
  bool native_first = true;
#define TACTIX_FEEDBACK_SCALAR(name)                                      \
  if (!native_equal(base.cumulative.name, value.cumulative.name)) {       \
    if (native_first) out << ",\"native\":{"; else out << ',';         \
    native_first = false;                                                 \
    out << '"' << #name << "\":";                                     \
    native_value(out, value.cumulative.name);                             \
  }
#define TACTIX_FEEDBACK_ARRAY(name)
#include "strategy/feedback_statistics.inc"
#undef TACTIX_FEEDBACK_SCALAR
#undef TACTIX_FEEDBACK_ARRAY
  if (!native_first) out << '}';
  bool array_object_first = true;
#define TACTIX_FEEDBACK_SCALAR(name)
#define TACTIX_FEEDBACK_ARRAY(name)                                       \
  if (!native_equal(base.cumulative.name, value.cumulative.name)) {       \
    if (array_object_first) out << ",\"native_arrays\":{"; else out << ','; \
    array_object_first = false;                                           \
    out << '"' << #name << "\":[";                                     \
    size_t flat_index = 0;                                                \
    bool entry_first = true;                                              \
    native_array_delta(out, base.cumulative.name, value.cumulative.name,  \
                       flat_index, entry_first);                          \
    out << ']';                                                           \
  }
#include "strategy/feedback_statistics.inc"
#undef TACTIX_FEEDBACK_SCALAR
#undef TACTIX_FEEDBACK_ARRAY
  if (!array_object_first) out << '}';
  if ((!base.native_policy_state.empty() && value.native_policy_state.empty()) ||
      (!base.semantic_variable_candidates.empty() &&
       value.semantic_variable_candidates.empty())) {
    out << ",\"clear\":[";
    bool clear_first = true;
    if (!base.native_policy_state.empty() && value.native_policy_state.empty()) {
      out << "\"native_policy_state\"";
      clear_first = false;
    }
    if (!base.semantic_variable_candidates.empty() &&
        value.semantic_variable_candidates.empty()) {
      if (!clear_first) out << ',';
      out << "\"semantic_variable_candidates\"";
    }
    out << ']';
  }
  out << '}';
  return out.str();
}

std::string header(const std::string &metadata, uint32_t version = FEEDBACK_VERSION) {
  return "{\"type\":\"header\",\"version\":" + std::to_string(version) +
         ",\"metadata\":" + quote(metadata) + '}';
}

} // namespace

std::string serialize_observation(const Observation &value) {
  auto out = stream();
  out << "{\"version\":" << value.version << ",\"state\":";
  state(out, value.state);
  out << ",\"sequence\":" << value.sequence << ",\"variables\":" << value.variables
      << ",\"unassigned\":" << value.unassigned
      << ",\"active_unassigned\":" << value.active_unassigned
      << ",\"level\":" << value.level << ",\"trail\":" << value.trail_assignments
      << ",\"root_population\":" << value.root_population
      << ",\"irredundant_clauses\":" << value.irredundant_clauses
      << ",\"inconsistent\":" << value.inconsistent << ",\"conflict\":" << value.conflict
      << ",\"fixpoint\":" << value.propagation_fixpoint
      << ",\"searching\":" << value.searching
      << ",\"have_configuration\":" << value.have_search_configuration
      << ",\"configuration\":" << quote(name(value.search))
      << ",\"rotation_intervals\":" << value.rotation_intervals
      << ",\"next_restart_conflicts\":" << value.next_restart_conflicts
      << ",\"reluctant\":[" << value.reluctant_u << ',' << value.reluctant_v
      << ',' << value.reluctant_wait << "],\"fast_glue\":" << value.fast_glue
      << ",\"slow_glue\":" << value.slow_glue << ",\"last_action\":";
  if (value.last_action) {
    out << "{\"sequence\":" << value.last_action->sequence << ",\"type\":"
        << unsigned(value.last_action->type) << ",\"before\":";
    state(out, value.last_action->before);
    out << ",\"after\":"; state(out, value.last_action->after);
    out << ",\"propagation_ticks\":" << value.last_action->propagation_ticks
        << ",\"root_assignments\":" << value.last_action->root_assignments
        << ",\"outcome\":";
    outcome(out, feedback_outcome(*value.last_action));
    out << '}';
  } else out << "null";
  out << ",\"native\":{";
  bool first = true;
#define TACTIX_FEEDBACK_SCALAR(field) \
  if (!first) out << ','; \
  first = false; out << '"' << #field << "\":" << value.cumulative.field;
#define TACTIX_FEEDBACK_ARRAY(field) \
  if (!first) out << ','; \
  first = false; out << '"' << #field << "\":"; native_value(out, value.cumulative.field);
#include "strategy/feedback_statistics.inc"
#undef TACTIX_FEEDBACK_SCALAR
#undef TACTIX_FEEDBACK_ARRAY
  out << '}';
  if (!value.native_policy_state.empty())
    out << ",\"native_policy_state\":" << quote(value.native_policy_state);
  out << ",\"local_search_candidates\":[";
  bool first_candidate = true;
  for (const auto &candidate : value.local_search_candidates) {
    if (!first_candidate) out << ',';
    first_candidate = false;
    out << "{\"id\":" << candidate.id
        << ",\"formula_touch_epoch\":" << candidate.formula_touch_epoch
        << ",\"formula_root_assignments\":" << candidate.formula_root_assignments
        << ",\"external_variables\":" << candidate.external_variables
        << ",\"known_phases\":" << candidate.known_phases
        << ",\"objective\":" << candidate.objective
        << ",\"objective_exact\":" << candidate.objective_exact
        << ",\"effort\":" << candidate.effort.amount
        << ",\"effort_unit\":" << unsigned(candidate.effort.unit) << '}';
  }
  out << ']';
  out << ",\"local_search_last_target_decision_candidate\":"
      << value.local_search_last_target_decision_candidate
      << ",\"local_search_last_target_decision_matched\":"
      << value.local_search_last_target_decision_matched;
  if (!value.semantic_variable_candidates.empty()) {
    out << ",\"semantic_variable_candidates\":[";
    bool first_semantic_candidate = true;
    for (const auto &candidate : value.semantic_variable_candidates) {
      if (!first_semantic_candidate) out << ',';
      first_semantic_candidate = false;
      out << "{\"id\":" << candidate.id
          << ",\"action_id\":" << candidate.action_id
          << ",\"arena_generation\":" << candidate.arena_generation
          << ",\"touch_epoch\":" << candidate.touch_epoch
          << ",\"strategy_epoch\":" << candidate.strategy_epoch
          << ",\"producer\":" << unsigned(candidate.producer)
          << ",\"existing_variables\":" << candidate.existing_variables
          << ",\"introduced_variables\":" << candidate.introduced_variables
          << '}';
    }
    out << ']';
  }
  out << '}';
  return out.str();
}

std::string serialize_feedback(const ActionFeedback &value) {
  auto out = stream();
  out << "{\"type\":\"action\",\"version\":" << value.version
      << ",\"sequence\":" << value.sequence << ",\"kind\":" << unsigned(value.kind)
      << ",\"identity\":" << quote(value.identity) << ",\"instance\":" << value.instance
      << ",\"request\":";
  if (value.request) request(out, *value.request); else out << "null";
  out << ",\"allowance\":";
  if (value.allowance) allowance(out, *value.allowance); else out << "null";
  if (value.native_request) {
    out << ",\"native_request\":";
    native_request(out, *value.native_request);
  }
  out << ",\"before\":" << serialize_observation(value.before)
      << ",\"after\":" << serialize_observation(value.after) << ",\"outcome\":";
  outcome(out, value.outcome);
  out << ",\"stop\":" << unsigned(value.stop) << '}';
  return out.str();
}

std::string serialize_feedback(const SearchIntervalFeedback &value) {
  auto out = stream();
  out << "{\"type\":\"interval\",\"version\":" << value.version
      << ",\"sequence\":" << value.sequence << ",\"preceding\":" << value.preceding_interval
      << ",\"start\":" << unsigned(value.start) << ",\"stop\":" << unsigned(value.stop)
      << ",\"before\":" << serialize_observation(value.before)
      << ",\"after\":" << serialize_observation(value.after) << '}';
  return out.str();
}

std::string serialize_feedback_delta(const ActionFeedback &value,
                                     const Observation &base) {
  auto out = stream();
  out << "{\"type\":\"action\",\"version\":" << value.version
      << ",\"sequence\":" << value.sequence << ",\"kind\":" << unsigned(value.kind)
      << ",\"identity\":" << quote(value.identity) << ",\"instance\":" << value.instance
      << ",\"request\":";
  if (value.request) request(out, *value.request); else out << "null";
  out << ",\"allowance\":";
  if (value.allowance) allowance(out, *value.allowance); else out << "null";
  if (value.native_request) {
    out << ",\"native_request\":";
    native_request(out, *value.native_request);
  }
  out << ",\"before_delta\":" << serialize_observation_delta(base, value.before)
      << ",\"after_delta\":" << serialize_observation_delta(value.before, value.after)
      << ",\"outcome\":";
  outcome(out, value.outcome);
  out << ",\"stop\":" << unsigned(value.stop) << '}';
  return out.str();
}

std::string serialize_feedback_delta(const SearchIntervalFeedback &value,
                                     const Observation &base) {
  auto out = stream();
  out << "{\"type\":\"interval\",\"version\":" << value.version
      << ",\"sequence\":" << value.sequence << ",\"preceding\":"
      << value.preceding_interval << ",\"start\":" << unsigned(value.start)
      << ",\"stop\":" << unsigned(value.stop)
      << ",\"before_delta\":" << serialize_observation_delta(base, value.before)
      << ",\"after_delta\":" << serialize_observation_delta(value.before, value.after)
      << '}';
  return out.str();
}

bool FeedbackTrace::fail(const std::string &message) {
  if (error_.empty()) error_ = message;
  return false;
}
FeedbackTrace::~FeedbackTrace() { (void)close_stream(); }

bool FeedbackTrace::close_stream() {
  if (!compressed_) return true;
  const int status = gzclose(compressed_);
  compressed_ = nullptr;
  if (status != Z_OK) return fail("feedback compressed stream close failed");
  return true;
}

bool FeedbackTrace::read_line(std::string &line) {
  if (!compressed_) return bool(std::getline(input_, line));
  line.clear();
  char buffer[8192];
  while (true) {
    char *chunk = gzgets(compressed_, buffer, sizeof buffer);
    if (!chunk) {
      int status = Z_OK;
      (void)gzerror(compressed_, &status);
      if (status != Z_OK && status != Z_STREAM_END)
        fail("feedback compressed read failed");
      return !line.empty();
    }
    const size_t length = std::strlen(chunk);
    line.append(chunk, length);
    if (length && chunk[length - 1] == '\n') {
      line.pop_back();
      if (!line.empty() && line.back() == '\r') line.pop_back();
      return true;
    }
  }
}

bool FeedbackTrace::write_line(const std::string &line) {
  if (!compressed_) {
    output_ << line << '\n';
    return bool(output_);
  }
  if (line.size() > unsigned(INT_MAX))
    return fail("feedback compressed record too large");
  if (gzwrite(compressed_, line.data(), unsigned(line.size())) != int(line.size()) ||
      gzputc(compressed_, '\n') != '\n')
    return fail("feedback compressed write failed");
  return true;
}

bool FeedbackTrace::write(const char *path, const std::string &metadata) {
  if (opened_) return fail("feedback receiver already opened");
  opened_ = true;
  const std::string destination(path);
  compressed_output_ = destination.ends_with(".gz");
  if (compressed_output_) {
    compressed_ = gzopen(path, "wb6");
    if (!compressed_) return fail("cannot open compressed feedback records for writing");
    (void)gzbuffer(compressed_, 1u << 20);
  } else {
    output_.open(path);
    if (!output_) return fail("cannot open feedback records for writing");
  }
  event(header(metadata));
  return error_.empty();
}
bool FeedbackTrace::replay(const char *path, const std::string &metadata) {
  if (opened_) return fail("feedback receiver already opened");
  opened_ = true;
  replay_ = true;
  const std::string source(path);
  if (source.ends_with(".gz")) {
    compressed_ = gzopen(path, "rb");
    if (!compressed_) return fail("cannot open compressed feedback records for replay");
    (void)gzbuffer(compressed_, 1u << 20);
  } else {
    input_.open(path);
    if (!input_) return fail("cannot open feedback records for replay");
  }
  std::string first;
  if (!read_line(first)) return fail("truncated feedback record");
  ++line_;
  if (first == header(metadata, 1))
    wire_version_ = 1;
  else if (first == header(metadata, FEEDBACK_VERSION))
    wire_version_ = FEEDBACK_VERSION;
  else
    return fail("feedback divergence at line 1");
  return error_.empty();
}
void FeedbackTrace::event(const std::string &record) {
  if (!error_.empty()) return;
  if (!opened_) { fail("feedback receiver not opened"); return; }
  if (terminal_) { fail("feedback event after session end"); return; }
  ++line_;
  if (replay_) {
    std::string expected;
    if (!read_line(expected)) { fail("truncated feedback record"); return; }
    if (expected != record) fail("feedback divergence at line " + std::to_string(line_));
  } else {
    if (!write_line(record)) fail("feedback write failed");
  }
}
void FeedbackTrace::timing(uint64_t nanos) {
  if (!error_.empty()) return;
  static const std::string prefix = "{\"type\":\"elapsed_nanos\",\"value\":";
  ++line_;
  if (replay_) {
    std::string record;
    if (!read_line(record)) {
      fail("truncated feedback elapsed record"); return;
    }
    if (!record.starts_with(prefix) || record.back() != '}') {
      fail("invalid feedback elapsed record"); return;
    }
    uint64_t saved;
    const auto begin = record.data() + prefix.size();
    const auto end = record.data() + record.size() - 1;
    const auto parsed = std::from_chars(begin, end, saved);
    if (parsed.ec != std::errc{} || parsed.ptr != end) fail("invalid feedback elapsed value");
  } else {
    if (!write_line(prefix + std::to_string(nanos) + '}'))
      fail("feedback timing write failed");
  }
}
void FeedbackTrace::session_begin(const Observation &value) {
  if (begun_) { fail("duplicate feedback session begin"); return; }
  begun_ = true;
  event("{\"type\":\"session_begin\",\"observation\":" + serialize_observation(value) + '}');
  previous_observation_ = value;
}
void FeedbackTrace::after_action(const ActionFeedback &value) {
  if (!begun_ || value.version != FEEDBACK_VERSION ||
      value.sequence != ++action_sequence_) {
    fail("invalid feedback action lifecycle"); return;
  }
  if (!previous_observation_) { fail("feedback action without observation base"); return; }
  if (wire_version_ == 1) {
    ActionFeedback legacy = value;
    legacy.version = 1;
    event(serialize_feedback(legacy));
  } else {
    event(serialize_feedback_delta(value, *previous_observation_));
  }
  previous_observation_ = value.after;
  timing(value.duration_nanos);
}
void FeedbackTrace::search_interval(const SearchIntervalFeedback &value) {
  if (!begun_ || value.version != FEEDBACK_VERSION ||
      value.sequence != ++interval_sequence_ ||
      (value.start != IntervalStart::ResumeSearch && interval_sequence_ == 1) ||
      value.preceding_interval != (value.start != IntervalStart::ResumeSearch
                                      ? interval_sequence_ - 1 : 0)) {
    fail("invalid feedback interval lifecycle"); return;
  }
  if (!previous_observation_) { fail("feedback interval without observation base"); return; }
  if (wire_version_ == 1) {
    SearchIntervalFeedback legacy = value;
    legacy.version = 1;
    event(serialize_feedback(legacy));
  } else {
    event(serialize_feedback_delta(value, *previous_observation_));
  }
  previous_observation_ = value.after;
  timing(value.duration_nanos);
}
void FeedbackTrace::session_end(const Observation &value, int verdict, StopReason reason) {
  if (!begun_) { fail("feedback end without session begin"); return; }
  if (wire_version_ == 1)
    event("{\"type\":\"session_end\",\"verdict\":" + std::to_string(verdict) +
          ",\"stop\":" + std::to_string(unsigned(reason)) +
          ",\"observation\":" + serialize_observation(value) + '}');
  else
    event("{\"type\":\"session_end\",\"verdict\":" + std::to_string(verdict) +
          ",\"stop\":" + std::to_string(unsigned(reason)) +
          ",\"observation_delta\":" +
          serialize_observation_delta(*previous_observation_, value) + '}');
  if (!error_.empty()) return;
  terminal_ = true;
  if (replay_) {
    std::string extra;
    if (read_line(extra)) fail("feedback records after terminal session end");
    (void)close_stream();
  } else {
    if (compressed_) {
      (void)close_stream();
    } else {
      output_.flush();
      if (!output_) fail("feedback flush failed");
    }
  }
}

} // namespace tactix::strategy::controller
