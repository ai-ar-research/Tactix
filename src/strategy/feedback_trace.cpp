#include "strategy/feedback_trace.hpp"

#include <charconv>
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
  out << quote(serialize_online_strategy_program(program)) << '}';
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

std::string header(const std::string &metadata) {
  return "{\"type\":\"header\",\"version\":" + std::to_string(FEEDBACK_VERSION) +
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
        << ",\"objective\":" << candidate.objective
        << ",\"objective_exact\":" << candidate.objective_exact
        << ",\"effort\":" << candidate.effort.amount
        << ",\"effort_unit\":" << unsigned(candidate.effort.unit) << '}';
  }
  out << ']';
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

bool FeedbackTrace::fail(const std::string &message) {
  if (error_.empty()) error_ = message;
  return false;
}
bool FeedbackTrace::write(const char *path, const std::string &metadata) {
  if (opened_) return fail("feedback receiver already opened");
  opened_ = true;
  output_.open(path);
  if (!output_) return fail("cannot open feedback records for writing");
  event(header(metadata));
  return error_.empty();
}
bool FeedbackTrace::replay(const char *path, const std::string &metadata) {
  if (opened_) return fail("feedback receiver already opened");
  opened_ = true;
  replay_ = true;
  input_.open(path);
  if (!input_) return fail("cannot open feedback records for replay");
  event(header(metadata));
  return error_.empty();
}
void FeedbackTrace::event(const std::string &record) {
  if (!error_.empty()) return;
  if (!opened_) { fail("feedback receiver not opened"); return; }
  if (terminal_) { fail("feedback event after session end"); return; }
  ++line_;
  if (replay_) {
    std::string expected;
    if (!std::getline(input_, expected)) { fail("truncated feedback record"); return; }
    if (expected != record) fail("feedback divergence at line " + std::to_string(line_));
  } else {
    output_ << record << '\n';
    if (!output_) fail("feedback write failed");
  }
}
void FeedbackTrace::timing(uint64_t nanos) {
  if (!error_.empty()) return;
  static const std::string prefix = "{\"type\":\"elapsed_nanos\",\"value\":";
  ++line_;
  if (replay_) {
    std::string record;
    if (!std::getline(input_, record)) {
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
    output_ << prefix << nanos << "}\n";
    if (!output_) fail("feedback timing write failed");
  }
}
void FeedbackTrace::session_begin(const Observation &value) {
  if (begun_) { fail("duplicate feedback session begin"); return; }
  begun_ = true;
  event("{\"type\":\"session_begin\",\"observation\":" + serialize_observation(value) + '}');
}
void FeedbackTrace::after_action(const ActionFeedback &value) {
  if (!begun_ || value.version != FEEDBACK_VERSION ||
      value.sequence != ++action_sequence_) {
    fail("invalid feedback action lifecycle"); return;
  }
  event(serialize_feedback(value));
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
  event(serialize_feedback(value));
  timing(value.duration_nanos);
}
void FeedbackTrace::session_end(const Observation &value, int verdict, StopReason reason) {
  if (!begun_) { fail("feedback end without session begin"); return; }
  event("{\"type\":\"session_end\",\"verdict\":" + std::to_string(verdict) +
        ",\"stop\":" + std::to_string(unsigned(reason)) + ",\"observation\":" +
        serialize_observation(value) + '}');
  if (!error_.empty()) return;
  terminal_ = true;
  if (replay_) {
    std::string extra;
    if (std::getline(input_, extra)) fail("feedback records after terminal session end");
  } else {
    output_.flush();
    if (!output_) fail("feedback flush failed");
  }
}

} // namespace tactix::strategy::controller
