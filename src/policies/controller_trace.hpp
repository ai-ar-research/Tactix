#pragma once

#include <cstdint>
#include <fstream>
#include <string>

namespace tactix {

// Versioned deterministic controller transcript. Verification regenerates the
// seeded choices and checks every typed request, search service and outcome;
// wall-clock timings are retained separately from deterministic event fields.
class ControllerTrace {
public:
  bool write(const char *path, uint64_t seed, const std::string &configuration);
  bool replay(const char *path, const std::string &configuration);
  bool event(const std::string &line);
  bool timing(uint64_t nanos);
  bool finish(int verdict);
  uint64_t seed() const { return seed_; }
  bool replaying() const { return replay_; }
  const std::string &error() const { return error_; }

private:
  bool fail(const std::string &message);
  uint64_t seed_ = 0;
  uint64_t line_ = 0;
  bool replay_ = false;
  std::ifstream input_;
  std::ofstream output_;
  std::string error_;
};

} // namespace tactix
