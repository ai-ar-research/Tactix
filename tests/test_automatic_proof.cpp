// Automatic proof (plan §5): one proof at a time. The kernel sees --pbp, only
// the DSR copy is written, and a trigger converts it to PBP and continues on
// PBP. Runs that keep DSR compare with a single-format writer byte for byte; a
// conversion compares with its expected lines here, and tests/cli_auto_proof.sh
// and test_dsr_to_pbp check the semantics through the real checkers.
#undef NDEBUG
#include "proof/proof.hpp"
#include "rule_test_utils.hpp"
#include "test_io.hpp"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <functional>
#include <filesystem>
#include <set>
#include <sstream>
#include <string>
#include <csignal>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace tactix;

namespace {

void expect_equal(const std::string &got, const std::string &want,
                  const char *what) {
  if (got == want)
    return;
  std::fprintf(stderr, "FAIL %s: %zu bytes, want %zu\n", what, got.size(),
               want.size());
  std::abort();
}

// A fresh directory; removed with its entries at scope end.
struct TemporaryDirectory {
  std::string path;
  TemporaryDirectory() {
    const char *base = std::getenv("TMPDIR");
    path = std::string(base && *base ? base : "/tmp") + "/tactix-auto-XXXXXX";
    assert(mkdtemp(path.data()));
  }
  std::vector<std::string> entries() const {
    std::vector<std::string> names;
    DIR *directory = opendir(path.c_str());
    assert(directory);
    while (const dirent *entry = readdir(directory))
      if (std::strcmp(entry->d_name, ".") && std::strcmp(entry->d_name, ".."))
        names.push_back(entry->d_name);
    closedir(directory);
    return names;
  }
  std::string operator/(const char *name) const { return path + "/" + name; }
  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path,error);
    assert(!error);
  }
};

// Make every later write to the open file `path` fail with ENOSPC: point its
// writable descriptors at /dev/full. A regular caller stage stays a regular
// file when opened, so this exercises copy write failures without a full
// filesystem; read-only descriptors (the lazy journal's stage reader) keep
// reading the bytes written before, as they would after a real ENOSPC.
bool fail_writes_to(const std::string &path) {
  char resolved[PATH_MAX];
  if (!realpath(path.c_str(), resolved) || access("/dev/full", W_OK))
    return false;
  DIR *fds = opendir("/proc/self/fd");
  if (!fds)
    return false;
  bool redirected = false;
  while (const dirent *entry = readdir(fds)) {
    char target[PATH_MAX];
    const std::string link = std::string("/proc/self/fd/") + entry->d_name;
    const ssize_t length = readlink(link.c_str(), target, sizeof target - 1);
    if (length <= 0)
      continue;
    target[length] = 0;
    if (std::strcmp(target, resolved))
      continue;
    const int flags = fcntl(std::atoi(entry->d_name), F_GETFL);
    if (flags >= 0 && (flags & O_ACCMODE) == O_RDONLY)
      continue;
    const int full = open("/dev/full", O_WRONLY);
    redirected = full >= 0 && dup2(full, std::atoi(entry->d_name)) >= 0;
    if (full >= 0)
      close(full);
  }
  closedir(fds);
  return redirected;
}

using Events = std::function<void(Proof &)>;

const auto inputs = [](Proof &p) {
  const auto a = lits_of({1, 2, 3}), b = lits_of({-1, 2}), c = lits_of({-2, 3}),
             d = lits_of({-3, 4, 5}), e = lits_of({-3, -4}), f = lits_of({-3, -5});
  p.seed_population(1, a.data(), 3);
  p.seed_population(2, b.data(), 2);
  p.seed_population(3, c.data(), 2);
  p.seed_population(4, d.data(), 3);
  p.seed_population(5, e.data(), 2);
  p.seed_population(6, f.data(), 2);
  p.finish_input_transaction();
};

// A small CDCL-shaped refutation: learned clauses, a deletion, root units.
const auto refutation = [](Proof &p) {
  const auto l1 = lits_of({-3, 4}), l2 = lits_of({-3}), u = lits_of({3});
  p.add_rup(7, l1.data(), 2);
  p.del(8, 4, lits_of({-3, 4, 5}).data(), 3);
  p.add_rup(9, l2.data(), 1);
  p.add_derived_unit(10, internalize(-3));
  p.add_rup(11, lits_of({2}).data(), 1);
  p.add_empty(12);
  (void)u;
};

std::string single(ProofFormat format, const Events &events) {
  TemporaryDirectory directory;
  const std::string path = directory / "single";
  {
    Proof p;
    assert(format == ProofFormat::PbpStreaming ? p.open(path.c_str(), format)
                                               : p.open_dsr_writer(path.c_str(), format));
    events(p);
  }
  return read_file(path);
}

// A complete proof converted from DSR: the header, then lines, then the
// conclusion once.
void expect_complete_pbp(const std::string &text, const char *what) {
  const std::string conclusion =
      "output NONE ;\nconclusion UNSAT : -1 ;\nend pseudo-Boolean proof ;\n";
  const bool header = text.rfind("pseudo-Boolean proof version 3.0\nf ", 0) == 0;
  const bool ends = text.size() > conclusion.size() &&
                    !text.compare(text.size() - conclusion.size(),
                                  conclusion.size(), conclusion);
  if (!header || !ends || text.find("conclusion") != text.rfind("conclusion")) {
    std::fprintf(stderr, "%s: not one complete PBP proof:\n%s\n", what,
                 text.substr(0, 400).c_str());
    std::abort();
  }
}

size_t count_lines(const std::string &text, const char *prefix) {
  size_t count = 0;
  const std::string needle = std::string("\n") + prefix;
  for (size_t at = text.find(needle); at != std::string::npos;
       at = text.find(needle, at + 1))
    ++count;
  return count;
}

// The only proof cannot be written: the run aborts, as --dsr does.
void expect_abort(const std::function<void()> &body) {
  const pid_t child = fork();
  assert(child >= 0);
  if (!child) {
    (void)!freopen("/dev/null", "w", stderr);
    body();
    _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

void no_trigger_selects_dsr() {
  const Events events = [](Proof &p) { inputs(p); refutation(p); };
  for (const bool binary : {true, false}) {
    TemporaryDirectory directory;
    const std::string path = directory / "proof";
    std::string route;
    {
      Proof p;
      assert(p.open_automatic(path.c_str(), binary));
      assert(p.automatic() && p.automatic_dsr_live());
      assert(directory.entries() == std::vector<std::string>{"proof"});
      events(p);
      assert(p.automatic_reason() == AutomaticProofReason::DsrSelected);
      assert(!p.automatic_dsr_live());
      route = p.verification_route();
      assert(directory.entries() == std::vector<std::string>{"proof"});
    }
    assert(route == "dsr-cake_lpr");
    expect_equal(read_file(path),
                 single(binary ? ProofFormat::DsrBinary : ProofFormat::DsrAscii,
                        events),
                 binary ? "binary DSR selected" : "ASCII DSR selected");
    // The proof keeps the mode an ordinary proof file would have.
    const std::string reference = directory / "reference";
    std::fclose(std::fopen(reference.c_str(), "wb"));
    struct stat st, want;
    assert(!stat(path.c_str(), &st) && !stat(reference.c_str(), &want));
    assert((st.st_mode & 07777) == (want.st_mode & 07777));
  }
}

void startup_pb_keeps_pbp() {
  const Events events = [](Proof &p) {
    inputs(p);
    FILE *terminal = p.begin_pb();
    assert(terminal);
    std::fputs("pseudo-Boolean proof version 3.0\nf 6 ;\n", terminal);
    assert(p.finish_pb());
  };
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    events(p);
    assert(p.automatic_reason() ==
           AutomaticProofReason::StartupPbRefutation);
    assert(std::string(p.verification_route()) == "pbp");
    assert(directory.entries() == std::vector<std::string>{"proof"});
  }
  expect_equal(read_file(path), single(ProofFormat::PbpStreaming, events),
               "terminal PB");
}

// A witnessed line deletes the affine support its witness would have to hold
// against, in every stream, so the DSR copy follows it. A witness-free line
// transforms nothing and deletes nothing.
void sr_under_affine_support() {
  const Events lease = [](Proof &p) {
    p.arm_population_shadow();
    std::vector<uint64_t> handles;
    assert(p.retain_premise_groups({{{-1, 2}}}, 8, 100, 100000,
                                   [](uint64_t) { return true; }, handles,
                                   {4, 100}));
    p.del(30, 2, lits_of({-1, 2}).data(), 2);
  };
  const Events retain = [](Proof &p) {
    const int32_t support[] = {1, 2};
    assert(p.retain_external(20, support, 2));
  };
  const Events free_of_witness = [](Proof &p) {
    const auto body = lits_of({1, 2});
    const DsrLiteralMapping identity{internalize(1), internalize(1)};
    const Lit metadata = internalize(3);
    assert(plan_dsr_witness(body.data(), 2, &metadata, 1, &identity, 1).form ==
           DsrWitnessForm::Rup);
    assert(p.add_sr(21, body.data(), 2, &metadata, 1, &identity, 1, 7));
  };
  const Events witnessed = [](Proof &p) {
    const auto sr = lits_of({1, 6});
    const DsrLiteralMapping mapping[] = {{internalize(1), internalize(-1)}};
    assert(p.add_sr(22, sr.data(), 2, nullptr, 0, mapping, 1, 7));
  };
  for (const Events &support : {lease, retain}) {
    const Events events = [&](Proof &p) {
      inputs(p);
      support(p);
      free_of_witness(p);
      witnessed(p);
      refutation(p);
    };
    const std::string want = single(ProofFormat::DsrAscii, events);
    TemporaryDirectory directory;
    const std::string path = directory / "proof";
    {
      Proof p;
      assert(p.open_automatic(path.c_str(), false));
      events(p);
      assert(p.automatic_reason() == AutomaticProofReason::DsrSelected);
      assert(std::string(p.verification_route()) == "dsr-trestle");
    }
    expect_equal(read_file(path), want, "automatic DSR under affine support");
    {
      Proof p;
      assert(p.open(path.c_str(), ProofFormat::DsrAscii));
      events(p);
    }
    expect_equal(read_file(path), want, "--dsr under affine support");
    {
      Proof p;
      assert(p.open(path.c_str(), ProofFormat::PbpStreaming));
      events(p);
    }
    const std::string text = read_file(path);
    expect_complete_pbp(text, "PBP under affine support");
    const size_t witness = text.find("x1 -> ~x1");
    assert(witness != std::string::npos);
    assert(text.find("del ") < witness);
  }
}

// A line DSR cannot spell, for want of an auxiliary variable, converts before either
// stream deletes anything for it, so the support is deleted once.
void sr_beyond_the_auxiliary_limit() {
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    inputs(p);
    const int32_t support[] = {1, 2};
    assert(p.retain_external(20, support, 2));
    const auto sr = lits_of({1, 6});
    const DsrLiteralMapping mapping[] = {{internalize(1), internalize(-1)}};
    assert(p.add_sr(21, sr.data(), 2, nullptr, 0, mapping, 1, INT32_MAX, true));
    assert(p.automatic_reason() == AutomaticProofReason::CopyDeclined);
    assert(p.persistent_external_epoch() == 1);
    refutation(p);
    assert(std::string(p.verification_route()) == "pbp");
  }
  const std::string text = read_file(path);
  expect_complete_pbp(text, "PBP after a line DSR cannot spell");
  assert(count_lines(text, "red ") == 1);
}

// The largest variable name drat-trim and dsr-trim read.
constexpr uint32_t NAME_BOUND = (uint32_t{1} << 30) - 1;

// The run stops with exit status 1 and a proof error that says `needle`.
void expect_proof_error(const std::function<void()> &body, const char *needle) {
  TemporaryDirectory directory;
  const std::string said = directory / "stderr";
  const pid_t child = fork();
  assert(child >= 0);
  if (!child) {
    const int descriptor = open(said.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (descriptor < 0 || dup2(descriptor, 2) != 2)
      _exit(2);
    body();
    _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
  assert(read_file(said).find(needle) != std::string::npos);
}

// A parity reason, which the DSR copy spells with proof-only helpers.
void parity_reason(Proof &p) {
  const PbpParityEquation equation{{1, 2}, false};
  const int32_t reason[] = {-1, 2};
  assert(p.commit_clause_stream());
  PbpParityBoundsCache cache;
  PbpParityReasonPlan plan;
  assert(p.plan_pbp_parity_reason({equation}, reason, 2, 2, cache, plan));
  assert(p.add_prevalidated_pbp_parity_reason(30, {equation}, reason, 2, cache, plan));
}

// How many helpers that reason names after `inputs`: the names its lines carry
// beyond its own variables.
uint32_t parity_helpers() {
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), false, nullptr, true));
    inputs(p);
    parity_reason(p);
    p.flush();
  }
  std::set<long> names;
  std::istringstream text(read_file(path));
  for (std::string token; text >> token;)
    if (token != "d")
      names.insert(std::labs(std::stol(token)));
  for (const long own : {0L, 1L, 2L})
    names.erase(own);
  return static_cast<uint32_t>(names.size());
}

// The auxiliary of an SR line takes the checker name above every helper. One
// that would pass the bound converts under --auto-proof; one at the bound is
// written.
void sr_auxiliary_name_counts_helpers() {
  const uint32_t helpers = parity_helpers();
  assert(helpers > 0);
  const auto sr = lits_of({1, 6});
  const DsrLiteralMapping mapping[] = {{internalize(1), internalize(-1)}};
  for (const bool past : {false, true}) {
    TemporaryDirectory directory;
    const std::string path = directory / "proof";
    {
      Proof p;
      assert(p.open_automatic(path.c_str(), false));
      inputs(p);
      parity_reason(p);
      // The auxiliary is external variable `external_variables + 1`.
      const uint32_t external_variables = NAME_BOUND - helpers - !past;
      assert(p.add_sr(21, sr.data(), 2, nullptr, 0, mapping, 1,
                      external_variables, true));
      assert(p.automatic_reason() == (past ? AutomaticProofReason::CopyDeclined
                                           : AutomaticProofReason::Pending));
      refutation(p);
    }
    const std::string text = read_file(path);
    if (past)
      expect_complete_pbp(text, "PBP after an auxiliary past the name bound");
    else
      assert(text.find(std::to_string(NAME_BOUND) + " ") != std::string::npos);
  }
}

// --dsr writes every name up to the bound and stops, naming the bound, at a
// line that would pass it: an SR auxiliary, a solver variable above the
// helpers, or a new helper.
void dsr_only_stops_at_the_name_bound() {
  const uint32_t helpers = parity_helpers();
  const auto sr = lits_of({1, 6});
  const DsrLiteralMapping mapping[] = {{internalize(1), internalize(-1)}};
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  const auto run = [&](const Events &events) {
    Proof p;
    assert(p.open_automatic(path.c_str(), false, nullptr, true));
    inputs(p);
    events(p);
  };
  run([&](Proof &p) {
    parity_reason(p);
    assert(p.add_sr(21, sr.data(), 2, nullptr, 0, mapping, 1,
                    NAME_BOUND - helpers - 1, true));
    const int32_t top[] = {1, static_cast<int32_t>(NAME_BOUND - helpers)};
    assert(p.add_external_rup(22, top, 2));
    refutation(p);
  });
  const std::string text = read_file(path);
  const std::string name = std::to_string(NAME_BOUND);
  assert(text.find("-" + name + " ") != std::string::npos); // the auxiliary
  assert(text.find("\n1 " + name + " 0\n") != std::string::npos);
  expect_proof_error([&] {
    run([&](Proof &p) {
      parity_reason(p);
      p.add_sr(21, sr.data(), 2, nullptr, 0, mapping, 1, NAME_BOUND - helpers,
               true);
    });
  }, "2^30-1");
  expect_proof_error([&] {
    run([&](Proof &p) {
      parity_reason(p);
      const int32_t past[] = {1, static_cast<int32_t>(NAME_BOUND - helpers + 1)};
      p.add_external_rup(22, past, 2);
    });
  }, "2^30-1");
  expect_proof_error([&] {
    run([&](Proof &p) {
      const int32_t top[] = {1, static_cast<int32_t>(NAME_BOUND)};
      assert(p.add_external_rup(22, top, 2));
      parity_reason(p);
    });
  }, "2^30-1");
}

// A RAT line deletes retained support even with no lease left.
void rat_line_deletes_retained_support() {
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  Proof p;
  assert(p.open(path.c_str(), ProofFormat::PbpStreaming));
  inputs(p);
  const int32_t support[] = {1, 2};
  assert(p.retain_external(20, support, 2));
  const auto line = lits_of({1, 6});
  p.add_rat(21, line.data(), 2);
  assert(p.persistent_external_epoch() == 1);
}

void witnessed_sr_selects_trestle_route() {
  const Events events = [](Proof &p) {
    inputs(p);
    const auto sr = lits_of({6, 1});
    const DsrLiteralMapping mapping[] = {{internalize(6), internalize(-6)}};
    assert(p.add_sr(21, sr.data(), 2, nullptr, 0, mapping, 1, 7));
    refutation(p);
  };
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  std::string route;
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    events(p);
    assert(p.automatic_reason() == AutomaticProofReason::DsrSelected);
    route = p.verification_route();
  }
  assert(route == "dsr-trestle");
  expect_equal(read_file(path), single(ProofFormat::DsrBinary, events),
               "witnessed DSR selected");
}

// A symlinked destination is rewritten through the link, never replaced.
void symlink_destination_is_copied() {
  const Events events = [](Proof &p) { inputs(p); refutation(p); };
  TemporaryDirectory directory;
  const std::string target = directory / "target", link = directory / "link";
  std::fclose(std::fopen(target.c_str(), "wb"));
  assert(!symlink(target.c_str(), link.c_str()));
  {
    Proof p;
    assert(p.open_automatic(link.c_str(), true));
    assert(directory.entries().size() == 2); // an anonymous stage
    events(p);
    assert(p.automatic_reason() == AutomaticProofReason::DsrSelected);
  }
  struct stat st;
  assert(!lstat(link.c_str(), &st) && S_ISLNK(st.st_mode));
  expect_equal(read_file(target), single(ProofFormat::DsrBinary, events),
               "DSR copied through a symlink");
}

// A caller-owned stage is copied from and emptied, never renamed or removed.
void caller_stage_is_copied() {
  const Events events = [](Proof &p) { inputs(p); refutation(p); };
  TemporaryDirectory directory;
  const std::string path = directory / "proof", stage = directory / "stage";
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true, stage.c_str()));
    events(p);
    assert(p.automatic_reason() == AutomaticProofReason::DsrSelected);
  }
  expect_equal(read_file(path), single(ProofFormat::DsrBinary, events),
               "DSR copied from a caller stage");
  expect_equal(read_file(stage), "", "emptied caller stage");
}

// A copy that cannot be written is discarded; the PBP stream is unaffected.
void failed_copy_aborts() {
  if (access("/proc/self/fd", R_OK) || access("/dev/full", W_OK))
    return;
  expect_abort([] {
    TemporaryDirectory directory;
    const std::string path = directory / "proof", stage = directory / "stage";
    Proof p;
    assert(p.open_automatic(path.c_str(), true, stage.c_str()));
    assert(fail_writes_to(stage));
    inputs(p);
    refutation(p);
    p.flush();
  });
}

// The stage's last buffer fails during the conversion at the empty clause: no
// proof remains, so the run must not go on to claim UNSAT.
void failed_stage_at_the_switch_aborts() {
  if (access("/proc/self/fd", R_OK) || access("/dev/full", W_OK))
    return;
  expect_abort([] {
    setenv("TACTIX_AUTO_PROOF_SWITCH_AT", "end", 1);
    TemporaryDirectory directory;
    const std::string path = directory / "proof", stage = directory / "stage";
    Proof p;
    assert(p.open_automatic(path.c_str(), true, stage.c_str()));
    assert(fail_writes_to(stage));
    inputs(p);
    refutation(p);
  });
}

// A conversion that cannot be written at the empty clause keeps the complete
// DSR proof, which is smaller than the PBP text this file limit stops.
void failed_conversion_keeps_dsr() {
  const Events events = [](Proof &p) { inputs(p); refutation(p); };
  const std::string want = single(ProofFormat::DsrBinary, events);
  assert(want.size() < 40);
  for (const bool caller_stage : {true, false}) {
    const pid_t child = fork();
    assert(child >= 0);
    if (!child) {
      signal(SIGXFSZ, SIG_IGN);
      const rlimit limit{60, 60};
      setrlimit(RLIMIT_FSIZE, &limit);
      setenv("TACTIX_AUTO_PROOF_SWITCH_AT", "end", 1);
      TemporaryDirectory directory;
      const std::string path = directory / "proof", stage = directory / "stage";
      bool kept = false;
      {
        Proof p;
        assert(p.open_automatic(path.c_str(), true,
                                caller_stage ? stage.c_str() : nullptr));
        events(p);
        kept = p.automatic_reason() == AutomaticProofReason::DsrSelected &&
               std::string(p.lazy_pbp_report().state) == "fallback";
      }
      // The failed PBP left nothing behind.
      kept = kept && (caller_stage || directory.entries() ==
                                          std::vector<std::string>{"proof"});
      _exit(kept && read_file(path) == want ? 0 : 1);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
}

// No handler runs on SIGKILL, and the destination already holds what was flushed.
void killed_run_leaves_dsr_prefix() {
  const Events events = [](Proof &p) {
    inputs(p);
    p.add_rup(7, lits_of({-3, 4}).data(), 2);
    p.flush();
  };
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  const pid_t child = fork();
  assert(child >= 0);
  if (!child) {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    events(p);
    raise(SIGKILL);
    _exit(1);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL);
  assert(directory.entries() == std::vector<std::string>{"proof"});
  expect_equal(read_file(path), single(ProofFormat::DsrBinary, events),
               "DSR left by a killed run");
}

// A switch replaces the DSR file whole, with a new file of the same mode.
void switch_replaces_the_destination() {
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  FILE *const created = std::fopen(path.c_str(), "wb");
  assert(created && !std::fclose(created));
  assert(!chmod(path.c_str(), 0640));
  struct stat before, after;
  assert(!stat(path.c_str(), &before));
  setenv("TACTIX_AUTO_PROOF_SWITCH_AT", "end", 1);
  const Events events = [](Proof &p) { inputs(p); refutation(p); };
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    events(p);
    assert(p.automatic_reason() == AutomaticProofReason::ForcedSwitch);
    assert(directory.entries() == std::vector<std::string>{"proof"});
    assert(!stat(path.c_str(), &after));
    assert(after.st_ino != before.st_ino);
    assert((after.st_mode & 07777) == 0640);
  }
  unsetenv("TACTIX_AUTO_PROOF_SWITCH_AT");
  expect_complete_pbp(read_file(path), "PBP that replaced the DSR");
}

// With nowhere to put a sibling the DSR stays out of the destination, as a stage
// that can be rewritten over it; if the directory closes later, a conversion
// fails and the DSR stays the proof.
void unwritable_directory() {
  if (!geteuid())
    return; // root ignores the mode
  const Events events = [](Proof &p) { inputs(p); refutation(p); };
  const std::string want = single(ProofFormat::DsrBinary, events);
  {
    TemporaryDirectory directory;
    const std::string path = directory / "proof";
    FILE *const created = std::fopen(path.c_str(), "wb");
    assert(created && !std::fclose(created));
    assert(!chmod(directory.path.c_str(), 0555));
    {
      Proof p;
      assert(p.open_automatic(path.c_str(), true));
      assert(p.automatic_dsr_live());
      events(p);
      assert(p.automatic_reason() == AutomaticProofReason::DsrSelected);
    }
    assert(!chmod(directory.path.c_str(), 0755));
    expect_equal(read_file(path), want, "DSR rewritten from an anonymous stage");
  }
  {
    TemporaryDirectory directory;
    const std::string path = directory / "proof";
    setenv("TACTIX_AUTO_PROOF_SWITCH_AT", "end", 1);
    {
      Proof p;
      assert(p.open_automatic(path.c_str(), true));
      assert(!chmod(directory.path.c_str(), 0555));
      events(p);
      assert(p.automatic_reason() == AutomaticProofReason::DsrSelected);
      assert(std::string(p.lazy_pbp_report().state) == "fallback");
    }
    unsetenv("TACTIX_AUTO_PROOF_SWITCH_AT");
    assert(!chmod(directory.path.c_str(), 0755));
    expect_equal(read_file(path), want, "DSR kept when no sibling can open");
  }
}

// DSR rejects a tautological RAT line: the conversion runs first, and the line
// is written once, on PBP.
void tautological_rat_converts_first() {
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    inputs(p);
    p.add_rup(7, lits_of({-3, 4}).data(), 2);
    const int32_t tautology[] = {7, 1, -1};
    assert(p.add_external_rat(8, tautology, 3));
    assert(p.automatic_reason() == AutomaticProofReason::CopyDeclined);
    assert(std::string(p.lazy_pbp_report().state) == "converted");
    refutation(p);
  }
  const std::string text = read_file(path);
  expect_complete_pbp(text, "tautological RAT");
  assert(count_lines(text, "red ") == 1);
}

// Past the DSR limit a run stays on DSR with the same answers: a parity reason is
// spelled in clauses. Only an event DSR cannot follow still converts.
void dsr_limit_stays_on_dsr() {
  const PbpParityEquation equation{{1, 2}, false};
  const int32_t reason[] = {-1, 2};
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  setenv("TACTIX_AUTO_PROOF_DSR_LIMIT", "0", 1);
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    inputs(p);
    assert(p.commit_clause_stream());
    PbpParityBoundsCache cache;
    PbpParityReasonPlan plan;
    assert(p.plan_pbp_parity_reason({equation}, reason, 2, 2, cache, plan));
    assert(p.add_prevalidated_pbp_parity_reason(7, {equation}, reason, 2, cache, plan));
    assert(p.automatic_reason() == AutomaticProofReason::Pending);
    refutation(p);
    assert(p.automatic_reason() == AutomaticProofReason::DsrSelected);
    assert(std::string(p.verification_route()) != "pbp");
    assert(std::string(p.lazy_pbp_report().state) == "dsr-limit");
    assert(directory.entries() == std::vector<std::string>{"proof"});
  }
  assert(read_file(path).rfind("pseudo-Boolean", 0) != 0);
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    inputs(p);
    const int32_t tautology[] = {7, 1, -1};
    assert(p.add_external_rat(8, tautology, 3));
    assert(p.automatic_reason() == AutomaticProofReason::CopyDeclined);
    refutation(p);
    assert(std::string(p.lazy_pbp_report().state) == "converted");
  }
  expect_complete_pbp(read_file(path), "no clausal spelling past the limit");
  unsetenv("TACTIX_AUTO_PROOF_DSR_LIMIT");
}

// --dsr answers the solver as --pbp does and writes only DSR, to any destination,
// giving parity helpers names no later solver variable takes.
void dsr_only_writes_dsr_with_the_same_answers() {
  const PbpParityEquation equation{{1, 2}, false};
  const int32_t reason[] = {-1, 2};
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  std::vector<Var> externals; // internal i is external i + 1
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), false, nullptr, true));
    assert(!p.automatic());
    inputs(p);
    assert(p.commit_clause_stream());
    PbpParityBoundsCache cache;
    PbpParityReasonPlan plan;
    assert(p.plan_pbp_parity_reason({equation}, reason, 2, 2, cache, plan));
    assert(p.add_prevalidated_pbp_parity_reason(7, {equation}, reason, 2, cache, plan));
    refutation(p);
    assert(std::string(p.verification_route()) == "dsr-cake_lpr");
    assert(directory.entries() == std::vector<std::string>{"proof"});
  }
  const std::string text = read_file(path);
  assert(text.rfind("pseudo-Boolean", 0) != 0);
  assert(text.find("-1 2 0\n") != std::string::npos); // the parity reason
  {
    Proof p;
    assert(p.open_automatic("/dev/null", true, nullptr, true));
    inputs(p);
    refutation(p);
    assert(std::string(p.verification_route()) == "dsr-cake_lpr");
  }
  // Tautological RAT: nothing to spell, and nothing converts.
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true, nullptr, true));
    inputs(p);
    const int32_t tautology[] = {7, 1, -1};
    assert(p.add_external_rat(8, tautology, 3));
    refutation(p);
  }
  assert(read_file(path).rfind("pseudo-Boolean", 0) != 0);
}

// --dsr keeps its input transaction private, as whole-run PBP does, so a startup
// refutation finds its destination untouched: nothing reaches a file, even past
// one write buffer of input normalization, or a pipe before the first clausal
// event. The first clausal event publishes the whole prefix at once, and a
// refutation inside the input transaction publishes it at selection; either
// way the destination receives the bytes of a DSR writer.
void dsr_only_keeps_the_input_private() {
  constexpr uint64_t duplicates = 300000; // 5 bytes each in binary DSR
  const Events normalized = [](Proof &p) {
    const auto duplicate = lits_of({1, 2, 3});
    for (uint64_t id = 1; id <= duplicates; ++id) {
      p.seed_population(id, duplicate.data(), 3);
      if (id > 1)
        p.del(id, id, duplicate.data(), 3);
    }
    p.finish_input_transaction();
  };
  const Events derivation = [](Proof &p) {
    p.add_rup(duplicates + 1, lits_of({1, 2}).data(), 2);
  };
  TemporaryDirectory directory;
  const std::string path = directory / "proof", fifo = directory / "fifo";
  const std::string prefix = single(ProofFormat::DsrBinary, normalized);
  assert(prefix.size() > (1u << 20)); // past one write buffer
  {
    Proof p;
    assert(p.open(path.c_str(), ProofFormat::DsrBinary));
    normalized(p);
    assert(p.startup_pb_available() && read_file(path).empty());
    derivation(p);
    // The commit published the prefix: the open proof's destination holds it.
    expect_equal(read_file(path), prefix, "--dsr prefix at the first clausal event");
  }
  expect_equal(read_file(path),
               single(ProofFormat::DsrBinary,
                      [&](Proof &p) { normalized(p); derivation(p); }),
               "--dsr to a file");
  // Conflicting input units refuted before the transaction ends: no clausal
  // event commits the stream, so the selection publishes the prefix, which a
  // pipe cannot take by rewriting in place.
  const Events conflicting = [](Proof &p) {
    p.seed_population(1, lits_of({1}).data(), 1);
    p.seed_population(2, lits_of({-1}).data(), 1);
  };
  const Events refuted_in_input = [](Proof &p) {
    p.add_empty(3);
    p.finish_input_transaction();
  };
  const struct {
    const char *what;
    Events before, after;
  } piped_runs[] = {
      {"--dsr to a pipe at the first clausal event", inputs, refutation},
      {"--dsr to a pipe at selection", conflicting, refuted_in_input}};
  for (const auto &[what, before, after] : piped_runs) {
    assert(!mkfifo(fifo.c_str(), 0600));
    const int reader = open(fifo.c_str(), O_RDONLY | O_NONBLOCK);
    assert(reader >= 0);
    std::string piped;
    char buffer[4096];
    {
      Proof p;
      assert(p.open(fifo.c_str(), ProofFormat::DsrBinary));
      before(p);
      assert(read(reader, buffer, sizeof buffer) < 0 && errno == EAGAIN);
      after(p);
    }
    for (ssize_t n; (n = read(reader, buffer, sizeof buffer)) > 0;)
      piped.append(buffer, static_cast<size_t>(n));
    close(reader);
    assert(!unlink(fifo.c_str()));
    const std::string want = single(ProofFormat::DsrBinary, [&](Proof &p) {
      before(p);
      after(p);
    });
    assert(!want.empty());
    expect_equal(piped, want, what);
  }
}

// --dsr never converts, so its copy keeps no RAT marks: a validation switch at
// the empty clause finds no stage to read and keeps the DSR, byte for byte.
void dsr_only_switch_at_the_end_keeps_dsr() {
  const Events events = [](Proof &p) {
    inputs(p);
    p.add_rat(20, lits_of({6, -1}).data(), 2);
    const int32_t rat[] = {-6, 2};
    assert(p.add_external_rat(21, rat, 2));
    refutation(p);
  };
  for (const bool binary : {true, false}) {
    TemporaryDirectory directory;
    const std::string path = directory / "proof";
    setenv("TACTIX_AUTO_PROOF_SWITCH_AT", "end", 1);
    {
      Proof p;
      assert(p.open_automatic(path.c_str(), binary, nullptr, true));
      events(p);
      assert(std::string(p.lazy_pbp_report().state) == "fallback");
      assert(std::string(p.verification_route()) == "dsr-cake_lpr");
    }
    unsetenv("TACTIX_AUTO_PROOF_SWITCH_AT");
    assert(directory.entries() == std::vector<std::string>{"proof"});
    expect_equal(read_file(path),
                 single(binary ? ProofFormat::DsrBinary : ProofFormat::DsrAscii,
                        events),
                 "--dsr past a switch at the end");
  }
}

// The DSR limit counts binary DSR bytes in either spelling, names of one to
// five varint bytes included: a parity reason once the DSR has reached the
// limit stays on DSR, and one a byte below it converts.
void dsr_limit_counts_binary_bytes() {
  const auto lines = [](Proof &p) {
    inputs(p);
    const int32_t wide[][2] = {{1, 70}, {-2, 9000}, {3, -1100000}, {-4, 300000000}};
    for (uint64_t i = 0; i < 4; ++i)
      assert(p.add_external_rup(20 + i, wide[i], 2));
  };
  const uint64_t bytes = single(ProofFormat::DsrBinary, lines).size();
  setenv("TACTIX_AUTO_PROOF_PARITY_BYTES", "0", 1);
  for (const bool binary : {true, false})
    for (const uint64_t limit : {bytes, bytes + 1}) {
      setenv("TACTIX_AUTO_PROOF_DSR_LIMIT", std::to_string(limit).c_str(), 1);
      TemporaryDirectory directory;
      const std::string path = directory / "proof";
      Proof p;
      assert(p.open_automatic(path.c_str(), binary));
      lines(p);
      parity_reason(p);
      assert(p.automatic_reason() == (limit == bytes
                                          ? AutomaticProofReason::Pending
                                          : AutomaticProofReason::ParityReason));
    }
  unsetenv("TACTIX_AUTO_PROOF_DSR_LIMIT");
  unsetenv("TACTIX_AUTO_PROOF_PARITY_BYTES");
}

// A deletion block spilled behind a pending root unit is copied into the stage
// when the batch flushes; a failure there is a failure of the only proof.
void failed_spill_aborts() {
  if (access("/proc/self/fd", R_OK) || access("/dev/full", W_OK))
    return;
  expect_abort([] {
    TemporaryDirectory directory;
    const std::string path = directory / "proof", stage = directory / "stage";
    Proof p;
    assert(p.open_automatic(path.c_str(), true, stage.c_str()));
    assert(fail_writes_to(stage));
    inputs(p);
    p.add_rup(7, lits_of({3}).data(), 1); // a pending DSR unit
    for (int i = 0; i < 14000; ++i) {                // > 64 KiB of deletions
      const int a = 1 + i % 29, b = 1 + (i / 29) % 29;
      if (a != b)
        p.del(8 + i, 100000 + i, lits_of({a, -b, 30}).data(), 3);
    }
    p.add_rup(30000, lits_of({1, 2, 4}).data(), 3); // flushes it
    refutation(p);
    p.flush();
  });
}

// A caller stage must be a regular file other than the destination: a device
// would read back nothing (or forever) when the copy is installed.
void unusable_stage_is_pbp_only() {
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  for (const std::string &stage : {std::string("/dev/null"), path}) {
    Proof p;
    assert(p.open_automatic(path.c_str(), true, stage.c_str()));
    assert(p.automatic_reason() == AutomaticProofReason::StageUnavailable);
    assert(!p.automatic_dsr_live());
    inputs(p);
    refutation(p);
    assert(std::string(p.verification_route()) == "pbp");
  }
}

// Without the empty clause a run never materializes PBP: it leaves the DSR
// copy's prefix, and neither the stage nor the journal remains. The prefix is
// placed like a complete copy: by rename, by rewriting a destination that
// cannot be renamed over (a second hard link), or from a caller's stage.
void unfinished_run_leaves_dsr_prefix() {
  const Events events = [](Proof &p) {
    inputs(p);
    p.add_rup(7, lits_of({-3, 4}).data(), 2);
    p.flush();
  };
  enum Placement { Rename, Rewrite, CallerStage };
  for (const bool binary : {true, false})
    for (const Placement placement : {Rename, Rewrite, CallerStage}) {
      TemporaryDirectory directory;
      const std::string path = directory / "proof", stage = directory / "stage",
                        link = directory / "link";
      if (placement == Rewrite) {
        FILE *const created = std::fopen(path.c_str(), "wb");
        assert(created && !std::fclose(created));
        assert(!::link(path.c_str(), link.c_str()));
      }
      {
        Proof p;
        assert(p.open_automatic(path.c_str(), binary,
                                placement == CallerStage ? stage.c_str()
                                                         : nullptr));
        events(p);
        assert(p.automatic_reason() == AutomaticProofReason::Pending);
        assert(p.automatic_dsr_live());
        assert(std::string(p.lazy_pbp_report().state) == "dsr");
        // No PBP byte has reached the disk while the run is lazy; the DSR is
        // there already unless a stage holds it.
        expect_equal(
            read_file(path),
            placement == Rename
                ? single(binary ? ProofFormat::DsrBinary : ProofFormat::DsrAscii,
                         events)
                : std::string(),
            "destination during the run");
      }
      std::vector<std::string> expected = {"proof"};
      if (placement == Rewrite)
        expected = {"link", "proof"};
      if (placement == CallerStage) {
        expected = {"proof", "stage"};
        expect_equal(read_file(stage), "", "emptied caller stage");
      }
      std::vector<std::string> names = directory.entries();
      std::sort(names.begin(), names.end());
      assert(names == expected);
      expect_equal(read_file(path),
                   single(binary ? ProofFormat::DsrBinary : ProofFormat::DsrAscii,
                          events),
                   "unfinished DSR prefix");
    }
}

// A stage that fails during the final flush of an unfinished run loses the
// prefix: no abort (the verdict may be SAT), no PBP, an empty destination.
void unfinished_run_with_a_failed_stage_stays_empty() {
  if (access("/proc/self/fd", R_OK) || access("/dev/full", W_OK))
    return; // no fault injection on this system
  TemporaryDirectory directory;
  const std::string path = directory / "proof", stage = directory / "stage";
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true, stage.c_str()));
    inputs(p);
    p.add_rup(7, lits_of({-3, 4}).data(), 2);
    assert(fail_writes_to(stage));
  }
  expect_equal(read_file(path), "", "unfinished run after a stage failure");
}

// Non-regular destinations cannot be rewritten: whole-run PBP, no copy.
void device_destination_is_pbp_only() {
  Proof p;
  assert(p.open_automatic("/dev/null", true));
  assert(p.automatic_reason() == AutomaticProofReason::DestinationUnsupported);
  assert(!p.automatic_dsr_live());
  inputs(p);
  refutation(p);
  assert(std::string(p.verification_route()) == "pbp");
}

} // namespace


// A seeded script of input clauses, RUP units and clauses, RAT clauses and
// deletions.
void scripted_run(Proof &p, int total) {
  uint64_t state = 0x9e3779b97f4a7c15ull;
  const auto next = [&state](uint64_t bound) {
    state = state * 6364136223846793005ull + 1442695040888963407ull;
    return (state >> 33) % bound;
  };
  constexpr int vars = 40;
  const auto literal = [&] {
    const int v = 1 + static_cast<int>(next(vars));
    return next(2) ? v : -v;
  };
  std::vector<std::vector<int>> live;
  uint64_t id = 1;
  for (int i = 0; i < 60; ++i) {
    std::vector<int> c{literal(), literal(), literal()};
    if (c[0] == c[1] || c[0] == -c[1] || c[1] == c[2] || c[1] == -c[2] ||
        c[0] == c[2] || c[0] == -c[2])
      continue;
    const auto lits = lits_of({c[0], c[1], c[2]});
    p.seed_population(id++, lits.data(), 3);
  }
  p.finish_input_transaction();
  for (int i = 0; i < total; ++i) {
    const uint64_t kind = next(100);
    if (kind < 15) { // a RUP unit
      const int u = literal();
      p.add_rup(id++, lits_of({u}).data(), 1);
    } else if (kind < 60) { // a RUP clause
      const int a = literal(), b = literal();
      if (a == b || a == -b)
        continue;
      p.add_rup(id++, lits_of({a, b}).data(), 2);
      live.push_back({a, b});
    } else if (kind < 70) { // a RAT clause on a fresh variable
      const int fresh = vars + 1 + i, b = literal();
      p.add_rat(id++, lits_of({fresh, b}).data(), 2);
    } else if (!live.empty()) { // a deletion
      const size_t at = next(live.size());
      const std::vector<int> c = live[at];
      live.erase(live.begin() + static_cast<long>(at));
      p.del(id++, 0, lits_of({c[0], c[1]}).data(), 2);
    }
  }
  p.add_empty(id);
}

// RUP, RAT and external lines well past the copy's 1 MiB binary buffer, and
// pending root units with deletion blocks that spill (> 64 KiB). Returns how
// many lines are RAT.
size_t long_script(Proof &p) {
  uint64_t state = 0x2545f4914f6cdd1dull;
  const auto next = [&state](uint64_t bound) {
    state = state * 6364136223846793005ull + 1442695040888963407ull;
    return (state >> 33) % bound;
  };
  inputs(p);
  uint64_t id = 7;
  size_t rats = 0;
  for (int round = 0; round < 3; ++round) {
    for (int i = 0; i < 40000; ++i) {
      std::vector<int> c;
      for (int v = 1 + static_cast<int>(next(40)); c.size() < 8;
           v += 1 + static_cast<int>(next(60)))
        c.push_back(next(2) ? v : -v);
      std::vector<Lit> lits;
      for (const int e : c)
        lits.push_back(internalize(e));
      const uint64_t kind = next(10);
      if (kind < 7) {
        p.add_rup(id++, lits.data(), 8);
      } else if (kind < 8) {
        lits[0] = internalize(100000 + round * 40000 + i); // a fresh pivot
        p.add_rat(id++, lits.data(), 8);
        ++rats;
      } else if (kind < 9) {
        c[0] = 100000 + round * 40000 + i;
        assert(p.add_external_rat(id++, c.data(), 8));
        ++rats;
      } else {
        assert(p.add_external_rup(id++, c.data(), 8));
      }
    }
    p.add_rup(id++, lits_of({1 + round}).data(), 1); // pending unit
    for (int i = 0; i < 14000; ++i) { // a spilled deletion block
      const int a = 1 + i % 29, b = 1 + (i / 29) % 29;
      if (a != b)
        p.del(id++, 100000 + i, lits_of({a, -b, 30}).data(), 3);
    }
  }
  refutation(p);
  return rats;
}

// The marks stay aligned across the buffer, spills and every addition path.
void long_conversion_marks_every_rat_line() {
  setenv("TACTIX_AUTO_PROOF_SWITCH_AT", "end", 1);
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  size_t rats = 0;
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    rats = long_script(p);
    assert(p.automatic_reason() == AutomaticProofReason::ForcedSwitch);
    assert(std::string(p.lazy_pbp_report().state) == "converted");
  }
  unsetenv("TACTIX_AUTO_PROOF_SWITCH_AT");
  const std::string text = read_file(path);
  expect_complete_pbp(text, "long conversion");
  assert(count_lines(text, "red ") == rats);
}

// A switch at any point of a run yields one complete proof; nothing is lost
// around the switch.
void conversion_at_every_point() {
  static constexpr int total = 400;
  size_t additions = 0;
  {
    TemporaryDirectory directory;
    const std::string path = directory / "proof";
    setenv("TACTIX_AUTO_PROOF_SWITCH_AT", "end", 1);
    {
      Proof p;
      assert(p.open_automatic(path.c_str(), true));
      scripted_run(p, total);
    }
    const std::string text = read_file(path);
    expect_complete_pbp(text, "conversion at the end");
    additions = count_lines(text, "rup ") + count_lines(text, "red ");
  }
  assert(additions > 100);
  unsetenv("TACTIX_AUTO_PROOF_SWITCH_AT");
  const std::string pbp = single(
      ProofFormat::PbpStreaming, [](Proof &p) { scripted_run(p, total); });
  const size_t pbp_lines = count_lines(pbp, "rup ") + count_lines(pbp, "red ");
  for (size_t at = 1; at < additions; at += 17) {
    TemporaryDirectory directory;
    const std::string path = directory / "proof";
    setenv("TACTIX_AUTO_PROOF_SWITCH_AT", std::to_string(at).c_str(), 1);
    {
      Proof p;
      assert(p.open_automatic(path.c_str(), true));
      scripted_run(p, total);
      assert(p.automatic_reason() == AutomaticProofReason::ForcedSwitch);
      assert(std::string(p.lazy_pbp_report().state) == "converted");
    }
    const std::string text = read_file(path);
    expect_complete_pbp(text, "conversion mid-run");
    // DSR spells some units with two helper lines, so a converted prefix can
    // only add lines to --pbp's: no addition is lost.
    assert(count_lines(text, "rup ") + count_lines(text, "red ") >= pbp_lines);
  }
  unsetenv("TACTIX_AUTO_PROOF_SWITCH_AT");
}

int main() {
  // Failure tests deliberately abort or _exit in children. Their local
  // destructors cannot run; the parent owns their entire temporary tree.
  TemporaryDirectory temporary;
  assert(!setenv("TMPDIR",temporary.path.c_str(),1));
  no_trigger_selects_dsr();
  startup_pb_keeps_pbp();
  sr_under_affine_support();
  sr_beyond_the_auxiliary_limit();
  sr_auxiliary_name_counts_helpers();
  dsr_only_stops_at_the_name_bound();
  rat_line_deletes_retained_support();
  witnessed_sr_selects_trestle_route();
  symlink_destination_is_copied();
  caller_stage_is_copied();
  failed_copy_aborts();
  failed_spill_aborts();
  failed_stage_at_the_switch_aborts();
  failed_conversion_keeps_dsr();
  tautological_rat_converts_first();
  dsr_limit_stays_on_dsr();
  dsr_only_writes_dsr_with_the_same_answers();
  dsr_only_keeps_the_input_private();
  dsr_only_switch_at_the_end_keeps_dsr();
  dsr_limit_counts_binary_bytes();
  conversion_at_every_point();
  long_conversion_marks_every_rat_line();
  unusable_stage_is_pbp_only();
  unfinished_run_leaves_dsr_prefix();
  killed_run_leaves_dsr_prefix();
  switch_replaces_the_destination();
  unwritable_directory();
  unfinished_run_with_a_failed_stage_stays_empty();
  device_destination_is_pbp_only();
  std::puts("automatic proof: DSR, converted once to PBP at a startup refutation, "
            "a parity limit or an event DSR cannot follow");
  return 0;
}
