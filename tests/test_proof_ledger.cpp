// The proof ledger (N2): with no proof and with every proof format, the Proof
// object gives the search the same answers. Seeded random event scripts run in
// lockstep on a proof that writes nothing (open_nowhere), streaming PBP,
// --dsr, the automatic proof, and an automatic proof forced to switch to PBP
// (TACTIX_AUTO_PROOF_SWITCH_AT). After every event, every return value and
// every answer the search reads must agree, including after the empty clause
// and after a startup refutation. Fixed cases cover the aborts and proof
// errors every mode shares, a startup refutation after an input refutation,
// the files a run without a proof opens (none), the parity rows a switch writes
// again, and the staged root units an external event settles before a switch.
//
// Usage: test_proof_ledger [scripts [first seed]]; script i uses seed
// first + i and prints it first, so `test_proof_ledger 1 <seed>` replays the
// last seed printed before a failure.
#undef NDEBUG
#include "proof/proof.hpp"

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace tactix;

namespace {

constexpr int32_t VARIABLES = 16;
using Clause = std::vector<int32_t>; // DIMACS literals

const std::vector<Var> identity = [] {
  std::vector<Var> map(VARIABLES);
  for (Var v = 0; v < VARIABLES; ++v)
    map[v] = v;
  return map;
}();

std::vector<Lit> internal(const Clause &clause) {
  std::vector<Lit> lits;
  for (const int32_t literal : clause)
    lits.push_back(internalize(literal));
  return lits;
}

std::string read_all(const std::string &path) {
  FILE *f = std::fopen(path.c_str(), "rb");
  assert(f);
  std::string out;
  char buffer[4096];
  size_t n;
  while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0)
    out.append(buffer, n);
  std::fclose(f);
  return out;
}

// A fresh directory; removed with its entries at scope end.
struct TemporaryDirectory {
  std::string path;
  TemporaryDirectory() {
    const char *base = std::getenv("TMPDIR");
    path = std::string(base && *base ? base : "/tmp") + "/tactix-ledger-XXXXXX";
    assert(mkdtemp(path.data()));
  }
  std::string operator/(const char *name) const { return path + "/" + name; }
  ~TemporaryDirectory() {
    DIR *directory = opendir(path.c_str());
    assert(directory);
    std::vector<std::string> names;
    while (const dirent *entry = readdir(directory))
      if (std::strcmp(entry->d_name, ".") && std::strcmp(entry->d_name, ".."))
        names.push_back(entry->d_name);
    closedir(directory);
    for (const auto &name : names)
      std::remove((path + "/" + name).c_str());
    rmdir(path.c_str());
  }
};

struct Random {
  uint64_t state;
  uint64_t next() { // splitmix64
    uint64_t z = state += 0x9e3779b97f4a7c15ull;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
  }
  uint32_t below(uint32_t n) { return static_cast<uint32_t>(next() % n); }
  bool percent(uint32_t p) { return below(100) < p; }
  int32_t literal(int32_t variable) { return percent(50) ? variable : -variable; }
};

enum class Mode : uint8_t { None, Pbp, Dsr, Automatic, Switched };

const char *mode_name(Mode mode) {
  switch (mode) {
  case Mode::None: return "no proof";
  case Mode::Pbp: return "--pbp";
  case Mode::Dsr: return "--dsr";
  case Mode::Automatic: return "automatic";
  case Mode::Switched: return "automatic with a forced switch";
  }
  return "?";
}

const char *file_name(Mode mode) {
  switch (mode) {
  case Mode::None: return "none";
  case Mode::Pbp: return "pbp";
  case Mode::Dsr: return "dsr";
  case Mode::Automatic: return "automatic";
  case Mode::Switched: return "switched";
  }
  return "?";
}

constexpr Mode every_mode[] = {Mode::None, Mode::Pbp, Mode::Dsr, Mode::Automatic,
                               Mode::Switched};

// One proof under test and the state its caller keeps for it.
struct Subject {
  Mode mode;
  Proof proof;
  PbpParityBoundsCache cache;
  std::vector<uint64_t> charges; // every charge callback's argument, in order
  PbpParityReasonPlan plan;      // the last valid plan, for a later resubmission
  explicit Subject(Mode m) : mode(m) {}
};

// `switch_at` is TACTIX_AUTO_PROOF_SWITCH_AT, read when the proof opens.
void open_subject(Subject &s, const std::string &path, bool binary,
                  const char *switch_at = nullptr) {
  bool opened = false;
  switch (s.mode) {
  case Mode::None:
    opened = s.proof.open_nowhere();
    break;
  case Mode::Pbp:
    opened = s.proof.open(path.c_str(), ProofFormat::PbpStreaming);
    break;
  case Mode::Dsr:
    opened = s.proof.open(path.c_str(), binary ? ProofFormat::DsrBinary
                                               : ProofFormat::DsrAscii);
    break;
  case Mode::Automatic:
    opened = s.proof.open_automatic(path.c_str(), binary);
    break;
  case Mode::Switched:
    assert(switch_at && !setenv("TACTIX_AUTO_PROOF_SWITCH_AT", switch_at, 1));
    opened = s.proof.open_automatic(path.c_str(), binary);
    assert(!unsetenv("TACTIX_AUTO_PROOF_SWITCH_AT"));
    break;
  }
  assert(opened && s.proof.stream_identity());
  s.proof.bind_variable_map(&identity);
}

// The answers of one event, in order, each with its name.
struct Answers {
  std::vector<std::pair<const char *, uint64_t>> values;
  void operator()(const char *what, uint64_t value) {
    values.emplace_back(what, value);
  }
  uint64_t operator[](size_t at) const { return values.at(at).second; }
};

using Handles = std::vector<std::pair<uint64_t, Proof::PremiseGroup>>;

// Every answer the search reads between events.
void observe(Subject &s, const Handles &handles, Answers &a) {
  const Proof &p = s.proof;
  a("startup_pb_available", p.startup_pb_available());
  a("startup_pb_written", p.startup_pb_written());
  a("premise_epoch", p.premise_epoch());
  a("premise_literals", p.premise_literals());
  a("premise_bytes", p.premise_bytes());
  a("premise_deletions_deferred", p.premise_deletions_deferred());
  a("premise_pending_deletions", p.premise_pending_deletions());
  a("premise_pending_literal_work", p.premise_pending_literal_work());
  a("premise_maintenance_work", p.premise_maintenance_work());
  a("persistent_external_epoch", p.persistent_external_epoch());
  a("persistent_external_bytes", p.persistent_external_bytes());
  a("cache generation", s.cache.generation());
  a("cache size", s.cache.size());
  a("cache retained bytes", s.cache.retained_bytes());
  for (const auto &[handle, group] : handles) {
    a("premise_group_active", p.premise_group_active(handle));
    a("premise_group_matches", p.premise_group_matches(handle, group));
  }
  for (const uint64_t cost : s.charges)
    a("charge", cost);
  s.charges.clear();
}

using Body = std::function<void(Subject &, Answers &)>;

// Runs each event on every subject and checks that all of them answer alike.
struct Lockstep {
  std::vector<std::unique_ptr<Subject>> subjects;
  Handles handles; // every handle a retain returned, with its group
  std::vector<std::string> log;
  uint64_t seed = 0;

  Subject &add(Mode mode) {
    subjects.push_back(std::make_unique<Subject>(mode));
    return *subjects.back();
  }
  Proof &reference() { return subjects.front()->proof; }

  // The agreed answers, followed by the agreed observation.
  Answers run(const char *name, const Body &body) {
    log.push_back(name);
    Answers first;
    for (auto &subject : subjects) {
      Answers answers;
      body(*subject, answers);
      observe(*subject, handles, answers);
      if (subject == subjects.front())
        first = std::move(answers);
      else
        compare(first, answers, subject->mode);
    }
    return first;
  }

  void compare(const Answers &want, const Answers &got, Mode mode) const {
    for (size_t i = 0; i < std::max(want.values.size(), got.values.size()); ++i) {
      const bool both = i < want.values.size() && i < got.values.size();
      if (both && !std::strcmp(want.values[i].first, got.values[i].first) &&
          want.values[i].second == got.values[i].second)
        continue;
      std::fprintf(stderr, "FAIL seed %" PRIu64 " event %zu (%s): ", seed,
                   log.size(), log.back().c_str());
      if (both)
        std::fprintf(stderr, "%s says %s=%" PRIu64 ", %s says %s=%" PRIu64 "\n",
                     mode_name(mode), got.values[i].first, got.values[i].second,
                     mode_name(subjects.front()->mode), want.values[i].first,
                     want.values[i].second);
      else
        std::fprintf(stderr, "%s gives %zu answers, %s %zu\n", mode_name(mode),
                     got.values.size(), mode_name(subjects.front()->mode),
                     want.values.size());
      std::fprintf(stderr, "events:");
      for (size_t at = log.size() > 16 ? log.size() - 16 : 0; at < log.size(); ++at)
        std::fprintf(stderr, " %s", log[at].c_str());
      std::fprintf(stderr, "\n");
      std::abort();
    }
  }
};

// A charge callback that records each cost and refuses past `budget`.
std::function<bool(uint64_t)> metered(Subject &s, uint64_t budget) {
  return [&s, budget, spent = uint64_t{0}](uint64_t cost) mutable {
    s.charges.push_back(cost);
    if (cost > budget - std::min(spent, budget))
      return false;
    spent += cost;
    return true;
  };
}

bool write_pb(FILE *out) {
  return std::fputs("pseudo-Boolean proof version 3.0\nf 0 ;\n", out) >= 0;
}

PbpParityEquation flipped(PbpParityEquation equation) {
  equation.rhs = !equation.rhs;
  return equation;
}

// One random script: the caller's view of the clause database and of every
// handle, and the events a kernel may send in each phase.
struct Script {
  Lockstep &l;
  Random &r;
  struct Live {
    uint64_t id;
    Clause lits;
    bool external;
  };
  std::vector<Live> live;     // clauses a later event may delete or lease
  std::vector<Live> retained; // registered by retain_external
  std::vector<PbpParityEquation> pool;
  Clause startup_input;
  uint64_t next_id = 1;
  uint32_t extent = VARIABLES; // solver and parity-helper variables
  bool finished = false, terminal = false;
  // The query of every subject's stored plan, and its helper count.
  bool planned = false;
  std::vector<PbpParityEquation> planned_equations;
  Clause planned_reason;
  uint32_t planned_helpers = 0;

  // The pool holds under one hidden assignment, so every sum of its rows is
  // consistent: an inconsistent system refutes, and gets the empty reason.
  Script(Lockstep &lockstep, Random &random) : l(lockstep), r(random) {
    const uint64_t assignment = r.next();
    for (int i = 0; i < 5; ++i) {
      PbpParityEquation equation;
      const uint32_t width = 2 + r.below(3);
      while (equation.variables.size() < width) {
        const int32_t variable = 1 + static_cast<int32_t>(r.below(10));
        if (std::find(equation.variables.begin(), equation.variables.end(),
                      variable) == equation.variables.end())
          equation.variables.push_back(variable);
      }
      std::sort(equation.variables.begin(), equation.variables.end());
      for (const int32_t variable : equation.variables)
        equation.rhs ^= (assignment >> variable) & 1;
      pool.push_back(equation);
    }
  }

  // A registry flush (a witnessed or RAT line) deletes every retained clause.
  void track_registry(uint64_t epoch_before) {
    if (l.reference().persistent_external_epoch() != epoch_before)
      retained.clear();
  }

  std::vector<int32_t> leased_variables() {
    std::vector<int32_t> variables;
    for (const auto &[handle, group] : l.handles)
      if (l.reference().premise_group_active(handle))
        for (const auto &clause : group)
          for (const int32_t literal : clause)
            variables.push_back(static_cast<int32_t>(external_variable_of(literal)));
    return variables;
  }
  bool leasing() {
    for (const auto &entry : l.handles)
      if (l.reference().premise_group_active(entry.first))
        return true;
    return false;
  }
  int32_t pivot_variable() {
    const auto leased = leased_variables();
    if (!leased.empty() && r.percent(50))
      return leased[r.below(static_cast<uint32_t>(leased.size()))];
    return 1 + static_cast<int32_t>(r.below(VARIABLES));
  }
  int32_t fresh_variable(const std::vector<int32_t> &used) {
    for (;;) {
      const int32_t variable = 1 + static_cast<int32_t>(r.below(VARIABLES));
      if (std::find(used.begin(), used.end(), variable) == used.end())
        return variable;
    }
  }
  // Distinct variables, `pivot` first when given.
  Clause clause(uint32_t smallest, uint32_t largest, int32_t pivot = 0) {
    const uint32_t size = smallest + r.below(largest - smallest + 1);
    std::vector<int32_t> variables;
    if (pivot)
      variables.push_back(pivot);
    while (variables.size() < size)
      variables.push_back(fresh_variable(variables));
    Clause lits;
    for (const int32_t variable : variables)
      lits.push_back(r.literal(variable));
    return lits;
  }
  // A repeated or complementary literal.
  void spoil(Clause &lits) {
    lits.push_back(r.percent(50) ? lits.front() : -lits.front());
    std::swap(lits.back(), lits[r.below(static_cast<uint32_t>(lits.size()))]);
  }
  // A live clause, from leased bodies about half the time while leases exist.
  int pick(bool external) {
    std::vector<int> candidates, leased;
    const auto keys = [&] {
      std::vector<Clause> bodies;
      for (const auto &[handle, group] : l.handles)
        if (l.reference().premise_group_active(handle))
          for (Clause body : group) {
            std::sort(body.begin(), body.end());
            bodies.push_back(body);
          }
      return bodies;
    }();
    for (int i = 0; i < static_cast<int>(live.size()); ++i) {
      if (live[i].external != external)
        continue;
      candidates.push_back(i);
      Clause body = live[i].lits;
      std::sort(body.begin(), body.end());
      if (std::find(keys.begin(), keys.end(), body) != keys.end())
        leased.push_back(i);
    }
    const auto &from = !leased.empty() && r.percent(50) ? leased : candidates;
    return from.empty() ? -1 : from[r.below(static_cast<uint32_t>(from.size()))];
  }
  Live take(int at) {
    Live clause = std::move(live[at]);
    live.erase(live.begin() + at);
    return clause;
  }

  // --- Input ---
  void seed() {
    const Clause c = clause(1, 3);
    const auto lits = internal(c);
    const uint64_t id = next_id++;
    l.run("seed_population", [&](Subject &s, Answers &) {
      s.proof.seed_population(id, lits.data(), static_cast<uint32_t>(lits.size()));
    });
    live.push_back({id, c, false});
    startup_input.insert(startup_input.end(), c.begin(), c.end());
    startup_input.push_back(0);
  }
  void finish_input() {
    l.run("finish_input_transaction",
          [](Subject &s, Answers &) { s.proof.finish_input_transaction(); });
  }

  // --- Clausal events ---
  void rup() {
    const Clause c = clause(1, 4);
    const auto lits = internal(c);
    const uint64_t id = r.percent(5) ? 0 : next_id++;
    l.run("add_rup", [&](Subject &s, Answers &) {
      s.proof.add_rup(id, lits.data(), static_cast<uint32_t>(lits.size()));
    });
    live.push_back({id, c, false});
  }
  void derived_unit() {
    const int32_t unit = r.literal(1 + static_cast<int32_t>(r.below(VARIABLES)));
    const uint64_t id = next_id++;
    l.run("add_derived_unit", [&](Subject &s, Answers &) {
      s.proof.add_derived_unit(id, internalize(unit));
    });
    if (!terminal)
      live.push_back({id, {unit}, false});
  }
  // The empty clause, as an empty RUP line or an empty external one.
  void empty() {
    const uint64_t id = next_id++;
    const uint32_t kind = r.below(10);
    l.run("add_empty", [&](Subject &s, Answers &answers) {
      if (kind < 7)
        s.proof.add_empty(id);
      else if (kind < 8)
        s.proof.add_rup(id, nullptr, 0);
      else
        answers("accepted", s.proof.add_external_rup(id, nullptr, 0));
    });
    finished = true;
  }
  // Root units of a propagation, then (sometimes) its root conflict.
  void root_propagation() {
    l.run("begin_deferred_root_units",
          [](Subject &s, Answers &) { s.proof.begin_deferred_root_units(); });
    for (uint32_t units = finished ? 0 : 1 + r.below(3); units; --units)
      derived_unit();
    l.run("end_deferred_root_units",
          [](Subject &s, Answers &) { s.proof.end_deferred_root_units(); });
    if (!finished && !terminal && r.percent(40))
      empty();
  }
  void rat() {
    const Clause c = clause(1, 3, pivot_variable());
    const auto lits = internal(c);
    const uint64_t id = next_id++;
    const uint64_t epoch = l.reference().persistent_external_epoch();
    l.run("add_rat", [&](Subject &s, Answers &) {
      s.proof.add_rat(id, lits.data(), static_cast<uint32_t>(lits.size()));
    });
    track_registry(epoch);
    live.push_back({id, c, false});
  }
  // Direct or witnessed SR; its witness often names leased variables.
  void sr() {
    const int32_t pivot = pivot_variable();
    const Clause c = clause(1, 3, pivot);
    const auto lits = internal(c);
    std::vector<int32_t> used{pivot};
    std::vector<Lit> true_lits;
    std::vector<DsrLiteralMapping> mappings;
    const auto any_literal = [&] {
      return internalize(r.literal(1 + static_cast<int32_t>(r.below(VARIABLES))));
    };
    if (r.percent(70))
      mappings.push_back({internalize(pivot), any_literal()});
    for (uint32_t n = r.below(3); n; --n) {
      const int32_t variable = fresh_variable(used);
      used.push_back(variable);
      true_lits.push_back(internalize(r.literal(variable)));
    }
    for (uint32_t n = r.below(3); n; --n) {
      const auto leased = leased_variables();
      int32_t variable = leased.empty() || r.percent(50)
          ? 0 : leased[r.below(static_cast<uint32_t>(leased.size()))];
      if (!variable || std::find(used.begin(), used.end(), variable) != used.end())
        variable = fresh_variable(used);
      used.push_back(variable);
      Lit to = any_literal();
      while (to == internalize(variable))
        to = any_literal();
      mappings.push_back({internalize(variable), to});
    }
    const bool propagates = r.percent(30);
    const uint64_t id = next_id++;
    const uint32_t external_variables = extent;
    const uint64_t epoch = l.reference().persistent_external_epoch();
    const Answers a = l.run("add_sr", [&](Subject &s, Answers &answers) {
      answers("accepted",
              s.proof.add_sr(id, lits.data(), static_cast<uint32_t>(lits.size()),
                             true_lits.data(), static_cast<uint32_t>(true_lits.size()),
                             mappings.data(), static_cast<uint32_t>(mappings.size()),
                             external_variables, propagates));
    });
    track_registry(epoch);
    if (a[0])
      live.push_back({id, c, false});
  }
  void del() {
    const int at = pick(false);
    if (at < 0)
      return;
    const Live c = take(at);
    const auto lits = internal(c.lits);
    const uint64_t line = next_id;
    l.run("del", [&](Subject &s, Answers &) {
      s.proof.del(line, c.id, lits.data(), static_cast<uint32_t>(lits.size()));
    });
  }

  // --- Proof-only clauses ---
  void external_rup() {
    Clause c = clause(1, 4);
    if (r.percent(10)) {
      if (r.percent(30))
        c[r.below(static_cast<uint32_t>(c.size()))] = r.percent(50) ? 0 : INT32_MIN;
      else
        spoil(c);
    }
    const uint64_t id = next_id++;
    const Answers a = l.run("add_external_rup", [&](Subject &s, Answers &answers) {
      answers("accepted", s.proof.add_external_rup(id, c.data(),
                                                   static_cast<uint32_t>(c.size())));
    });
    if (a[0])
      live.push_back({id, c, true});
  }
  void external_rat() {
    Clause c = clause(1, 3, pivot_variable());
    // --dsr writes no tautology, so its copy would miss the lease retirement.
    const bool tautology = !leasing() && r.percent(10);
    if (tautology)
      spoil(c);
    const bool nothing = r.percent(3);
    const uint64_t id = next_id++;
    const uint64_t epoch = l.reference().persistent_external_epoch();
    const Answers a = l.run("add_external_rat", [&](Subject &s, Answers &answers) {
      answers("accepted",
              nothing ? s.proof.add_external_rat(id, nullptr, 0)
                      : s.proof.add_external_rat(id, c.data(),
                                                 static_cast<uint32_t>(c.size())));
    });
    track_registry(epoch);
    if (a[0] && !tautology && !nothing)
      live.push_back({id, c, true});
  }
  void del_external() {
    const int at = pick(true);
    if (at < 0)
      return external_rup();
    const Live c = take(at);
    const uint64_t line = next_id;
    l.run("del_external", [&](Subject &s, Answers &answers) {
      answers("accepted", s.proof.del_external(line, c.id, c.lits.data(),
                                               static_cast<uint32_t>(c.lits.size())));
    });
  }
  void retain_external() {
    const int at = pick(true);
    Live c{0, {}, true};
    const uint32_t kind = r.below(10);
    if (at >= 0 && kind < 7) {
      c = take(at);
    } else if (!retained.empty() && kind == 7) {
      c = retained[r.below(static_cast<uint32_t>(retained.size()))];
    } else {
      c.lits = clause(1, 3);
      if (kind == 8)
        spoil(c.lits);
      else if (kind == 9)
        c.lits.clear();
    }
    const bool fresh = at >= 0 && kind < 7;
    const Answers a = l.run("retain_external", [&](Subject &s, Answers &answers) {
      answers("accepted", s.proof.retain_external(c.id, c.lits.data(),
                                                  static_cast<uint32_t>(c.lits.size())));
    });
    if (fresh)
      (a[0] ? retained : live).push_back(c);
  }

  // --- Premise leases ---
  std::vector<Proof::PremiseGroup> groups() {
    std::vector<Proof::PremiseGroup> result(1 + r.below(r.percent(10) ? 3 : 2));
    for (auto &group : result) {
      if (r.percent(3))
        continue; // an empty group
      for (uint32_t n = 1 + r.below(3); n; --n) {
        Clause body = live.empty() || r.percent(5)
            ? clause(1, 3) : live[r.below(static_cast<uint32_t>(live.size()))].lits;
        if (r.percent(2))
          spoil(body);
        group.push_back(body);
      }
    }
    return result;
  }
  Proof::DeferredDeletionLimits limits() {
    const uint32_t kind = r.below(10);
    if (kind < 6)
      return {};
    if (kind < 9)
      return {1 + r.below(4), 4 + r.below(30)};
    return r.percent(50) ? Proof::DeferredDeletionLimits{0, 8}
                         : Proof::DeferredDeletionLimits{2, 0};
  }
  uint64_t budget() { return r.percent(80) ? UINT64_MAX : r.below(200); }
  void retain_groups() {
    if (live.empty())
      return;
    const auto chosen = groups();
    const auto bounds = limits();
    const uint64_t spend = budget();
    const bool no_meter = r.percent(2);
    const uint64_t maximum_groups = r.percent(80) ? 8 : 1 + r.below(3);
    const uint64_t maximum_literals = r.percent(80) ? 100 : 2 + r.below(6);
    const uint64_t maximum_bytes = r.percent(80) ? 100000 : 200 + r.below(800);
    const Answers a = l.run("retain_premise_groups", [&](Subject &s, Answers &answers) {
      std::vector<uint64_t> handles{999};
      const std::function<bool(uint64_t)> charge =
          no_meter ? std::function<bool(uint64_t)>() : metered(s, spend);
      answers("accepted", s.proof.retain_premise_groups(
                              chosen, maximum_groups, maximum_literals,
                              maximum_bytes, charge, handles, bounds));
      answers("handles", handles.size());
      for (const uint64_t handle : handles)
        answers("handle", handle);
    });
    if (a[0])
      for (size_t i = 0; i < chosen.size(); ++i)
        l.handles.emplace_back(a[2 + i], chosen[i]);
  }
  void release_groups() {
    if (l.handles.empty())
      return retain_groups();
    std::vector<uint64_t> chosen;
    for (const auto &entry : l.handles)
      if (r.percent(50))
        chosen.push_back(entry.first);
    if (r.percent(5))
      chosen.push_back(999999);
    const uint64_t line = next_id;
    l.run("release_premise_groups", [&](Subject &s, Answers &) {
      s.proof.release_premise_groups(chosen, line);
    });
  }
  void can_defer() {
    const auto chosen = groups();
    const auto bounds = limits();
    const uint64_t spend = budget();
    l.run("can_defer_premise_group_deletions", [&](Subject &s, Answers &answers) {
      answers("deferrable",
              s.proof.can_defer_premise_group_deletions(chosen, bounds, metered(s, spend)));
    });
  }

  // --- Parity reasons ---
  // Equations from the pool (a repeat is a duplicate row) and a reason their
  // sum entails; sometimes one it does not, or a refutation.
  struct ParityQuery {
    std::vector<PbpParityEquation> equations;
    Clause reason;
  };
  ParityQuery parity_query() {
    std::vector<PbpParityEquation> equations;
    for (uint32_t n = 1 + r.below(3); n; --n)
      equations.push_back(pool[r.below(static_cast<uint32_t>(pool.size()))]);
    if (r.percent(4))
      return {{equations.front(), flipped(equations.front())}, {}};
    std::vector<int32_t> odd;
    bool rhs = false;
    for (const auto &equation : equations) {
      rhs ^= equation.rhs;
      for (const int32_t variable : equation.variables) {
        const auto found = std::find(odd.begin(), odd.end(), variable);
        if (found == odd.end())
          odd.push_back(variable);
        else
          odd.erase(found);
      }
    }
    Clause reason;
    bool negatives = false;
    for (const int32_t variable : odd) {
      reason.push_back(r.literal(variable));
      negatives ^= reason.back() < 0;
    }
    // The assignment that falsifies the reason must break the summed parity.
    if (!reason.empty() && negatives == rhs)
      reason.front() = -reason.front();
    if (!odd.empty() && r.percent(15)) { // a weaker reason
      std::vector<int32_t> used = odd;
      reason.push_back(r.literal(fresh_variable(used)));
    }
    if (!reason.empty() && r.percent(6)) // not entailed
      reason.erase(reason.begin());
    for (size_t i = reason.size(); i > 1; --i)
      std::swap(reason[i - 1], reason[r.below(static_cast<uint32_t>(i))]);
    return {equations, reason};
  }
  // A reason's clause becomes live; the empty one ends the proof.
  void parity_accepted(uint64_t id, const Clause &reason, uint32_t helpers) {
    extent += helpers;
    if (reason.empty())
      finished = true;
    else
      live.push_back({id, reason, true});
  }
  void parity_reason(bool submit) {
    const ParityQuery query = parity_query();
    const auto &equations = query.equations;
    const auto &reason = query.reason;
    const uint64_t id = r.percent(10) ? 0 : next_id++;
    const uint32_t starting = extent;
    const Answers a = l.run(submit ? "plan and add parity reason" : "plan parity reason",
                            [&](Subject &s, Answers &answers) {
      PbpParityReasonPlan plan;
      const bool planned = s.proof.plan_pbp_parity_reason(
          equations, reason.data(), static_cast<uint32_t>(reason.size()), starting,
          s.cache, plan);
      answers("planned", planned);
      answers("accepted", planned && submit &&
                              s.proof.add_prevalidated_pbp_parity_reason(
                                  id, equations, reason.data(),
                                  static_cast<uint32_t>(reason.size()), s.cache, plan));
      answers("helper_variables", plan.helper_variables);
      answers("valid", plan.valid);
      answers("cache_generation", plan.cache_generation);
      answers("proof_constraints", plan.proof_constraints);
      answers("retained_bytes", plan.retained_bytes);
      answers("starting_external_extent", plan.starting_external_extent);
      for (const int32_t variable : plan.odd_variables)
        answers("odd variable", static_cast<uint64_t>(variable));
      if (planned)
        s.plan = plan;
    });
    if (a[0]) {
      planned = true;
      planned_equations = equations;
      planned_reason = reason;
      planned_helpers = static_cast<uint32_t>(a[2]);
    }
    if (a[1])
      parity_accepted(id, reason, static_cast<uint32_t>(a[2]));
  }
  // The stored plan again: accepted only while its cache generation holds.
  void resubmit() {
    if (!planned)
      return parity_reason(true);
    const uint64_t id = next_id++;
    const Answers a = l.run("resubmit parity reason", [&](Subject &s, Answers &answers) {
      answers("accepted", s.proof.add_prevalidated_pbp_parity_reason(
                              id, planned_equations, planned_reason.data(),
                              static_cast<uint32_t>(planned_reason.size()), s.cache,
                              s.plan));
    });
    if (a[0])
      parity_accepted(id, planned_reason, planned_helpers);
  }
  void unplanned() {
    const ParityQuery query = parity_query();
    const auto &equations = query.equations;
    const auto &reason = query.reason;
    const uint64_t id = next_id++;
    l.run("add parity reason without a plan", [&](Subject &s, Answers &answers) {
      answers("accepted", s.proof.add_prevalidated_pbp_parity_reason(
                              id, equations, reason.data(),
                              static_cast<uint32_t>(reason.size()), s.cache, {}));
    });
  }

  // --- Stream boundaries ---
  void commit() {
    l.run("commit_clause_stream", [](Subject &s, Answers &answers) {
      answers("committed", s.proof.commit_clause_stream());
    });
  }
  void flush() {
    l.run("flush", [](Subject &s, Answers &) { s.proof.flush(); });
  }
  void settle() {
    l.run("settle_population_observer", [](Subject &s, Answers &answers) {
      answers("settled", s.proof.settle_population_observer());
    });
  }
  // A startup refutation; the stream must still be unpublished.
  void refute() {
    const PbpParityEquation &equation = pool[r.below(static_cast<uint32_t>(pool.size()))];
    const std::vector<PbpParityEquation> equations{equation, flipped(equation)};
    const bool parity = r.percent(50);
    const Answers a = l.run(parity ? "refute_parity" : "refute_by_counting",
                            [&](Subject &s, Answers &answers) {
      std::vector<int32_t> taken = startup_input;
      answers("refuted", parity ? s.proof.refute_parity(equations, write_pb)
                                : s.proof.refute_by_counting(write_pb, taken));
    });
    if (a[0])
      terminal = true;
  }

  // One event during the input transaction; an input refutation (the empty
  // clause) leaves only seeds and non-clausal events.
  void input_step() {
    const uint32_t roll = r.below(100);
    if (roll < 80 || (roll < 89 && finished)) return seed();
    if (roll < 85) return rup();
    if (roll < 89) return derived_unit();
    if (roll < 92) return del();
    if (roll < 93) return flush();
    if (roll < 94) return commit();
    if (roll < 96) return retain_groups();
    if (roll < 98) return parity_reason(true);
    if (roll < 99 || finished) return refute();
    return empty();
  }
  // One event of the search, or after its end: no clausal addition follows the
  // empty clause, and no clausal event at all follows a startup refutation.
  void step() {
    struct Kind {
      uint32_t weight;
      void (Script::*event)();
      bool after_finish, after_refutation;
    };
    static constexpr Kind kinds[] = {
        {14, &Script::rup, false, false},
        {4, &Script::derived_unit, false, true},
        {4, &Script::root_propagation, true, true},
        {5, &Script::rat, false, false},
        {8, &Script::sr, true, true},
        {12, &Script::del, true, false},
        {6, &Script::external_rup, true, true},
        {4, &Script::external_rat, true, true},
        {6, &Script::del_external, true, true},
        {4, &Script::retain_external, true, true},
        {5, &Script::retain_groups, true, true},
        {3, &Script::release_groups, true, true},
        {3, &Script::can_defer, true, true},
        {3, &Script::plan_only, true, true},
        {6, &Script::plan_and_add, true, true},
        {2, &Script::resubmit, true, true},
        {1, &Script::unplanned, true, true},
        {3, &Script::commit, true, true},
        {2, &Script::flush, true, true},
        {2, &Script::settle, true, true},
        {1, &Script::refute, true, true},
        {1, &Script::empty, false, false},
    };
    uint32_t total = 0;
    for (const Kind &kind : kinds)
      if (allowed(kind.after_finish, kind.after_refutation))
        total += kind.weight;
    uint32_t roll = r.below(total);
    for (const Kind &kind : kinds) {
      if (!allowed(kind.after_finish, kind.after_refutation))
        continue;
      if (roll < kind.weight)
        return (this->*kind.event)();
      roll -= kind.weight;
    }
  }
  bool allowed(bool after_finish, bool after_refutation) const {
    return (!finished || after_finish) && (!terminal || after_refutation);
  }
  void plan_only() { parity_reason(false); }
  void plan_and_add() { parity_reason(true); }
};

void run_script(uint64_t seed) {
  std::fprintf(stderr, "seed %" PRIu64 "\n", seed);
  Random r{seed};
  TemporaryDirectory directory;
  const bool binary = r.percent(50);
  const std::string switch_at =
      r.percent(15) ? "end" : std::to_string(1 + r.below(30));
  Lockstep l;
  l.seed = seed;
  for (const Mode mode : every_mode)
    open_subject(l.add(mode), directory / file_name(mode), binary, switch_at.c_str());
  Script s(l, r);
  for (uint32_t events = 3 + r.below(12); events; --events)
    s.input_step();
  s.finish_input();
  if (r.percent(15))
    s.refute();
  for (uint32_t events = 10 + r.below(70); events && !s.finished && !s.terminal;
       --events)
    s.step();
  if (!s.finished && !s.terminal && r.percent(30))
    s.empty();
  for (uint32_t events = r.below(12); events; --events)
    s.step();
}

// --- Fixed cases ---

const std::vector<Clause> inputs = {{1, 2, 3}, {1, -2, -3}, {-1, 2, -3},
                                    {-1, -2, 3}, {3, 4}, {-3, -4}};

void seed_inputs(Proof &p) {
  uint64_t id = 0;
  for (const Clause &c : inputs) {
    const auto lits = internal(c);
    p.seed_population(++id, lits.data(), static_cast<uint32_t>(lits.size()));
  }
  p.finish_input_transaction();
}

const std::vector<PbpParityEquation> refuting = {{{1, 2}, false}, {{1, 2}, true}};

// The wait status of `body` run in a child whose stderr goes to `said`.
int child_status(const std::function<void()> &body, const std::string &said) {
  std::fflush(stdout);
  std::fflush(stderr);
  const pid_t child = fork();
  assert(child >= 0);
  if (!child) {
    const int descriptor = ::open(said.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (descriptor < 0 || dup2(descriptor, 2) != 2)
      _exit(2);
    body();
    _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  return status;
}

// The child reaches its last event (it says "reached") and aborts there.
void expect_abort(const std::function<void()> &body, const char *what) {
  TemporaryDirectory directory;
  const std::string said = directory / "stderr";
  const int status = child_status(body, said);
  if (WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT &&
      read_all(said).find("reached") != std::string::npos)
    return;
  std::fprintf(stderr, "FAIL %s: status %d, said:\n%s\n", what, status,
               read_all(said).c_str());
  std::abort();
}

// The child stops with exit status 1 and a proof error that says `needle`.
void expect_proof_error(const std::function<void()> &body, const char *needle,
                        const char *what) {
  TemporaryDirectory directory;
  const std::string said = directory / "stderr";
  const int status = child_status(body, said);
  if (WIFEXITED(status) && WEXITSTATUS(status) == 1 &&
      read_all(said).find(needle) != std::string::npos)
    return;
  std::fprintf(stderr, "FAIL %s: status %d, said:\n%s\n", what, status,
               read_all(said).c_str());
  std::abort();
}

void reached() { std::fputs("reached\n", stderr); }

// One complete PBP proof: the header, then lines, then the conclusion once.
void expect_complete_pbp(const std::string &text, const char *what) {
  const std::string conclusion =
      "output NONE ;\nconclusion UNSAT : -1 ;\nend pseudo-Boolean proof ;\n";
  if (text.rfind("pseudo-Boolean proof version 3.0\nf ", 0) == 0 &&
      text.size() > conclusion.size() &&
      !text.compare(text.size() - conclusion.size(), conclusion.size(), conclusion) &&
      text.find("conclusion") == text.rfind("conclusion"))
    return;
  std::fprintf(stderr, "FAIL %s: not one complete PBP proof:\n%s\n", what,
               text.substr(0, 600).c_str());
  std::abort();
}

// Every streaming ledger stops at a clausal line after the empty clause, and at
// any clausal event after a startup refutation, with or without a proof.
void clausal_events_after_the_end_abort() {
  const auto line = internal({1, 5});
  for (const Mode mode : {Mode::None, Mode::Pbp, Mode::Dsr, Mode::Automatic})
    for (const int ending : {0, 1, 2, 3}) {
      TemporaryDirectory directory;
      expect_abort([&] {
        Subject s(mode);
        open_subject(s, directory / "proof", true);
        seed_inputs(s.proof);
        if (ending < 2)
          s.proof.add_empty(10);
        else
          assert(s.proof.refute_parity(refuting, write_pb));
        reached();
        if (ending == 1)
          s.proof.add_rat(11, line.data(), 2);
        else if (ending == 3)
          s.proof.del(11, 1, internal(inputs[0]).data(), 3);
        else
          s.proof.add_rup(11, line.data(), 2);
      }, mode_name(mode));
    }
}

// An empty clause inside the input transaction refutes the run before its
// stream is published. No mode then takes a startup refutation, though the
// automatic and --dsr writers already kept their copy at that clause.
void no_startup_refutation_after_an_input_refutation() {
  for (const bool binary : {true, false})
    for (const bool parity : {true, false}) {
      TemporaryDirectory directory;
      {
        Lockstep l;
        for (const Mode mode : every_mode)
          open_subject(l.add(mode), directory / file_name(mode), binary, "end");
        l.run("an input refutation", [](Subject &s, Answers &) {
          uint64_t id = 0;
          for (const Clause &c : inputs) {
            const auto lits = internal(c);
            s.proof.seed_population(++id, lits.data(),
                                    static_cast<uint32_t>(lits.size()));
          }
          s.proof.add_empty(++id);
          s.proof.finish_input_transaction();
        });
        const Answers a = l.run("startup refutation", [&](Subject &s, Answers &answers) {
          std::vector<int32_t> input{1, 0, -1, 0};
          answers("refuted", parity ? s.proof.refute_parity(refuting, write_pb)
                                    : s.proof.refute_by_counting(write_pb, input));
          answers("input kept", input.size());
        });
        assert(!a[0] && a[1] == 4 && !a[3] /* startup_pb_written */);
        l.run("after the refutation", [](Subject &s, Answers &answers) {
          answers("committed", s.proof.commit_clause_stream());
          s.proof.flush();
        });
      }
      expect_complete_pbp(read_all(directory / file_name(Mode::Pbp)), "--pbp");
      assert(!read_all(directory / file_name(Mode::Dsr)).empty());
    }
}

size_t open_descriptors() {
  DIR *directory = opendir("/proc/self/fd");
  assert(directory);
  size_t count = 0;
  while (readdir(directory))
    ++count;
  closedir(directory);
  return count;
}

// A run without a proof opens no file, from open to teardown; a proof does.
void no_proof_opens_no_file() {
  const size_t before = open_descriptors();
  {
    Subject s(Mode::None);
    open_subject(s, "", true);
    seed_inputs(s.proof);
    const auto unit = internal({4});
    s.proof.add_rup(7, unit.data(), 1);
    s.proof.add_empty(8);
    s.proof.flush();
    assert(open_descriptors() == before);
  }
  assert(open_descriptors() == before);
  TemporaryDirectory directory;
  Proof proof; // its destination and its private input-transaction file
  assert(proof.open((directory / "proof").c_str(), ProofFormat::PbpStreaming));
  assert(open_descriptors() == before + 2);
}

// An SR line whose auxiliary would pass the checkers' names: every proof that
// can go on accepts it alike (the automatic copy converts), and --dsr stops.
void sr_beyond_the_name_bound() {
  const auto lits = internal({1, 6});
  const DsrLiteralMapping mapping[] = {{internalize(1), internalize(-1)}};
  const Body sr = [&](Subject &s, Answers &answers) {
    answers("accepted", s.proof.add_sr(20, lits.data(), 2, nullptr, 0, mapping, 1,
                                       INT32_MAX, true));
  };
  {
    TemporaryDirectory directory;
    Lockstep l;
    open_subject(l.add(Mode::None), directory / "none", true);
    open_subject(l.add(Mode::Pbp), directory / "pbp", true);
    Subject &automatic = l.add(Mode::Automatic);
    open_subject(automatic, directory / "automatic", true);
    l.run("inputs", [](Subject &s, Answers &) { seed_inputs(s.proof); });
    assert(l.run("add_sr", sr)[0]);
    assert(automatic.proof.automatic_reason() == AutomaticProofReason::CopyDeclined);
  }
  TemporaryDirectory directory;
  expect_proof_error([&] {
    Subject s(Mode::Dsr);
    open_subject(s, directory / "proof", true);
    seed_inputs(s.proof);
    reached();
    Answers ignored;
    sr(s, ignored);
  }, "2^30-1", "--dsr SR past the name bound");
}

// A --dsr startup parity refutation is spelled in clauses, and its ledger is
// as terminal as every other mode's after a PB refutation.
void dsr_startup_parity_refutation() {
  for (const bool binary : {true, false}) {
    TemporaryDirectory directory;
    {
      Lockstep l;
      for (const Mode mode : {Mode::None, Mode::Pbp, Mode::Dsr, Mode::Automatic})
        open_subject(l.add(mode), directory / file_name(mode), binary);
      l.run("inputs", [](Subject &s, Answers &) { seed_inputs(s.proof); });
      const Answers a = l.run("refute_parity", [](Subject &s, Answers &answers) {
        answers("refuted", s.proof.refute_parity(refuting, write_pb));
      });
      assert(a[0] && a[2] /* startup_pb_written */);
      const int32_t external[] = {1, 3};
      const auto unit = internal({4});
      const DsrLiteralMapping flip[] = {{internalize(4), internalize(-4)}};
      l.run("after the refutation", [&](Subject &s, Answers &answers) {
        Proof &p = s.proof;
        answers("committed", p.commit_clause_stream());
        answers("add_sr", p.add_sr(10, unit.data(), 1, nullptr, 0, flip, 1, 16));
        answers("add_external_rup", p.add_external_rup(11, external, 2));
        answers("retain_external", p.retain_external(11, external, 2));
        answers("del_external", p.del_external(12, 11, external, 2));
        p.begin_deferred_root_units();
        p.add_derived_unit(12, unit[0]);
        p.end_deferred_root_units();
        p.flush();
        answers("settled", p.settle_population_observer());
      });
    }
    assert(!read_all(directory / file_name(Mode::Dsr)).empty());
    assert(read_all(directory / file_name(Mode::Pbp)).rfind("pseudo-Boolean", 0) == 0);
  }
}

size_t count(const std::string &text, const std::string &line) {
  size_t n = 0;
  for (size_t at = text.find("\n" + line); at != std::string::npos;
       at = text.find("\n" + line, at + 1))
    ++n;
  return n;
}

// Every constraint a `pol` line names was written: no id is 0.
void expect_named_rows(const std::string &text, const char *what) {
  std::istringstream lines(text);
  for (std::string line; std::getline(lines, line);) {
    if (line.rfind("pol ", 0))
      continue;
    std::istringstream tokens(line.substr(4));
    std::vector<std::string> words;
    for (std::string word; tokens >> word;)
      words.push_back(word);
    for (size_t i = 0; i < words.size(); ++i)
      if (words[i] == "0" && (i + 1 == words.size() || words[i + 1] != "d")) {
        std::fprintf(stderr, "FAIL %s: an unwritten row: %s\n", what, line.c_str());
        std::abort();
      }
  }
}

// A parity reason under a plan; true when accepted.
bool parity_reason(Lockstep &l, uint64_t id, std::vector<PbpParityEquation> equations,
                   Clause reason, uint32_t extent) {
  const Answers a = l.run("parity reason", [&](Subject &s, Answers &answers) {
    PbpParityReasonPlan plan;
    const bool planned = s.proof.plan_pbp_parity_reason(
        equations, reason.data(), static_cast<uint32_t>(reason.size()), extent,
        s.cache, plan);
    answers("accepted", planned && s.proof.add_prevalidated_pbp_parity_reason(
                                       id, equations, reason.data(),
                                       static_cast<uint32_t>(reason.size()),
                                       s.cache, plan));
    answers("helper_variables", plan.helper_variables);
  });
  return a[0];
}

// The first parity reason switches the automatic proof. The rows it wrote
// belong to the abandoned numbering, so the next reason writes them again, as
// it does after a witness retires them; a new row is written before its use.
void parity_rows_after_a_switch() {
  for (const bool binary : {true, false}) {
    TemporaryDirectory directory;
    {
      Lockstep l;
      open_subject(l.add(Mode::Pbp), directory / "pbp", binary);
      Subject &switched = l.add(Mode::Switched);
      open_subject(switched, directory / "switched", binary, "1");
      l.run("inputs", [](Subject &s, Answers &answers) {
        seed_inputs(s.proof);
        answers("committed", s.proof.commit_clause_stream());
      });
      const PbpParityEquation three{{1, 2, 3}, true}, two{{3, 4}, false};
      assert(parity_reason(l, 7, {three}, {1, 2, 3}, 16));
      assert(switched.proof.automatic_reason() == AutomaticProofReason::ForcedSwitch);
      assert(parity_reason(l, 8, {three}, {2, 3, 1}, 17));
      assert(parity_reason(l, 9, {three, two}, {1, 2, 4}, 17));
      const auto line = internal({1, 5});
      const DsrLiteralMapping flip[] = {{internalize(1), internalize(-1)}};
      const Answers a = l.run("witnessed add_sr", [&](Subject &s, Answers &answers) {
        answers("accepted", s.proof.add_sr(10, line.data(), 2, nullptr, 0, flip, 1, 18));
      });
      assert(a[0]);
      assert(parity_reason(l, 11, {two, three}, {4, 1, 2}, 18));
      l.run("add_empty", [](Subject &s, Answers &) { s.proof.add_empty(12); });
    }
    for (const char *name : {"pbp", "switched"}) {
      const std::string text = read_all(directory / name);
      expect_complete_pbp(text, name);
      expect_named_rows(text, name);
    }
  }
}

// Staged root units settle before the external event that switches the
// automatic proof: the converted DSR states each unit once and the PBP that
// follows does not state it again.
void staged_units_then_a_switch() {
  for (const bool binary : {true, false}) {
    TemporaryDirectory directory;
    {
      Lockstep l;
      Subject &pbp = l.add(Mode::Pbp);
      open_subject(pbp, directory / "pbp", binary);
      Subject &switched = l.add(Mode::Switched);
      open_subject(switched, directory / "switched", binary, "1");
      l.run("inputs", [](Subject &s, Answers &) { seed_inputs(s.proof); });
      l.run("begin_deferred_root_units",
            [](Subject &s, Answers &) { s.proof.begin_deferred_root_units(); });
      l.run("add_derived_unit", [](Subject &s, Answers &) {
        s.proof.add_derived_unit(7, internalize(5));
      });
      l.run("add_derived_unit", [](Subject &s, Answers &) {
        s.proof.add_derived_unit(8, internalize(-6));
      });
      l.run("end_deferred_root_units",
            [](Subject &s, Answers &) { s.proof.end_deferred_root_units(); });
      assert(switched.proof.automatic_reason() == AutomaticProofReason::Pending);
      const int32_t external[] = {5, 7, 8};
      const Answers a = l.run("add_external_rup", [&](Subject &s, Answers &answers) {
        answers("accepted", s.proof.add_external_rup(9, external, 3));
      });
      assert(a[0]);
      assert(switched.proof.automatic_reason() == AutomaticProofReason::ForcedSwitch);
      l.run("add_empty", [](Subject &s, Answers &) { s.proof.add_empty(10); });
      for (const Subject *s : {&pbp, &switched}) {
        const Proof::PbpDeletionCounters &counters = s->proof.pbp_deletion_counters();
        assert(counters.deferred_batches == 1 && counters.deferred_units == 2);
        assert(counters.dropped_batches == 0 && counters.dropped_units == 0);
      }
    }
    for (const char *name : {"pbp", "switched"}) {
      const std::string text = read_all(directory / name);
      expect_complete_pbp(text, name);
      if (count(text, "rup 1 x5 >= 1 ;\n") != 1 ||
          count(text, "rup 1 ~x6 >= 1 ;\n") != 1) {
        std::fprintf(stderr, "FAIL %s: a staged unit is not stated once:\n%s\n",
                     name, text.c_str());
        std::abort();
      }
    }
  }
}

} // namespace

int main(int argc, char **argv) {
  for (const char *knob : {"TACTIX_AUTO_PROOF_SWITCH_AT", "TACTIX_AUTO_PROOF_DSR_LIMIT",
                           "TACTIX_AUTO_PROOF_PARITY_BYTES"})
    unsetenv(knob);
  const uint64_t scripts = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 2000;
  const uint64_t first = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1;
  clausal_events_after_the_end_abort();
  no_startup_refutation_after_an_input_refutation();
  no_proof_opens_no_file();
  sr_beyond_the_name_bound();
  dsr_startup_parity_refutation();
  parity_rows_after_a_switch();
  staged_units_then_a_switch();
  for (uint64_t i = 0; i < scripts; ++i)
    run_script(first + i);
  std::printf("proof ledger: %" PRIu64 " scripts agree\n", scripts);
  return 0;
}
