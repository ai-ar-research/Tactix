#undef NDEBUG
#include "strategy/feedback_provenance.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <unistd.h>

using namespace tactix::strategy::controller;

int main() {
  assert(feedback_sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  assert(feedback_sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  const std::string million(1000000, 'a');
  assert(feedback_sha256(million) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  FeedbackSha256 fragmented;
  for (char byte : million) fragmented.append(&byte, 1);
  assert(fragmented.finish() == feedback_sha256(million));
  assert(fragmented.finish() == feedback_sha256(million)); // Finishing is pure.
  std::string bytes;
  for (unsigned i = 0; i < 17000; ++i) bytes.push_back(static_cast<char>(i));
  char temporary[] = "/tmp/tactix-feedback-provenance-XXXXXX";
  const int fd = mkstemp(temporary);
  assert(fd >= 0 && close(fd) == 0);
  { std::ofstream output(temporary, std::ios::binary); output << bytes; assert(output.good()); }
  std::string file_digest;
  assert(feedback_file_sha256(temporary, file_digest) && file_digest == feedback_sha256(bytes));
  assert(unlink(temporary) == 0);
  assert(!feedback_file_sha256(temporary, file_digest));
  assert(!feedback_file_sha256("/tmp", file_digest));

  // Keep known environment provenance complete when configuration knobs are
  // added. Dynamic per-rule cursor/order keys are handled by the frontend.
  std::set<std::string> declared;
#define TACTIX_FEEDBACK_ENVIRONMENT(name) declared.insert(#name);
#include "strategy/feedback_environment.inc"
#undef TACTIX_FEEDBACK_ENVIRONMENT
  // The source audit includes every declared name. Runtime provenance must
  // omit event/journal settings when their implementations are compiled out.
  std::set<std::string> runtime_declared;
#define TACTIX_FEEDBACK_RUNTIME_ENVIRONMENT
#define TACTIX_FEEDBACK_ENVIRONMENT(name) runtime_declared.insert(#name);
#include "strategy/feedback_environment.inc"
#undef TACTIX_FEEDBACK_ENVIRONMENT
#undef TACTIX_FEEDBACK_RUNTIME_ENVIRONMENT
  for (const auto &name : declared) {
    bool expected = true;
#ifndef TACTIX_ENABLE_EVENT_TRACE
    if (name.find("TACTIX_EVENT_TRACE") == 0 || name.find("SAT_EVENT_TRACE") == 0)
      expected = false;
#endif
#ifndef TACTIX_ENABLE_RULE_JOURNAL
    if (name.find("TACTIX_RULE_JOURNAL") == 0 || name.find("SAT_RULE_JOURNAL") == 0)
      expected = false;
#endif
    assert(bool(runtime_declared.count(name)) == expected);
  }
  const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path();
  const std::regex pattern("\"((?:TACTIX|SAT)_[A-Z0-9_]+)\"");
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root / "src")) {
    if (entry.path().extension() != ".cpp" && entry.path().extension() != ".hpp") continue;
    FILE *input = std::fopen(entry.path().c_str(), "rb");
    assert(input);
    std::string source;
    char buffer[4096];
    size_t size;
    while ((size = std::fread(buffer, 1, sizeof buffer, input)))
      source.append(buffer, size);
    assert(!std::ferror(input));
    assert(!std::fclose(input));
    for (std::sregex_iterator match(source.begin(), source.end(), pattern), end; match != end; ++match)
      assert(declared.count((*match)[1].str()));
  }
}
