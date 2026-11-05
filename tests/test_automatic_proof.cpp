// Automatic proof (plan §5): one proof at a time. The kernel sees --pbp, only
// the DSR copy is written, and a trigger converts it to PBP and continues on
// PBP. Runs that keep DSR compare with a single-format writer byte for byte; a
// conversion compares with its expected lines here, and tests/cli_auto_proof.sh
// and test_dsr_to_pbp check the semantics through the real checkers.
#undef NDEBUG
#include "proof/proof.hpp"

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <functional>
#include <string>
#include <csignal>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace tactix;

namespace {

std::string read_all(const std::string &path) {
  FILE *f = std::fopen(path.c_str(), "rb");
  assert(f);
  std::string out;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
    out.append(buf, n);
  std::fclose(f);
  return out;
}

void expect_equal(const std::string &got, const std::string &want,
                  const char *what) {
  if (got == want)
    return;
  std::fprintf(stderr, "FAIL %s: %zu bytes, want %zu\n", what, got.size(),
               want.size());
  std::abort();
}

std::vector<Lit> clause(std::initializer_list<int> external) {
  std::vector<Lit> lits;
  for (const int literal : external)
    lits.push_back(internalize(literal));
  return lits;
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
    for (const auto &name : entries())
      std::remove((path + "/" + name).c_str());
    rmdir(path.c_str());
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
  const auto a = clause({1, 2, 3}), b = clause({-1, 2}), c = clause({-2, 3}),
             d = clause({-3, 4, 5}), e = clause({-3, -4}), f = clause({-3, -5});
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
  const auto l1 = clause({-3, 4}), l2 = clause({-3}), u = clause({3});
  p.add_rup(7, l1.data(), 2);
  p.del(8, 4, clause({-3, 4, 5}).data(), 3);
  p.add_rup(9, l2.data(), 1);
  p.add_derived_unit(10, internalize(-3));
  p.add_rup(11, clause({2}).data(), 1);
  p.add_empty(12);
  (void)u;
};

std::string single(ProofFormat format, const Events &events) {
  TemporaryDirectory directory;
  const std::string path = directory / "single";
  {
    Proof p;
    assert(p.open(path.c_str(), format));
    events(p);
  }
  return read_all(path);
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
      const AutomaticCheckEstimate *e = p.automatic_estimate();
      assert(e && e->census.rat_lines == 0 && !e->pbp_faster());
      route = p.verification_route();
      assert(directory.entries() == std::vector<std::string>{"proof"});
    }
    assert(route == "dsr-cake_lpr");
    expect_equal(read_all(path),
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
  expect_equal(read_all(path), single(ProofFormat::PbpStreaming, events),
               "terminal PB");
}

void sr_with_support_keeps_pbp() {
  const Events events = [](Proof &p) {
    inputs(p);
    const int32_t support[] = {1, 2};
    assert(p.retain_external(20, support, 2));
    const auto sr = clause({6, 1});
    const DsrLiteralMapping mapping[] = {{internalize(6), internalize(-6)}};
    assert(p.add_sr(21, sr.data(), 2, nullptr, 0, mapping, 1, 7));
    refutation(p);
  };
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    events(p);
    assert(p.automatic_reason() ==
           AutomaticProofReason::SrWithAffineSupport);
    assert(std::string(p.verification_route()) == "pbp");
    assert(directory.entries() == std::vector<std::string>{"proof"});
  }
  // The SR event was accepted before the copy declined it: it follows the
  // (here empty) converted prefix, once, and PBP continues.
  const std::string text = read_all(path);
  expect_complete_pbp(text, "PBP after SR with retained support");
  assert(count_lines(text, "red ") == 1);
}

void witnessed_sr_selects_trestle_route() {
  const Events events = [](Proof &p) {
    inputs(p);
    const auto sr = clause({6, 1});
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
  expect_equal(read_all(path), single(ProofFormat::DsrBinary, events),
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
  expect_equal(read_all(target), single(ProofFormat::DsrBinary, events),
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
  expect_equal(read_all(path), single(ProofFormat::DsrBinary, events),
               "DSR copied from a caller stage");
  expect_equal(read_all(stage), "", "emptied caller stage");
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
      _exit(kept && read_all(path) == want ? 0 : 1);
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
    p.add_rup(7, clause({-3, 4}).data(), 2);
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
  expect_equal(read_all(path), single(ProofFormat::DsrBinary, events),
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
  expect_complete_pbp(read_all(path), "PBP that replaced the DSR");
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
    expect_equal(read_all(path), want, "DSR rewritten from an anonymous stage");
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
    expect_equal(read_all(path), want, "DSR kept when no sibling can open");
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
    p.add_rup(7, clause({-3, 4}).data(), 2);
    const int32_t tautology[] = {7, 1, -1};
    assert(p.add_external_rat(8, tautology, 3));
    assert(p.automatic_reason() == AutomaticProofReason::CopyDeclined);
    assert(std::string(p.lazy_pbp_report().state) == "converted");
    refutation(p);
  }
  const std::string text = read_all(path);
  expect_complete_pbp(text, "tautological RAT");
  assert(count_lines(text, "red ") == 1);
}

// Past the DSR limit the run stays on DSR wherever it has a choice: a witnessed
// step the copy cannot follow, a compact parity reason and a terminal PB
// refutation all decline. An event with no clausal spelling still converts.
void dsr_limit_stays_on_dsr() {
  const PbpParityEquation equation{{1, 2}, false};
  const int32_t reason[] = {-1, 2};
  const auto sr = clause({6, 1});
  const DsrLiteralMapping mapping[] = {{internalize(6), internalize(-6)}};
  const int32_t support[] = {1, 2};
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  setenv("TACTIX_AUTO_PROOF_DSR_LIMIT", "0", 1);
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    inputs(p);
    assert(p.retain_external(20, support, 2));
    assert(!p.add_sr(21, sr.data(), 2, nullptr, 0, mapping, 1, 7));
    assert(p.automatic_reason() == AutomaticProofReason::Pending);
    assert(p.commit_clause_stream());
    PbpParityBoundsCache cache;
    PbpParityReasonPlan plan;
    assert(!p.plan_pbp_parity_reason({equation}, reason, 2, 2, cache, plan));
    assert(!p.begin_pb());
    assert(p.automatic_reason() == AutomaticProofReason::Pending);
    refutation(p);
    assert(p.automatic_reason() == AutomaticProofReason::DsrSelected);
    assert(std::string(p.verification_route()) != "pbp");
    assert(std::string(p.lazy_pbp_report().state) == "dsr-limit");
    assert(directory.entries() == std::vector<std::string>{"proof"});
  }
  assert(read_all(path).rfind("pseudo-Boolean", 0) != 0);
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
  expect_complete_pbp(read_all(path), "no clausal spelling past the limit");
  // Under the limit the same plan is available.
  setenv("TACTIX_AUTO_PROOF_DSR_LIMIT", "1000000", 1);
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    inputs(p);
    assert(p.commit_clause_stream());
    PbpParityBoundsCache cache;
    PbpParityReasonPlan plan;
    assert(p.plan_pbp_parity_reason({equation}, reason, 2, 2, cache, plan));
    refutation(p);
    assert(std::string(p.lazy_pbp_report().state) == "dsr-kept");
  }
  unsetenv("TACTIX_AUTO_PROOF_DSR_LIMIT");
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
    p.add_rup(7, clause({3}).data(), 1); // a pending DSR unit
    for (int i = 0; i < 14000; ++i) {                // > 64 KiB of deletions
      const int a = 1 + i % 29, b = 1 + (i / 29) % 29;
      if (a != b)
        p.del(8 + i, 100000 + i, clause({a, -b, 30}).data(), 3);
    }
    p.add_rup(30000, clause({1, 2, 4}).data(), 3); // flushes it
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
    p.add_rup(7, clause({-3, 4}).data(), 2);
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
            read_all(path),
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
        expect_equal(read_all(stage), "", "emptied caller stage");
      }
      std::vector<std::string> names = directory.entries();
      std::sort(names.begin(), names.end());
      assert(names == expected);
      expect_equal(read_all(path),
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
    p.add_rup(7, clause({-3, 4}).data(), 2);
    assert(fail_writes_to(stage));
  }
  expect_equal(read_all(path), "", "unfinished run after a stage failure");
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

// The census charges each non-RUP line against the database it is added to:
// non-unit clauses for drat-trim's watches, every clause for Trestle, and
// slots that stay allocated after deletion. Unit deletions are ignored.
void census_counts_the_checker_database() {
  DsrCheckCensus c;
  c.line(false, 3);
  c.line(false, 2);
  c.line(false, 1);
  c.non_rup_line(false);
  assert(c.rat_lines == 1 && c.sr_lines == 0);
  assert(c.watch_scan == 4 && c.watched_literal_scan == 5);
  assert(c.live_scan == 3 && c.live_literal_scan == 6);
  assert(c.deleted_slot_scan == 0);
  c.line(true, 2);
  c.line(true, 1); // ignored, as both DSR elaborators ignore unit deletions
  c.line(false, 0); // the empty clause
  c.non_rup_line(true);
  assert(c.sr_lines == 1 && c.committed == 3 && c.live == 2);
  assert(c.watch_scan == 4 + 2 && c.watched_literal_scan == 5 + 3);
  assert(c.deleted_slot_scan == 1 && c.live_scan == 3 + 2);
}

// Factor-shaped RAT lines over a large database: the reference DSR checker
// would scan the whole formula for each, so the empty clause keeps the
// complete PBP stream, byte for byte as --pbp wrote it.
void rat_scan_keeps_pbp() {
  constexpr int inputs = 100000, rats = 1000;
  const Events events = [](Proof &p) {
    for (int i = 0; i < inputs; ++i) {
      const auto c = clause({i + 1, -(i + 2)});
      p.seed_population(i + 1, c.data(), 2);
    }
    p.finish_input_transaction();
    for (int k = 0; k < rats; ++k) {
      const int fresh = inputs + 2 + 2 * k;
      const auto r = clause({fresh, fresh + 1});
      p.add_rat(inputs + 1 + k, r.data(), 2);
    }
    p.add_empty(inputs + rats + 1);
  };
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  std::string route;
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    events(p);
    assert(p.automatic_reason() == AutomaticProofReason::PbpPredictedFaster);
    const AutomaticCheckEstimate *e = p.automatic_estimate();
    assert(e && !e->witnessed && e->census.rat_lines == rats);
    assert(e->pbp_faster() && e->dsr_scan_seconds > e->pbp_seconds);
    route = p.verification_route();
    assert(directory.entries() == std::vector<std::string>{"proof"});
  }
  assert(route == "pbp");
  // Past the DSR limit the same run keeps DSR.
  setenv("TACTIX_AUTO_PROOF_DSR_LIMIT", "0", 1);
  {
    TemporaryDirectory limited;
    const std::string kept = limited / "proof";
    Proof p;
    assert(p.open_automatic(kept.c_str(), true));
    events(p);
    assert(p.automatic_reason() == AutomaticProofReason::DsrSelected);
    assert(p.automatic_estimate() && p.automatic_estimate()->pbp_faster());
    assert(std::string(p.lazy_pbp_report().state) == "dsr-limit");
  }
  unsetenv("TACTIX_AUTO_PROOF_DSR_LIMIT");
  // Only RAT lines and the empty clause: the conversion spells --pbp's lines.
  expect_equal(read_all(path), single(ProofFormat::PbpStreaming, events),
               "converted RAT lines equal --pbp");
}

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
    const auto lits = clause({c[0], c[1], c[2]});
    p.seed_population(id++, lits.data(), 3);
  }
  p.finish_input_transaction();
  for (int i = 0; i < total; ++i) {
    const uint64_t kind = next(100);
    if (kind < 15) { // a RUP unit
      const int u = literal();
      p.add_rup(id++, clause({u}).data(), 1);
    } else if (kind < 60) { // a RUP clause
      const int a = literal(), b = literal();
      if (a == b || a == -b)
        continue;
      p.add_rup(id++, clause({a, b}).data(), 2);
      live.push_back({a, b});
    } else if (kind < 70) { // a RAT clause on a fresh variable
      const int fresh = vars + 1 + i, b = literal();
      p.add_rat(id++, clause({fresh, b}).data(), 2);
    } else if (!live.empty()) { // a deletion
      const size_t at = next(live.size());
      const std::vector<int> c = live[at];
      live.erase(live.begin() + static_cast<long>(at));
      p.del(id++, 0, clause({c[0], c[1]}).data(), 2);
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
    p.add_rup(id++, clause({1 + round}).data(), 1); // pending unit
    for (int i = 0; i < 14000; ++i) { // a spilled deletion block
      const int a = 1 + i % 29, b = 1 + (i / 29) % 29;
      if (a != b)
        p.del(id++, 100000 + i, clause({a, -b, 30}).data(), 3);
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
  const std::string text = read_all(path);
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
    const std::string text = read_all(path);
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
    const std::string text = read_all(path);
    expect_complete_pbp(text, "conversion mid-run");
    // DSR spells some units with two helper lines, so a converted prefix can
    // only add lines to --pbp's: no addition is lost.
    assert(count_lines(text, "rup ") + count_lines(text, "red ") >= pbp_lines);
  }
  unsetenv("TACTIX_AUTO_PROOF_SWITCH_AT");
}

int main() {
  census_counts_the_checker_database();
  no_trigger_selects_dsr();
  rat_scan_keeps_pbp();
  startup_pb_keeps_pbp();
  sr_with_support_keeps_pbp();
  witnessed_sr_selects_trestle_route();
  symlink_destination_is_copied();
  caller_stage_is_copied();
  failed_copy_aborts();
  failed_spill_aborts();
  failed_stage_at_the_switch_aborts();
  failed_conversion_keeps_dsr();
  tautological_rat_converts_first();
  dsr_limit_stays_on_dsr();
  conversion_at_every_point();
  long_conversion_marks_every_rat_line();
  unusable_stage_is_pbp_only();
  unfinished_run_leaves_dsr_prefix();
  killed_run_leaves_dsr_prefix();
  switch_replaces_the_destination();
  unwritable_directory();
  unfinished_run_with_a_failed_stage_stays_empty();
  device_destination_is_pbp_only();
  std::puts("automatic proof: one proof at a time; DSR kept unless a PB-native "
            "step, an unfollowable event, or a faster predicted PBP check "
            "converted it to PBP");
  return 0;
}
