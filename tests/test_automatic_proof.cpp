// Automatic proof selection (plan §5): the PBP stream owns every decision and
// a native-DSR copy receives the same events. The empty clause keeps the copy
// unless a PB-native step (or an event the copy cannot follow) discarded it.
// Every case compares the destination with a single-format writer fed the
// same events, so selection changes which bytes survive, never the bytes.
#undef NDEBUG
#include "proof/proof.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <functional>
#include <string>
#include <sys/stat.h>
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
  p.add_rup(7, l1.data(), 2, nullptr, 0);
  p.del(8, 4, clause({-3, 4, 5}).data(), 3);
  p.add_rup(9, l2.data(), 1, nullptr, 0);
  p.add_derived_unit(10, internalize(-3), nullptr, 0);
  p.add_rup(11, clause({2}).data(), 1, nullptr, 0);
  p.add_empty(12, nullptr, 0);
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
      assert(directory.entries().size() == 2); // destination + sibling stage
      events(p);
      assert(p.automatic_reason() == AutomaticProofReason::DsrSelected);
      assert(!p.automatic_dsr_live());
      route = p.verification_route();
      assert(directory.entries() == std::vector<std::string>{"proof"});
    }
    assert(route == "dsr-cake_lpr");
    expect_equal(read_all(path),
                 single(binary ? ProofFormat::DsrBinary : ProofFormat::DsrAscii,
                        events),
                 binary ? "binary DSR selected" : "ASCII DSR selected");
    // The renamed stage keeps the mode an ordinary proof file would have.
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
  expect_equal(read_all(path), single(ProofFormat::PbpStreaming, events),
               "PBP after SR with retained support");
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
void failed_copy_keeps_pbp() {
  if (access("/dev/full", W_OK))
    return;
  const Events events = [](Proof &p) { inputs(p); refutation(p); };
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true, "/dev/full"));
    events(p);
    assert(p.automatic_reason() == AutomaticProofReason::CopyWriteFailed);
    assert(std::string(p.verification_route()) == "pbp");
  }
  expect_equal(read_all(path), single(ProofFormat::PbpStreaming, events),
               "PBP after a failed copy");
}

// Without the empty clause the destination keeps its PBP prefix.
void unfinished_run_keeps_pbp() {
  const Events events = [](Proof &p) {
    inputs(p);
    p.add_rup(7, clause({-3, 4}).data(), 2, nullptr, 0);
    p.flush();
  };
  TemporaryDirectory directory;
  const std::string path = directory / "proof";
  {
    Proof p;
    assert(p.open_automatic(path.c_str(), true));
    events(p);
    assert(p.automatic_reason() == AutomaticProofReason::Pending);
    assert(p.automatic_dsr_live());
  }
  assert(directory.entries() == std::vector<std::string>{"proof"});
  expect_equal(read_all(path), single(ProofFormat::PbpStreaming, events),
               "unfinished PBP prefix");
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

int main() {
  no_trigger_selects_dsr();
  startup_pb_keeps_pbp();
  sr_with_support_keeps_pbp();
  witnessed_sr_selects_trestle_route();
  symlink_destination_is_copied();
  caller_stage_is_copied();
  failed_copy_keeps_pbp();
  unfinished_run_keeps_pbp();
  device_destination_is_pbp_only();
  std::puts("automatic proof selection: DSR kept unless a PB-native step or "
            "an unfollowable event discarded it; bytes equal single-format "
            "writers");
  return 0;
}
