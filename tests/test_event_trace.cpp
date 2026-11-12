#undef NDEBUG
#include "diagnostics/event_trace.hpp"
#include "policies/heuristics.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <string>
#include <utility>

#include <sys/wait.h>
#include <unistd.h>

using namespace tactix;

using Environment = std::initializer_list<std::pair<const char *, const char *>>;

// Artifacts are named after the writing process so concurrent ctest runs over
// several build directories cannot race on one temporary file.
static std::string trace_path(const char *stem) {
  return "/tmp/tactix-test-" + std::string(stem) + "-" +
         std::to_string(static_cast<long>(getpid())) + ".tsv";
}

// Sets each named variable; a null value leaves it unset.
static void set_environment(Environment environment) {
  for (const auto &[name, value] : environment)
    if (value)
      setenv(name, value, 1);
}

static void unset_environment(Environment environment) {
  for (const auto &[name, value] : environment)
    unsetenv(name);
}

// The contents of `path` (empty if absent), which is then removed.
static std::string take_file(const std::string &path) {
  std::string text;
  if (FILE *file = std::fopen(path.c_str(), "rb")) {
    char bytes[4096];
    while (const size_t count = std::fread(bytes, 1, sizeof bytes, file))
      text.append(bytes, count);
    std::fclose(file);
  }
  std::remove(path.c_str());
  return text;
}

static bool has(const std::string &text, const char *needle) {
  return text.find(needle) != std::string::npos;
}

// The text of a trace opened on `k` under `environment` while `body` runs.
template <class Body>
static std::string traced(const Kernel &k, const char *stem,
                          Environment environment, Body body) {
  const std::string path = trace_path(stem);
  std::remove(path.c_str());
  setenv("TACTIX_EVENT_TRACE", path.c_str(), 1);
  set_environment(environment);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    body(trace);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unset_environment(environment);
  return take_file(path);
}

// The wait status of a child process that runs `body` and then exits 99.
template <class Body> static int forked(Body body) {
  const pid_t child = fork();
  assert(child >= 0);
  if (!child) {
    body();
    std::_Exit(99);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  return status;
}

static bool exited_cleanly(int status) {
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main() {
  {
    Kernel implied;
    kernel_reserve_vars(implied, 2);
    ScopedHeuristics ih(implied);
    implied.warming = true;
    kernel_assign_decision(implied, internalize(1));
    const size_t begin = implied.trail.size();
    kernel_assign_binary(implied, internalize(2), internalize(-1));
    const std::string text = traced(
        implied, "imply-only",
        {{"TACTIX_EVENT_TRACE_IMPLY_ONLY", "1"},
         {"TACTIX_EVENT_TRACE_CONFLICT_FROM", "77"},
         {"TACTIX_EVENT_TRACE_CONFLICT_TO", "77"},
         {"SAT_EVENT_TRACE_IMPLY_LITERALS", "2"}},
        [&](EventTrace &trace) {
          for (uint64_t conflict : {76, 77, 78}) {
            implied.stats.conflicts = conflict;
            trace.implications(implied, begin);
            trace.detail(implied, "unrelated", 42);
          }
        });
    assert(has(text, "\timply\t77\t"));
    assert(has(text, "lit=2|level=1|reason=-1,2"));
    assert(!has(text, "\timply\t76\t") && !has(text, "\timply\t78\t"));
    assert(!has(text, "unrelated"));
  }

  const std::string macro_path = trace_path("macro-boundary");
  const std::string macro_state_path = trace_path("macro-state");
  const std::string macro_active_set_path = trace_path("macro-active-set");
  const std::string inprocess_score_path = trace_path("inprocess-score");
  for (const std::string *stale : {&macro_path, &macro_state_path,
                                   &macro_active_set_path,
                                   &inprocess_score_path})
    std::remove(stale->c_str());
  assert(exited_cleanly(forked([&] {
    Kernel child;
    kernel_reserve_vars(child, 2);
    ScopedHeuristics child_h(child);
    child.values[make_lit(1, false)] = 1;
    child.values[make_lit(1, true)] = -1;
    child.assigned[1].level = 0;
    child.stats.conflicts = 17;
    child.stats.decisions = 23;
    child.stats.propagations = 29;
    child.stats.restarts = 3;
    child.stats.reductions = 2;
    child.stats.switched = 1;
    child.stats.rephased = 1;
    set_environment({{"TACTIX_MACRO_BOUNDARY_TRACE", macro_path.c_str()},
                     {"SAT_MACRO_BOUNDARY_CONFLICT_FROM", "17"},
                     {"SAT_MACRO_BOUNDARY_EXIT_EVENT", "probe_begin"},
                     {"SAT_SEARCH_CHECKPOINT_INTERVAL", "4"},
                     {"SAT_SEARCH_CHECKPOINT_CONFLICT_FROM", "17"},
                     {"TACTIX_MACRO_BOUNDARY_STATE", macro_state_path.c_str()},
                     {"SAT_MACRO_BOUNDARY_STATE_EVENT", "probe_begin"},
                     {"TACTIX_MACRO_ACTIVE_SET", macro_active_set_path.c_str()},
                     {"SAT_MACRO_ACTIVE_SET_EVENT", "probe_begin"},
                     {"SAT_MACRO_ACTIVE_SET_CONFLICT", "17"},
                     {"TACTIX_INPROCESS_SCORE_TRACE",
                      inprocess_score_path.c_str()},
                     {"SAT_INPROCESS_SCORE_EXTERNAL", "1"},
                     {"SAT_INPROCESS_SCORE_CONFLICT", "17"}});
    EventTrace trace;
    trace.macro(child, "restart_begin");
    trace.macro(child, "restart_end");
    trace.checkpoint(child);
    child.stats.conflicts = 20;
    trace.checkpoint(child);
    child.stats.conflicts = 21;
    child.stats.decisions = 31;
    trace.checkpoint(child);
    child.stats.conflicts = 17;
    child.stats.decisions = 23;
    trace.macro(child, "probe_begin");
  })));
  const std::string macro_text = take_file(macro_path);
  assert(has(macro_text, "# tactix-macro-boundary\t1"));
  assert(has(macro_text, "\trestart_begin\t17\t23\t29\t"));
  assert(has(macro_text, "\trestart_end\t17\t23\t29\t"));
  assert(has(macro_text, "\tsearch_checkpoint\t17\t23\t29\t"));
  assert(has(macro_text, "\tsearch_checkpoint\t21\t31\t29\t"));
  assert(!has(macro_text, "\tsearch_checkpoint\t20\t"));
  assert(has(macro_text, "\tprobe_begin\t17\t23\t29\t"));
  const std::string macro_state_text = take_file(macro_state_path);
  assert(has(macro_state_text, "# tactix-macro-state\t1"));
  assert(has(macro_state_text,
             "tactix\tprobe_begin\t17\t23\t29\t1\t2\t0\t0\t0\t0"));
  const std::string macro_active_set_text = take_file(macro_active_set_path);
  assert(has(macro_active_set_text, "# tactix\tprobe_begin\t17\t1"));
  assert(has(macro_active_set_text, "# external\n1\n"));
  const std::string inprocess_score_text = take_file(inprocess_score_path);
  assert(has(inprocess_score_text, "# tactix-inprocess-score\t1"));
  assert(has(inprocess_score_text, "tactix\tprobe_begin\t17\t23\t1\t0\t-\t0\t1"));

  Kernel k;
  kernel_reserve_vars(k, 3);
  ScopedHeuristics h(k);

  const std::string disabled_path = trace_path("event-trace");
  std::remove(disabled_path.c_str());
  unsetenv("TACTIX_EVENT_TRACE");
  {
    EventTrace disabled;
    assert(!disabled.open_from_env(k));
    disabled.macro(k, "must_not_exist");
  }
  assert(!std::ifstream(disabled_path).good());

  const std::string text = traced(
      k, "event-trace",
      {{"TACTIX_EVENT_TRACE_FORMULA_MACRO", "1"},
       {"TACTIX_EVENT_TRACE_WATCH_POSE", "1"},
       {"TACTIX_EVENT_TRACE_WATCH_COST", "1"},
       {"TACTIX_EVENT_TRACE_WATCH_WITHIN_VIVIFY", "1"},
       {"TACTIX_EVENT_TRACE_WATCH_MOVE_CLAUSE_SET", "-3,-2,1"},
       {"TACTIX_EVENT_TRACE_VIVIFY_CANDIDATE_FROM", "7"},
       {"TACTIX_EVENT_TRACE_VIVIFY_CANDIDATE_TO", "7"},
       {"TACTIX_EVENT_TRACE_MODE_CLOCK", "1"},
       {"TACTIX_EVENT_TRACE_HEAP_REMOVE_DECISION", "0"},
       {"TACTIX_EVENT_TRACE_FACTOR_MARK", "-2"},
       {"SAT_EVENT_TRACE_VIVIFY_LITERALS", "-3,1"},
       {"SAT_EVENT_TRACE_VIVIFY_CLAUSES", "1"},
       {"SAT_EVENT_TRACE_SUBSTITUTE_ALL", "1"}},
      [&](EventTrace &trace) {
        trace.macro(k, "lucky_early_begin");
        trace.macro(k, "lucky_early_end", "unknown");
        trace.detail(k, "decision", 3);
        k.h->stable = true;
        trace.heap_insert(k, 1, 17, 4);
        trace.heap_remove(k, 1, 2, 17, 20, 4);
        k.h->stable = false;
        trace.slot_boundary(k, "Sweep", "begin");
        trace.sweep_seed(k, 1);
        trace.sweep_environment(k, 1, 3, 4, 0x1234);
        trace.sweep_effort(k, 2, 17, 31, 5);
        trace.sweep_equivalence(k, internalize(1), internalize(-2));
        trace.sweep_unit(k, internalize(-3));
        trace.factor_seed(k, internalize(-2), 17, 9);
        setenv("TACTIX_EVENT_TRACE_FACTOR_SEED", "1", 1);
        trace.factor_seed(k, internalize(-2), 18, 10);
        trace.factor_seed(k, internalize(1), 19, 11);
        unsetenv("TACTIX_EVENT_TRACE_FACTOR_SEED");
        trace.factor_grid(k, internalize(-2), 3, 4, 5);
        trace.factor_grid_member(k, 0, internalize(-2));
        const Lit factor_members[3] = {internalize(-2), internalize(1),
                                       internalize(3)};
        k.stats.factorings = 7;
        trace.factor_fresh(k, internalize(-2), 2, factor_members, 3, 4, 5, 0,
                           0, 0, 0, 0);
        // Call the recorder directly: this fixture links the trace-enabled
        // recorder to a trace-disabled library, whose inline hook differs.
        trace.factor_mark_transition(k, internalize(-2), true, "fixture");
        trace.substitute_map(k, internalize(2), internalize(2));
        trace.substitute_map(k, internalize(2), internalize(-3));
        trace.detail(k, "analysis_begin", 2);
        k.stats.search_ticks = 17;
        k.diagnostic_search_propagation_ticks = 11;
        trace.mode_clock(k, "pre_analyze", 23, 5, 3);
        kernel_assign_decision(k, internalize(1));
        const size_t trail_begin = k.trail.size();
        kernel_assign_binary(k, internalize(2), internalize(-1));
        trace.implications(k, trail_begin);
        const Lit conflict[3] = {internalize(-3), internalize(1),
                                 internalize(-2)};
        k.conflict_ref = kernel_add_large_clause(k, conflict, 3, false, 0);
        k.conflict_valid = true;
        clause_at(k.arena, k.conflict_ref)->searched = 3;
        trace.watch_move(k, internalize(3), internalize(2), internalize(1),
                         clause_at(k.arena, k.conflict_ref));
        const uint32_t vivify_counts[6] = {11, 12, 13, 14, 15, 16};
        trace.vivify_counts(k, vivify_counts, 6);
        set_environment({{"SAT_EVENT_TRACE_CLAUSE_SET", "-3,1,-2"},
                         {"SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_FROM", "0"},
                         {"SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_TO", "0"}});
        trace.vivify_candidate(k, "vivify_candidate", 6, k.conflict_ref, 10,
                               99);
        trace.watch_cost(k, internalize(1), 33, 31, 3, 1, 2, 1, 0, 7);
        trace.vivify_candidate(k, "vivify_candidate", 7, k.conflict_ref, 10,
                               99);
        trace.watch_cost(k, internalize(1), 33, 31, 3, 1, 2, 1, 0, 7);
        trace.watch_scan(k, internalize(3));
        trace.vivify_candidate(k, "vivify_candidate_end", 7, k.conflict_ref,
                               17, 99);
        trace.watch_cost(k, internalize(1), 1, 1, 2, 0, 0, 0, 0, 2);
        trace.conflict(k);
        trace.learned(k, {internalize(3), internalize(-1), internalize(2)});
        trace.clause_glue(k, "fixture", clause_at(k.arena, k.conflict_ref), 3,
                          2);
        trace.clause_used(k, clause_at(k.arena, k.conflict_ref), 0);
        trace.detail(k, "analysis_end", 1);
        k.tier1 = 3;
        k.tier2 = 8;
        RetierSchedule tiers;
        tiers.tier1_by_mode[0] = 3;
        tiers.tier2_by_mode[0] = 8;
        k.stats.used_glue[0][3] = 5;
        trace.retier(k, tiers, 0, k.stats.used_glue[0]);
        trace.reorder_begin(k);
        trace.reorder_end(k);
        trace.macro(k, "terminate", "10");
      });
  unset_environment({{"SAT_EVENT_TRACE_CLAUSE_SET", nullptr},
                     {"SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_FROM", nullptr},
                     {"SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_TO", nullptr}});
  for (const char *needle :
       {"# tactix-event-trace\t1", "\treorder_begin\t", "\treorder_end\t",
        "\tlucky_early_begin\t", "\tlucky_early_end\t", "\tdecision\t",
        "stage=pre_analyze|ticks=17|propagate_ticks=11|analysis_ticks=6|"
        "limit=23|anchor=5|count=3",
        "\twatch_move\t", "\theap_insert\t", "var=2|score=0.5|from=17|to=4",
        "\theap_remove\t",
        "removed=2:0.5|removed_pos=17|last=3:0.66666666666666674|from=20|to=4",
        "\tslot_boundary\t", "name=Sweep|stage=begin", "|formula_binary_hash=",
        "|root_fixed=", "|root_fixed_hash=", "\tsweep_seed\t", "external=2",
        "\tsweep_environment\t", "\tsweep_effort\t",
        "vars=3|clauses=4|hash=0000000000001234", "\tsweep_equivalence\t",
        "a=1|b=-2", "\tsweep_unit\t", "lit=-3", "\tfactor_seed\t",
        "external=-2|score=17|remainders=9",
        "external=1|score=19|remainders=11", "\tfactor_grid\t",
        "\tfactor_grid_member\t", "\tfactor_fresh\t",
        "fresh=3|ordinal=7|seed=-2|factors=3|remainders=4|delta=5|"
        "members=-2,1,3",
        "ordinal=0|external=-2",
        "external=-2|factors=3|remainders=4|delta=5",
        "\tfactor_mark_transition\t", "external=-2|marked=1|source=fixture",
        "\tsubstitute_map\t", "external=2|representative=-3",
        "\tanalysis_begin\t", "\timply\t", "\tlit=2|level=1|reason=-1,2\n",
        "\tconflict\t", "|clause=-3,-2,1|pose=-3,1,-2|priority=",
        "\tvivify_counts\t", "\tvivify_occurrence_clause\t",
        "literal=-3|hash=",
        "literals=-3:16@raw=5@import=5@compact=5,"
        "1:11@raw=0@import=0@compact=0",
        "ordinal=7|ticks=10|limit=99|size=3",
        "|clause_poses=id=0,ref=0,order=0,searched=3,redundant=0,glue=0,"
        "used=0,priority=0,pose=-3,1,-2",
        "|searched=3|pose=-3,1,-2\n", "\torder=-3,1,-2|set=-3,-2,1|",
        "|ref=0|id=0|glue=0|used=0|redundant=0", "\tlearned\t", "\tretier\t",
        "focused=3,8|stable=2,6|posed=3,8|used=0:0,1:0,2:0,3:5,",
        "\torder=3,-1,2|set=-1,2,3|levels=0,1,1|nonasserting_glue=1\n",
        "\treason_glue\t",
        "source=fixture|id=0|old=3|new=2|pose=-3,1,-2|levels=0,1,1|"
        "set=-3,-2,1",
        "mode=0|glue=0|pose=-3,1,-2|levels=0,1,1|set=-3,-2,1"})
    assert(has(text, needle));
  assert(!has(text, "external=-2|score=18|remainders=10"));
  assert(!has(text, "external=2|representative=2"));
  assert(!has(text, "ordinal=6|scan=1"));
  const size_t watch_cost_pos = text.find("\twatch_cost\t");
  assert(watch_cost_pos != std::string::npos);
  assert(text.find("ordinal=7|scan=1|words_before=33|words_after=31|base=3"
                   "|binary_assign=1|large_deref=2|relocate=1"
                   "|large_assign=0|total=7",
                   watch_cost_pos) != std::string::npos);
  const size_t analysis = text.find("\tanalysis_end\t");
  assert(analysis != std::string::npos);
  const std::string analysis_line =
      text.substr(analysis, text.find('\n', analysis) - analysis);
  assert(has(analysis_line, "\tfocused\t3\t1\t2\t3\t0\t") &&
         "analysis_end must carry the post-analysis ranking snapshot");
  assert(has(analysis_line, "\t3\t3\t1") &&
         "focused top32 must retain queue-tail decision order");
  assert(has(analysis_line,
             "|clause_set=-3,1,-2|clause_poses=id=0,ref=0,order=0,searched=3,"
             "redundant=0,glue=0,used=0,priority=0,pose=-3,1,-2"));

  const std::string factor_update_only = traced(
      k, "event-factor-update-only",
      {{"SAT_EVENT_TRACE_FACTOR_UPDATE_ONLY", "1"},
       {"SAT_EVENT_TRACE_FACTOR_UPDATE_ORDINAL", "7"}},
      [&](EventTrace &trace) {
        trace.macro(k, "restart_begin");
        trace.factor_seed(k, internalize(-2), 17, 9);
        trace.factor_update(k, internalize(-2), 3, internalize(1), 11);
        const Lit factor_members[2] = {internalize(-2), internalize(1)};
        k.stats.factorings = 8;
        trace.factor_fresh(k, internalize(-2), 2, factor_members, 2, 3, 1, 0,
                           0, 0, 0, 0);
      });
  assert(has(factor_update_only, "\tfactor_update\t"));
  assert(has(factor_update_only, "\tfactor_fresh\t"));
  assert(has(factor_update_only,
             "factor=7|grid=-2|ordinal=3|external=1|score=11"));
  assert(!has(factor_update_only, "\tfactor_seed\t"));
  assert(!has(factor_update_only, "\trestart_begin\t"));

  const std::string factor_literal_only = traced(
      k, "event-factor-literal-only",
      {{"SAT_EVENT_TRACE_FACTOR_LITERAL_ONLY", "1"},
       {"TACTIX_EVENT_TRACE_FACTOR_LITERAL", "-2"},
       {"TACTIX_EVENT_TRACE_FACTOR_MARK", "-2"},
       {"SAT_EVENT_TRACE_BINARY_ADD", "-2"}},
      [&](EventTrace &trace) {
        trace.macro(k, "restart_begin");
        trace.factor_seed(k, internalize(-2), 17, 9);
        trace.factor_literal_state(k, internalize(-2), true, false, 0, 2);
        trace.factor_literal_entry(k, internalize(-2), 0, true,
                                   internalize(1), 0, nullptr);
        trace.factor_mark_transition(k, internalize(-2), true, "test_source");
        trace.binary_add(k, internalize(-2), internalize(3));
        const Lit factor_members[2] = {internalize(-2), internalize(1)};
        trace.factor_fresh(k, internalize(-2), 2, factor_members, 2, 3, 1, 0,
                           0, 0, 0, 0);
      });
  for (const char *needle :
       {"\tfactor_literal_state\t", "\tfactor_literal_entry\t",
        "\tfactor_fresh\t", "\tfactor_mark_transition\t",
        "external=-2|marked=1|source=test_source", "\tbinary_add\t",
        "a=-2|b=3"})
    assert(has(factor_literal_only, needle));
  assert(!has(factor_literal_only, "\tfactor_seed\t"));
  assert(!has(factor_literal_only, "\trestart_begin\t"));

  const std::string clause_lifecycle = traced(
      k, "event-clause-lifecycle-only",
      {{"SAT_EVENT_TRACE_CLAUSE_LIFECYCLE_ONLY", "1"},
       {"TACTIX_EVENT_TRACE_FACTOR_LITERAL", "-2"},
       {"TACTIX_EVENT_TRACE_CONFLICT_FROM", "10"},
       {"TACTIX_EVENT_TRACE_CONFLICT_TO", "10"}},
      [&](EventTrace &trace) {
        trace.detail_text(k, "watch", "discarded");
        trace.macro(k, "restart_begin");
        trace.slot_boundary(k, "Outside", "begin");
        k.stats.conflicts = 10;
        trace.macro(k, "probe_begin");
        trace.slot_boundary(k, "Factor", "begin");
        trace.factor_literal_state(k, internalize(-2), true, false, 0, 2);
      });
  k.stats.conflicts = 0;
  assert(has(clause_lifecycle, "\tprobe_begin\t"));
  assert(has(clause_lifecycle, "\tslot_boundary\t"));
  assert(has(clause_lifecycle, "name=Factor|stage=begin"));
  assert(has(clause_lifecycle, "\tfactor_literal_state\t"));
  assert(!has(clause_lifecycle, "\twatch\t"));
  assert(!has(clause_lifecycle, "\trestart_begin\t"));
  assert(!has(clause_lifecycle, "name=Outside"));

  const std::string factor_fresh_only = traced(
      k, "event-factor-fresh-only",
      {{"SAT_EVENT_TRACE_FACTOR_FRESH_ONLY", "1"},
       {"SAT_EVENT_TRACE_FACTOR_METER", "1"},
       {"SAT_EVENT_TRACE_FACTOR_METER_COMPONENTS", "1"}},
      [&](EventTrace &trace) {
        trace.macro(k, "restart_begin");
        trace.factor_seed(k, internalize(-2), 17, 9);
        const Lit factor_members[2] = {internalize(-2), internalize(1)};
        trace.factor_fresh(k, internalize(-2), 2, factor_members, 2, 3, 1, 123,
                           7, 11, 13, 17);
      });
  assert(has(factor_fresh_only, "\tfactor_fresh\t"));
  assert(has(factor_fresh_only, "|ticks=123"));
  assert(has(factor_fresh_only,
             "|pops=7|first_ticks=11|next_ticks=13|match_ticks=17"));
  assert(!has(factor_fresh_only, "\tfactor_seed\t"));
  assert(!has(factor_fresh_only, "\trestart_begin\t"));

  const std::string factor_seed_only = traced(
      k, "event-factor-seed-only",
      {{"SAT_EVENT_TRACE_FACTOR_SEED_ONLY", "1"},
       {"TACTIX_EVENT_TRACE_FACTOR_SEED", "1"}},
      [&](EventTrace &trace) {
        trace.macro(k, "restart_begin");
        trace.factor_seed(k, internalize(-2), 17, 9);
        trace.factor_seed(k, internalize(1), 19, 11);
        const Lit factor_members[2] = {internalize(1), internalize(-2)};
        trace.factor_fresh(k, internalize(-2), 2, factor_members, 2, 3, 1, 0,
                           0, 0, 0, 0);
        trace.factor_fresh(k, internalize(1), 2, factor_members, 2, 3, 1, 0,
                           0, 0, 0, 0);
      });
  assert(has(factor_seed_only, "\ttrace_begin\t"));
  assert(has(factor_seed_only, "\tfactor_seed\t"));
  assert(has(factor_seed_only, "external=1|score=19|remainders=11"));
  assert(!has(factor_seed_only, "external=-2|score=17"));
  assert(has(factor_seed_only, "\tfactor_fresh\t"));
  assert(has(factor_seed_only, "seed=1|factors=2"));
  assert(!has(factor_seed_only, "seed=-2|factors=2"));
  assert(!has(factor_seed_only, "\trestart_begin\t"));

  k.h->stable = true;
  const std::string heap_vars = traced(
      k, "event-heap-vars-only",
      {{"TACTIX_EVENT_TRACE_HEAP_VARS_ONLY", "1"},
       {"SAT_EVENT_TRACE_HEAP_VARS", "2"}},
      [&](EventTrace &trace) {
        trace.detail(k, "decision", 99);
        trace.heap_insert(k, 0, 8, 4);
        trace.heap_insert(k, 1, 7, 3);
        trace.heap_remove(k, 1, 2, 3, 9, 5);
        trace.heap_pop(k, "begin", 1, 0, 7, 0);
        trace.substitute_map(k, internalize(1), internalize(1));
        trace.substitute_map(k, internalize(2), internalize(-1));
        Kernel root;
        kernel_reserve_vars(root, 2);
        ScopedHeuristics root_h(root);
        kernel_assign_unit(root, internalize(1));
        kernel_assign_unit(root, internalize(2));
      });
  for (const char *needle :
       {"\theap_insert\t", "var=2|", "\theap_remove\t", "removed=2:",
        "\theap_pop\t", "stage=begin|held=2:", "\troot_assign\t",
        "lit=2|warming=0|trail_after=", "|reason=unit", "\tsubstitute_map\t",
        "external=2|representative=-1|"})
    assert(has(heap_vars, needle));
  for (const char *needle : {"\tdecision\t", "var=1|", "lit=1|warming=",
                             "external=1|representative=1|"})
    assert(!has(heap_vars, needle));

  const uint64_t saved_conflicts = k.stats.conflicts;
  const std::string selected_conflict = std::to_string(saved_conflicts + 1);
  const std::string heap_window = traced(
      k, "event-heap-vars-window",
      {{"TACTIX_EVENT_TRACE_HEAP_VARS_ONLY", "1"},
       {"SAT_EVENT_TRACE_HEAP_VARS", "2"},
       {"TACTIX_EVENT_TRACE_CONFLICT_FROM", selected_conflict.c_str()},
       {"TACTIX_EVENT_TRACE_CONFLICT_TO", selected_conflict.c_str()}},
      [&](EventTrace &trace) {
        trace.heap_insert(k, 1, 7, 3);
        k.stats.conflicts++;
        trace.heap_insert(k, 1, 7, 3);
      });
  k.stats.conflicts = saved_conflicts;
  k.h->stable = false;
  const size_t selected_insert = heap_window.find("\theap_insert\t");
  assert(selected_insert != std::string::npos);
  assert(heap_window.find("\theap_insert\t", selected_insert + 1) ==
         std::string::npos);

  const std::string glue_only = traced(
      k, "event-glue-only", {{"TACTIX_EVENT_TRACE_GLUE_ONLY", "1"}},
      [&](EventTrace &trace) {
        trace.macro(k, "must_be_filtered");
        trace.clause_glue(k, "fixture", clause_at(k.arena, k.conflict_ref), 3,
                          2);
        trace.detail(k, "analysis_end", 1);
      });
  assert(has(glue_only, "\treason_glue\t"));
  assert(has(glue_only, "\tanalysis_end\t"));
  assert(!has(glue_only, "must_be_filtered"));
  assert(!has(glue_only, "\ttrace_begin\t"));

  const std::string heap_dump_path = trace_path("event-heap-dump");
  std::remove(heap_dump_path.c_str());
  k.h->scores.stack = {0};
  k.h->scores.pos[0] = 0;
  k.h->scores.score[0] = 3.25;
  const std::string formula_only = traced(
      k, "event-formula-only",
      {{"TACTIX_EVENT_TRACE_FORMULA_ONLY", "1"},
       {"TACTIX_EVENT_TRACE_FORMULA_THRESHOLDS", "1,10"},
       {"TACTIX_EVENT_TRACE_HEAP_DUMP", heap_dump_path.c_str()}},
      [&](EventTrace &trace) {
        trace.detail(k, "decision", 1);
        trace.macro(k, "must_be_filtered");
        k.stats.conflicts = 2; // first analysis end after threshold 1
        trace.detail(k, "analysis_end", 1);
        k.stats.conflicts = 12; // conflict counters may skip threshold 10
        trace.detail(k, "analysis_end", 1);
        trace.macro(k, "terminate", "0");
      });
  k.stats.conflicts = saved_conflicts;
  assert(has(formula_only, "\ttrace_begin\t"));
  const size_t first_formula = formula_only.find("\tformula_checkpoint\t");
  assert(first_formula != std::string::npos);
  assert(formula_only.find("\tformula_checkpoint\t", first_formula + 1) !=
         std::string::npos);
  assert(has(formula_only, "\tterminate\t"));
  assert(!has(formula_only, "\tdecision\t"));
  assert(!has(formula_only, "must_be_filtered"));
  const std::string heap_dump = take_file(heap_dump_path);
  assert(has(heap_dump, "tactix\t2\t0\t1\t0\t0\t0\t3.25\n"));
  assert(has(heap_dump, "tactix\t12\t0\t1\t0\t0\t0\t3.25\n"));

  const std::string conflict_only = traced(
      k, "event-conflict-only", {{"TACTIX_EVENT_TRACE_CONFLICT_ONLY", "1"}},
      [&](EventTrace &trace) {
        trace.detail(k, "decision", 1);
        trace.conflict(k);
        trace.macro(k, "must_be_filtered");
        trace.macro(k, "terminate", "0");
      });
  assert(has(conflict_only, "\ttrace_begin\t"));
  assert(has(conflict_only, "\tconflict\t"));
  assert(has(conflict_only, "\tterminate\t"));
  assert(!has(conflict_only, "\tdecision\t"));
  assert(!has(conflict_only, "must_be_filtered"));

  const std::string learned_only = traced(
      k, "event-learned-only",
      {{"TACTIX_EVENT_TRACE_LEARNED_ONLY", "1"},
       {"TACTIX_EVENT_TRACE_TRAJECTORY_ONLY", "1"},
       {"TACTIX_EVENT_TRACE_CONFLICT_FROM", "2"},
       {"TACTIX_EVENT_TRACE_CONFLICT_TO", "2"}},
      [&](EventTrace &trace) {
        k.stats.conflicts = 1;
        trace.learned(k, {internalize(1), internalize(-2)});
        k.stats.conflicts = 2;
        trace.detail(k, "decision", 1);
        trace.conflict(k);
        trace.macro(k, "must_be_filtered");
        trace.learned_stage(k, "learned_raw",
                            {internalize(-1), internalize(3)});
        trace.learned(k, {internalize(1), internalize(-2)});
        k.stats.conflicts = 3;
        trace.learned(k, {internalize(2), internalize(-3)});
        trace.macro(k, "terminate", "0");
      });
  k.stats.conflicts = saved_conflicts;
  assert(has(learned_only, "\ttrace_begin\t"));
  assert(has(learned_only, "\tlearned\t2\t"));
  assert(has(learned_only, "size=2|order_hash="));
  assert(has(learned_only, "\tterminate\t3\t"));
  for (const char *needle :
       {"\tlearned\t1\t", "\tlearned\t3\t", "\tdecision\t", "\tconflict\t",
        "\tlearned_raw\t", "must_be_filtered"})
    assert(!has(learned_only, needle));

  // Trajectory-only traces keep every analysis boundary past the post-Reorder
  // detail budget.
  const std::string trajectory_reorder = traced(
      k, "event-trajectory-reorder",
      {{"TACTIX_EVENT_TRACE_TRAJECTORY_ONLY", "1"}}, [&](EventTrace &trace) {
        trace.reorder_begin(k);
        trace.reorder_end(k);
        for (unsigned i = 0; i != 1024; ++i)
          trace.detail(k, "analysis_mark", i);
        trace.detail(k, "analysis_begin", 2);
        trace.detail(k, "analysis_end", 1);
      });
  assert(has(trajectory_reorder, "\tanalysis_begin\t"));
  assert(has(trajectory_reorder, "\tanalysis_end\t"));
  assert(!has(trajectory_reorder, "\tanalysis_mark\t"));

  const std::string bve_pop_only = traced(
      k, "event-bve-pop-only", {{"TACTIX_EVENT_TRACE_BVE_POP_ONLY", "1"}},
      [&](EventTrace &trace) {
        const Var members[2] = {0, 1};
        const double scores[3] = {3.0, 2.0, 1.0};
        trace.bve_schedule(k, members, scores, 2, 0, 17);
        trace.bve_candidate(k, 0, "begin", 1, 2, false);
        trace.bve_plan(k, 0, 1, 1, 0, 0, 0, 1, false);
        trace.bve_pop(k, 4096, 23, 0, 3.0, 1, 1);
        trace.detail(k, "decision", 1);
      });
  assert(has(bve_pop_only, "\tbve_schedule\t"));
  assert(has(bve_pop_only, "\tbve_pop_checkpoint\t"));
  assert(!has(bve_pop_only, "\tbve_candidate\t"));
  assert(!has(bve_pop_only, "\tbve_plan\t"));
  assert(!has(bve_pop_only, "\tdecision\t"));

  // Compact inprocessing traces keep BVE and its phase cleanup.
  const std::string compact = traced(
      k, "event-compact-inprocess", {{"TACTIX_EVENT_TRACE_COMPACT_PROBE", "1"}},
      [&](EventTrace &trace) {
        trace.bve_candidate(k, 0, "begin", 11, 19, false);
        trace.bve_plan(k, 0, 2, 3, 0, 0, 0, 6, false);
        trace.eliminate_cleanup(k, "before");
        trace.detail(k, "decision", 1);
      });
  assert(has(compact, "\tbve_candidate\t"));
  assert(has(compact, "pivot_value="));
  assert(has(compact, "\tbve_plan\t"));
  assert(has(compact, "\teliminate_cleanup\t"));
  assert(!has(compact, "\tdecision\t"));

  const std::string boundary = traced(
      k, "event-boundary-only", {{"TACTIX_EVENT_TRACE_BOUNDARY_ONLY", "1"}},
      [&](EventTrace &trace) {
        trace.slot_boundary(k, "Vivify", "begin");
        trace.factor_seed(k, 0, 3, 2);
      });
  assert(has(boundary, "\tslot_boundary\t"));
  assert(has(boundary, "name=Vivify|stage=begin"));
  assert(!has(boundary, "\tfactor_seed\t"));

  // Stable top32 breaks score ties by Kissat's first-import index order.
  const std::string heap_set_path = trace_path("event-heap-set");
  std::remove(heap_set_path.c_str());
  Kernel import_tie_kernel;
  kernel_reserve_vars(import_tie_kernel, 3);
  kernel_record_import(import_tie_kernel, 1);
  kernel_record_import(import_tie_kernel, 0);
  kernel_record_import(import_tie_kernel, 2);
  ScopedHeuristics import_tie_h(import_tie_kernel);
  for (Var v = 0; v < import_tie_kernel.nvars; ++v)
    heuristics_score_set(import_tie_kernel, v, 0.0);
  import_tie_kernel.h->stable = true;
  heuristics_update_scores(import_tie_kernel);
  const std::string import_tie = traced(
      import_tie_kernel, "event-import-tie",
      {{"SAT_EVENT_TRACE_HEAP_SET_PATH", heap_set_path.c_str()},
       {"SAT_EVENT_TRACE_HEAP_SET_CONFLICT", "0"},
       {"SAT_EVENT_TRACE_HEAP_SET_DECISION", "0"}},
      [&](EventTrace &trace) {
        trace.heap_scores(import_tie_kernel);
        trace.detail(import_tie_kernel, "analysis_end", 0);
      });
  const size_t import_tie_analysis = import_tie.find("\tanalysis_end\t");
  assert(import_tie_analysis != std::string::npos);
  assert(has(import_tie.substr(import_tie_analysis,
                               import_tie.find('\n', import_tie_analysis) -
                                   import_tie_analysis),
             "\t2,1,3\t0") &&
         "stable score ties must follow Kissat's first-import index order");
  const std::string heap_set = take_file(heap_set_path);
  assert(has(heap_set, "# tactix-heap-set\t1\n"));
  assert(has(heap_set, "2\t0\t0\t0\t1\n"));
  assert(has(heap_set, "1\t1\t0\t0\t1\n"));
  assert(has(heap_set, "3\t2\t0\t0\t1\n"));

  Kernel watch_kernel;
  kernel_reserve_vars(watch_kernel, 3);
  ScopedHeuristics watch_h(watch_kernel);
  const std::string watch_positions = traced(
      watch_kernel, "event-watch-positions",
      {{"SAT_EVENT_TRACE_WATCH_LIST", "-1"},
       {"SAT_EVENT_TRACE_WATCH_TARGETS", "2,3"},
       {"SAT_EVENT_TRACE_BINARY_ADD", "-1"},
       {"SAT_EVENT_TRACE_SWEEP_REWRITE", "-1"}},
      [&](EventTrace &trace) {
        trace.binary_add(watch_kernel, internalize(-1), internalize(2));
        kernel_add_binary_clause(watch_kernel, internalize(-1), internalize(2));
        trace.binary_add(watch_kernel, internalize(-1), internalize(-3));
        kernel_add_binary_clause(watch_kernel, internalize(-1),
                                 internalize(-3));
        trace.sweep_rewrite_binary(watch_kernel, internalize(-2),
                                   internalize(-1), 7, internalize(3));
        trace.macro(watch_kernel, "watch_positions");
      });
  for (const char *needle :
       {"|watch_lit=-1|binary_count=2|binary_hash=",
        "|target_a=2|target_a_pos=1|target_a_lit=2|target_b=3|target_b_pos=2"
        "|target_b_lit=-3",
        "\tbinary_add\t", "\ta=-1|b=2\n", "\ta=-1|b=-3\n",
        "\tsweep_rewrite_binary\t", "\tsource=-2|repr=-1|position=7|other=3\n"})
    assert(has(watch_positions, needle));
  assert(!has(watch_positions, "|binary="));

  const std::string binary_set_path = trace_path("event-binary-set");
  std::remove(binary_set_path.c_str());
  traced(watch_kernel, "event-binary-set-events",
         {{"SAT_EVENT_TRACE_BINARY_SET_PATH", binary_set_path.c_str()},
          {"SAT_EVENT_TRACE_BINARY_SET_MATCH", "selected-snapshot"},
          {"SAT_EVENT_TRACE_BINARY_SET_CONFLICT", "0"}},
         [&](EventTrace &trace) {
           trace.macro(watch_kernel, "other-snapshot");
           trace.macro(watch_kernel, "selected-snapshot");
         });
  const std::string binary_set = take_file(binary_set_path);
  assert(has(binary_set, "# begin\t0\ttactix\tselected-snapshot\t0\t2\t-"));
  assert(has(binary_set, "0\t-3\t-1\n"));
  assert(has(binary_set, "0\t-1\t2\n"));
  assert(!has(binary_set, "other-snapshot"));

  // The standalone BVE schedule observer needs no global trace; it emits one
  // row and exits before the first candidate.
  const std::string bve_schedule_path = trace_path("standalone-bve-schedule");
  std::remove(bve_schedule_path.c_str());
  assert(exited_cleanly(forked([&] {
    unsetenv("TACTIX_EVENT_TRACE");
    set_environment({{"TACTIX_BVE_SCHEDULE_TRACE", bve_schedule_path.c_str()},
                     {"SAT_BVE_SCHEDULE_CONFLICT", "1"},
                     {"SAT_BVE_SCHEDULE_ORDINAL", "1"},
                     {"SAT_BVE_SCHEDULE_MEMBERS", "1"},
                     {"SAT_BVE_SCHEDULE_SELECTED", "1,2"}});
    std::vector<uint32_t> occurrence_counts(2 * watch_kernel.nvars, 0);
    occurrence_counts[make_lit(0, false)] = 7;
    occurrence_counts[make_lit(0, true)] = 11;
    occurrence_counts[make_lit(2, false)] = 13;
    occurrence_counts[make_lit(2, true)] = 17;
    watch_kernel.bve_occurrence_count_image = &occurrence_counts;
    const Var members[2] = {0, 2};
    const double scores[3] = {-3.0, 0.0, -5.0};
    // A non-matching schedule neither writes nor terminates; the observer
    // selects the requested later conflict and ordinal exactly.
    event_trace_bve_schedule(watch_kernel, members, scores, 2, 0,
                             0x123456789abcdef0ull);
    watch_kernel.stats.conflicts = 1;
    event_trace_bve_schedule(watch_kernel, members, scores, 2, 0,
                             0x123456789abcdef0ull);
    event_trace_bve_schedule(watch_kernel, members, scores, 2, 0,
                             0x123456789abcdef0ull);
  })));
  const std::string bve_schedule = take_file(bve_schedule_path);
  for (const char *needle :
       {"# bve-schedule-observer\t1\n", "tactix\tbound=0|scheduled=2|",
        "|conflicts=1|decisions=0|propagations=", "|active=3|vars=3|trail=",
        "|insertion_hash=123456789abcdef0|",
        "member\texternal=1\tposition=0\tpositive=7\tnegative=11\t",
        "member\texternal=3\tposition=1\tpositive=13\tnegative=17\t",
        "selected\texternal=1\tposition=0\tpositive=7\tnegative=11\t"
        "value=0\tlevel=0\tretired=0\n"})
    assert(has(bve_schedule, needle));

  const std::string bve_candidate_path = trace_path("standalone-bve-candidate");
  std::remove(bve_candidate_path.c_str());
  assert(exited_cleanly(forked([&] {
    unsetenv("TACTIX_EVENT_TRACE");
    set_environment({{"TACTIX_BVE_CANDIDATE_TRACE", bve_candidate_path.c_str()},
                     {"SAT_BVE_CANDIDATE_CONFLICT", "1"},
                     {"SAT_BVE_CANDIDATE_EXTERNAL", "1"},
                     {"SAT_BVE_CANDIDATE_EXIT_AFTER_END", "1"}});
    watch_kernel.stats.conflicts = 1;
    watch_kernel.bve_retry.resize(watch_kernel.nvars, 0);
    watch_kernel.bve_retry[0] = 1;
    event_trace_bve_candidate(watch_kernel, 0, "begin", 7, 11, false);
    watch_kernel.bve_retry[0] = 0;
    event_trace_bve_candidate(watch_kernel, 0, "end", 9, 11, false);
  })));
  const std::string bve_candidate = take_file(bve_candidate_path);
  assert(has(bve_candidate, "# bve-candidate-observer\t1\n"));
  assert(has(bve_candidate,
             "external=1\tstage=begin\tconflicts=1\tticks=7\tlimit=11\t"
             "applied=0\tretry=1\tvalue=0\tretired=0\troots=0\ttrail=0\t"
             "propagate=0\n"));
  assert(has(bve_candidate,
             "external=1\tstage=end\tconflicts=1\tticks=9\tlimit=11\t"
             "applied=0\tretry=0\tvalue=0\tretired=0\troots=0\ttrail=0\t"
             "propagate=0\n"));

  const std::string bve_retry_path = trace_path("standalone-bve-retry");
  std::remove(bve_retry_path.c_str());
  assert(exited_cleanly(forked([&] {
    unsetenv("TACTIX_EVENT_TRACE");
    set_environment({{"TACTIX_BVE_RETRY_TRACE", bve_retry_path.c_str()},
                     {"SAT_BVE_RETRY_CONFLICT", "1"},
                     {"SAT_BVE_RETRY_EXTERNAL", "1"},
                     {"SAT_BVE_RETRY_EXIT_AFTER_MARK", "1"}});
    watch_kernel.stats.conflicts = 1;
    watch_kernel.bve_retry.resize(watch_kernel.nvars, 0);
    watch_kernel.bve_retry[0] = 0;
    event_trace_bve_candidate(watch_kernel, 0, "end", 9, 11, false);
    // Call the producer directly, as for factor_mark_transition above.
    watch_kernel.bve_retry[0] = 1;
    watch_kernel.stats.bve_marks++;
    event_trace_bve_retry_mark(watch_kernel, 0, "unit-test-source", false,
                               true, nullptr);
  })));
  const std::string bve_retry = take_file(bve_retry_path);
  assert(has(bve_retry, "# bve-retry-observer\t1\n"));
  assert(has(bve_retry,
             "external=1\tconflicts=1\tsource=unit-test-source\t"
             "trigger=1\tbefore=0\tafter=1\tvalue=0\tretired=0\t"
             "clause_id=0\tclause_size=0\tclause_hash=0000000000000000\t"
             "clause_lits=-\n"));

  // A zero cap plus target-only mode proves the selected late boundary
  // bypasses both ordinary event work and the cap at its public entry point.
  const std::string matched_exit_path = trace_path("event-matched-exit");
  std::remove(matched_exit_path.c_str());
  assert(exited_cleanly(forked([&] {
    set_environment({{"TACTIX_EVENT_TRACE_LATE_PATH", matched_exit_path.c_str()},
                     {"TACTIX_EVENT_TRACE_LIMIT", "0"},
                     {"SAT_EVENT_TRACE_TARGET_ONLY", "1"},
                     {"SAT_EVENT_TRACE_EXIT_AFTER_MATCH",
                      "slot_boundary|name=selected|stage=begin"},
                     {"SAT_EVENT_TRACE_EXIT_CONFLICT", "0"}});
    event_trace_slot_boundary(watch_kernel, "other", "begin");
    event_trace_slot_boundary(watch_kernel, "selected", "begin");
  })));
  const std::string matched_exit = take_file(matched_exit_path);
  assert(!has(matched_exit, "\tother\t"));
  assert(has(matched_exit, "\tslot_boundary\t"));
  assert(has(matched_exit, "name=selected|stage=begin"));

  const std::string macro_exit_path = trace_path("event-macro-matched-exit");
  std::remove(macro_exit_path.c_str());
  assert(exited_cleanly(forked([&] {
    unsetenv("TACTIX_EVENT_TRACE");
    set_environment({{"TACTIX_EVENT_TRACE_LATE_PATH", macro_exit_path.c_str()},
                     {"TACTIX_EVENT_TRACE_LIMIT", "0"},
                     {"SAT_EVENT_TRACE_TARGET_ONLY", "1"},
                     {"SAT_EVENT_TRACE_EXIT_AFTER_MATCH", "eliminate_end"},
                     {"SAT_EVENT_TRACE_EXIT_CONFLICT", "0"}});
    EventTrace late_macro;
    late_macro.macro(watch_kernel, "probe_end");
    late_macro.macro(watch_kernel, "eliminate_end");
  })));
  const std::string macro_exit = take_file(macro_exit_path);
  assert(!has(macro_exit, "\tprobe_end\t"));
  assert(has(macro_exit, "\teliminate_end\t"));

  const std::string factor_fresh_exit_path =
      trace_path("factor-fresh-matched-exit");
  std::remove(factor_fresh_exit_path.c_str());
  assert(exited_cleanly(forked([&] {
    unsetenv("TACTIX_EVENT_TRACE_LATE_PATH");
    unsetenv("SAT_EVENT_TRACE_TARGET_ONLY");
    set_environment({{"TACTIX_EVENT_TRACE", factor_fresh_exit_path.c_str()},
                     {"SAT_EVENT_TRACE_FACTOR_FRESH_ONLY", "1"},
                     {"TACTIX_EVENT_TRACE_LIMIT", "1"},
                     {"SAT_EVENT_TRACE_EXIT_AFTER_MATCH",
                      "slot_boundary|name=Factor|stage=end"},
                     {"SAT_EVENT_TRACE_EXIT_CONFLICT", "0"}});
    EventTrace trace;
    assert(trace.open_from_env(watch_kernel));
    trace.slot_boundary(watch_kernel, "Factor", "end");
  })));
  const std::string factor_fresh_exit = take_file(factor_fresh_exit_path);
  assert(has(factor_fresh_exit, "\tslot_boundary\t"));
  assert(has(factor_fresh_exit, "name=Factor|stage=end"));

  const std::string randec_exit_path = trace_path("event-randec-late-exit");
  std::remove(randec_exit_path.c_str());
  assert(exited_cleanly(forked([&] {
    set_environment({{"TACTIX_EVENT_TRACE_LATE_PATH", randec_exit_path.c_str()},
                     {"TACTIX_EVENT_TRACE_LIMIT", "0"},
                     {"SAT_EVENT_TRACE_TARGET_ONLY", "1"},
                     {"SAT_EVENT_TRACE_EXIT_AFTER_MATCH", "randec_draw"},
                     {"SAT_EVENT_TRACE_EXIT_CONFLICT", "0"},
                     {"SAT_EVENT_TRACE_EXIT_DECISION", "42"}});
    watch_kernel.stats.decisions = 41;
    event_trace_randec_draw(watch_kernel, 17, 0, 0, true);
    watch_kernel.stats.decisions = 42;
    event_trace_randec_draw(watch_kernel, 19, 1, 1, true);
  })));
  const std::string randec_exit = take_file(randec_exit_path);
  assert(has(randec_exit, "\trandec_draw\t0\t42\t"));
  assert(has(randec_exit, "raw=19|slot=1|external=2|accepted=1"));
  assert(!has(randec_exit, "raw=17"));

  const std::string focused_stop_path = trace_path("event-focused-stop");
  std::remove(focused_stop_path.c_str());
  const int stop_status = forked([&] {
    Kernel stop_kernel;
    kernel_reserve_vars(stop_kernel, 1);
    ScopedHeuristics stop_h(stop_kernel);
    stop_kernel.h->stable = false;
    stop_kernel.stats.conflicts = 1;
    set_environment({{"TACTIX_EVENT_TRACE", focused_stop_path.c_str()},
                     {"TACTIX_EVENT_TRACE_STOP_AFTER_CONFLICT", "1"}});
    EventTrace trace;
    assert(trace.open_from_env(stop_kernel));
    trace.detail(stop_kernel, "analysis_end", 0);
  });
  assert(WIFSIGNALED(stop_status) && WTERMSIG(stop_status) == SIGTERM);
  std::remove(focused_stop_path.c_str());

  const std::string keep_snapshots = traced(
      k, "event-keep-snapshots-before-stop",
      {{"TACTIX_EVENT_TRACE_STOP_AFTER_CONFLICT", "2"},
       {"TACTIX_EVENT_TRACE_KEEP_SNAPSHOTS", "1"}},
      [&](EventTrace &trace) {
        k.stats.conflicts = 1;
        trace.macro(k, "restart_begin");
      });
  assert(has(keep_snapshots, "\trestart_begin\t1\t"));

  const std::string window = traced(
      k, "event-window",
      {{"TACTIX_EVENT_TRACE_CONFLICT_FROM", "2"},
       {"TACTIX_EVENT_TRACE_CONFLICT_TO", "2"}},
      [&](EventTrace &trace) {
        k.stats.conflicts = 1;
        trace.detail(k, "decision", 11);
        k.stats.conflicts = 2;
        trace.detail(k, "decision", 22);
        k.stats.conflicts = 3;
        trace.detail(k, "decision", 33);
        trace.macro(k, "terminate", "20");
      });
  assert(has(window, "# detail-conflicts\t2\t2"));
  assert(has(window, "\tdecision\t2\t"));
  assert(!has(window, "\tdecision\t1\t") && !has(window, "\tdecision\t3\t"));

  // WATCH_ONLY keeps macro snapshots; its strict form rejects them too.
  for (const bool strict : {false, true}) {
    const std::string watch_only = traced(
        k, strict ? "event-watch-only-strict" : "event-watch-only",
        {{"TACTIX_EVENT_TRACE_WATCH_ONLY", "1"},
         {"TACTIX_EVENT_TRACE_WATCH_ONLY_STRICT", strict ? "1" : nullptr},
         {"TACTIX_EVENT_TRACE_WATCH_CLAUSE_SET", "2,3;-3,-2,1"}},
        [&](EventTrace &trace) {
          trace.detail(k, "decision", 44);
          trace.detail(k, "analysis_begin", 2);
          trace.watch_scan(k, internalize(3));
          trace.macro(k, "restart_begin");
        });
    assert(has(watch_only, "\twatch\t"));
    assert(has(watch_only, "\trestart_begin\t") == !strict);
    assert(has(watch_only, "\ttrace_begin\t") == !strict);
    assert(!has(watch_only, "\tdecision\t"));
    assert(!has(watch_only, "\tanalysis_begin\t"));
  }

  const std::string garbage_formula_path = trace_path("event-formula-garbage");
  std::remove(garbage_formula_path.c_str());
  Kernel garbage_kernel;
  kernel_reserve_vars(garbage_kernel, 3);
  const Lit garbage_lits[3] = {internalize(1), internalize(-2), internalize(3)};
  const Ref garbage_ref =
      kernel_add_large_clause(garbage_kernel, garbage_lits, 3, false, 0);
  kernel_delete_clause(garbage_kernel,
                       clause_at(garbage_kernel.arena, garbage_ref));
  traced(garbage_kernel, "event-formula-garbage-trace",
         {{"SAT_EVENT_TRACE_FORMULA_SET_PATH", garbage_formula_path.c_str()},
          {"SAT_EVENT_TRACE_FORMULA_SET_MATCH", "fixture"},
          {"SAT_EVENT_TRACE_FORMULA_SET_CONFLICT", "0"},
          {"SAT_EVENT_TRACE_FORMULA_SET_INCLUDE_GARBAGE", "1"},
          {"SAT_EVENT_TRACE_FORMULA_SET_META", "1"}},
         [&](EventTrace &trace) { trace.macro(garbage_kernel, "fixture"); });
  assert(has(take_file(garbage_formula_path),
             "-2,1,3\tglue=0\tpriority=0\tsearched=2\tused=0"
             "\treason=0\tshrunken=0\tgarbage=1"));

  const std::string skip_ticks = traced(
      k, "event-skip-analysis-ticks",
      {{"TACTIX_EVENT_TRACE_SKIP_ANALYSIS_TICKS", "1"}},
      [&](EventTrace &trace) {
        trace.detail(k, "analysis_tick_reason", 1);
        trace.detail(k, "analysis_tick_shrink", 1);
        trace.detail(k, "decision", 7);
      });
  assert(!has(skip_ticks, "\tanalysis_tick"));
  assert(has(skip_ticks, "\tdecision\t"));

  const std::string skip_randec = traced(
      k, "event-skip-randec", {{"TACTIX_EVENT_TRACE_SKIP_RANDEC_DRAWS", "1"}},
      [&](EventTrace &trace) {
        trace.randec_draw(k, 17, 2, 0, false);
        trace.detail(k, "decision", 1);
      });
  assert(!has(skip_randec, "\trandec_draw\t"));
  assert(has(skip_randec, "\tdecision\t"));

  const std::string compact_bumps = traced(
      k, "event-compact-bumps",
      {{"TACTIX_EVENT_TRACE_SEARCH_ONLY", "1"},
       {"TACTIX_EVENT_TRACE_SKIP_HEAP", "1"},
       {"SAT_EVENT_TRACE_SWEEP_CLAUSES", "1"},
       {"SAT_EVENT_TRACE_SWEEP_QUERIES", "1"},
       {"SAT_EVENT_TRACE_SWEEP_COMMIT", "1,-2"}},
      [&](EventTrace &trace) {
        trace.detail(k, "score_bump", 2);
        trace.detail(k, "reason_bump_limit", 10);
        trace.detail(k, "heap_reinsert", 1);
        trace.randec_state(k, "pre", 523, 533, 4, 0x1234);
        trace.sweep_seed(k, 0);
        trace.sweep_environment(k, 0, 3, 4, 5);
        const Lit sweep_clause[2] = {internalize(1), internalize(-2)};
        trace.sweep_environment_clause(k, 0, 0, sweep_clause, 2);
        trace.sweep_query(k, 0, 0, "initial", INVALID_LIT, INVALID_LIT, 0, 7,
                          0);
        const Var sweep_vars[2] = {0, 1};
        const int8_t sweep_values[2] = {1, -1};
        trace.sweep_model(k, 0, 0, sweep_vars, sweep_values, 2);
        trace.sweep_equivalence(k, internalize(1), internalize(-2));
        trace.sweep_commit(k, internalize(-2), internalize(1), -1, 0, "entry");
        trace.detail(k, "diagnostic_noise", 9);
      });
  for (const char *needle :
       {"\tscore_bump\t", "\treason_bump_limit\t", "\theap_reinsert\t",
        "\trandec_state\t", "\tsweep_seed\t", "\tsweep_environment\t",
        "\tsweep_environment_clause\t", "\tsweep_query\t",
        "\tsweep_query_model\t", "\tsweep_equivalence\t", "\tsweep_commit\t",
        "a=-2|b=1|av=-1|bv=0|proof=0|stage=entry"})
    assert(has(compact_bumps, needle));
  assert(!has(compact_bumps, "\tdiagnostic_noise\t"));

  const std::string trajectory = traced(
      k, "event-trajectory-only", {{"TACTIX_EVENT_TRACE_TRAJECTORY_ONLY", "1"}},
      [&](EventTrace &trace) {
        trace.detail(k, "decision", 3);
        trace.detail(k, "analysis_begin", 2);
        trace.learned_stage(k, "learned_shrunk",
                            {internalize(3), internalize(-1), internalize(2)});
        trace.detail(k, "analysis_tail_probe", 7);
        trace.detail(k, "analysis_end", 1);
        k.h->stable = true;
        trace.heap_scores(k);
        k.h->stable = false;
        trace.detail(k, "score_bump", 2);
        trace.detail(k, "analysis_mark", 1);
        trace.macro(k, "restart_begin");
      });
  for (const char *needle :
       {"\tdecision\t", "\tanalysis_begin\t", "\tlearned_shrunk\t",
        "size=3|order_hash=", "|set_hash=", "\tanalysis_end\t",
        "\trestart_begin\t"})
    assert(has(trajectory, needle));
  for (const char *needle : {"\tanalysis_tail_probe\t", "\theap_scores\t",
                             "\tscore_bump\t", "\tanalysis_mark\t"})
    assert(!has(trajectory, needle));

  const std::string sweep_only = traced(
      k, "event-sweep-only",
      {{"TACTIX_EVENT_TRACE_TRAJECTORY_ONLY", "1"},
       {"TACTIX_EVENT_TRACE_SWEEP_ONLY", "1"},
       {"SAT_EVENT_TRACE_SWEEP_CLAUSES", "2"},
       {"SAT_EVENT_TRACE_SWEEP_QUERIES", "2"},
       {"SAT_EVENT_TRACE_SWEEP_VARS", "2"},
       {"SAT_EVENT_TRACE_SWEEP_BINARIES", "2"}},
      [&](EventTrace &trace) {
        trace.detail(k, "analysis_begin", 2);
        trace.sweep_seed(k, 1);
        trace.sweep_environment_var(k, 1, 0, 2, 1);
        trace.sweep_binary_attempt(k, 1, 0, internalize(1), internalize(-2), 1,
                                   2, "add");
        const Lit clause[2] = {internalize(1), internalize(-2)};
        trace.sweep_environment_clause(k, 1, 0, clause, 2);
        trace.sweep_query(k, 1, 0, "initial", INVALID_LIT, INVALID_LIT, 0, 7,
                          0);
        trace.sweep_effort(k, 2, 17, 31, 5);
        trace.macro(k, "sweep_end");
      });
  assert(!has(sweep_only, "\tanalysis_begin\t"));
  for (const char *needle :
       {"\tsweep_seed\t", "\tsweep_environment_var\t",
        "\tsweep_binary_attempt\t", "lit_depth=1|other_depth=2|disposition=add",
        "\tsweep_environment_clause\t", "\tsweep_query\t",
        "external=2|before=17|after=31|delta=14|solves=5", "\tsweep_end\t"})
    assert(has(sweep_only, needle));

  const std::string search_watch_cost = traced(
      k, "event-search-watch-cost-only",
      {{"TACTIX_EVENT_TRACE_WATCH_COST", "1"},
       {"TACTIX_EVENT_TRACE_SEARCH_WATCH_COST_ONLY", "1"}},
      [&](EventTrace &trace) {
        k.inprocessing = true;
        trace.watch_scan(k, internalize(1));
        trace.watch_cost(k, internalize(1), 7, 7, 2, 1, 2, 0, 0, 5);
        k.inprocessing = false;
        trace.watch_scan(k, internalize(1));
        trace.watch_cost(k, internalize(1), 7, 7, 2, 1, 2, 0, 0, 5);
      });
  const size_t first_watch_cost = search_watch_cost.find("\twatch_cost\t");
  assert(first_watch_cost != std::string::npos);
  assert(search_watch_cost.find("\twatch_cost\t", first_watch_cost + 1) ==
         std::string::npos);
  assert(!has(search_watch_cost, "\twatch\t"));

  // The reduction journal records the selected conflict window only.
  const std::string reduce_path = trace_path("reduce-trace");
  std::remove(reduce_path.c_str());
  set_environment({{"TACTIX_REDUCE_TRACE", reduce_path.c_str()},
                   {"TACTIX_REDUCE_TRACE_CONFLICT_FROM", "999"},
                   {"TACTIX_REDUCE_TRACE_CONFLICT_TO", "1000"}});
  {
    Kernel reduced;
    kernel_reserve_vars(reduced, 4);
    ScopedHeuristics rh(reduced);
    const Lit a[3] = {internalize(1), internalize(-2), internalize(3)};
    const Lit b[3] = {internalize(-1), internalize(2), internalize(4)};
    const Ref ra = kernel_add_large_clause(reduced, a, 3, true, 7);
    kernel_add_large_clause(reduced, b, 3, true, 2);
    reduced.tier1 = 2;
    reduced.tier2 = 6;
    EventTrace trace;
    for (const uint64_t conflicts : {998, 999, 1001}) {
      reduced.stats.conflicts = conflicts;
      trace.reduction(reduced, {ra}, 1);
    }
  }
  unset_environment({{"TACTIX_REDUCE_TRACE", nullptr},
                     {"TACTIX_REDUCE_TRACE_CONFLICT_FROM", nullptr},
                     {"TACTIX_REDUCE_TRACE_CONFLICT_TO", nullptr}});
  const std::string reduce = take_file(reduce_path);
  assert(has(reduce, "# tactix-kissat-reduce\t1"));
  assert(has(reduce, "tactix\tsummary\t1\t999\tfocused\t2\t6\t2\t1\t1"));
  assert(has(reduce, "\tcandidate\t0\t1\t-2,1,3\n"));
  assert(!has(reduce, "\t998\tfocused\t") && !has(reduce, "\t1001\tfocused\t"));
  std::puts("test_event_trace OK");
  return 0;
}
