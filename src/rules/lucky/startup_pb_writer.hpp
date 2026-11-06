#pragma once

#include "kernel/kernel.hpp"
#include "kernel/types.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace tactix {

// Text and constraint numbering shared by the startup pseudo-Boolean
// certificates. A null stream is the count-only preflight: constraint IDs
// advance exactly as when writing, but no byte is produced.
class StartupPbWriter {
protected:
  FILE *out_;
  uint64_t initial_, id_;

  StartupPbWriter(const Kernel &k, FILE *out)
      : out_(out),
        initial_(k.input_unit_clause_count + k.input_nonunit_clause_count),
        id_(initial_) {}

  template <class... Args> void text(const char *format, Args... args) {
    if (!out_)
      return;
    if constexpr (sizeof...(Args) == 0)
      std::fputs(format, out_);
    else
      std::fprintf(out_, format, args...);
  }
  void term(unsigned coefficient, int32_t literal) {
    text("%u %sx%u ", coefficient, literal < 0 ? "~" : "",
         external_variable_of(literal));
  }
  void term(int32_t literal) { term(1, literal); }
  // pol ids[0] [scale *] ids[1] + ... [divide d] ;
  uint64_t sum(const std::vector<uint64_t> &ids, uint64_t scale = 1,
               uint64_t divide = 1) {
    text("pol %llu", (unsigned long long)ids[0]);
    if (scale != 1)
      text(" %llu *", (unsigned long long)scale);
    for (size_t at = 1; at < ids.size(); ++at)
      text(" %llu +", (unsigned long long)ids[at]);
    if (divide != 1)
      text(" %llu d", (unsigned long long)divide);
    text(" ;\n");
    return ++id_;
  }
  void header() { text("pseudo-Boolean proof version 3.0\n"); }
  void footer() {
    text("output NONE ;\nconclusion UNSAT : -1 ;\nend pseudo-Boolean proof ;\n");
  }

public:
  uint64_t additions() const { return id_ - initial_; }
};

} // namespace tactix
