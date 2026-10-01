#include "policies/controller_trace.hpp"

#include <iomanip>
#include <sstream>

namespace tactix {

bool ControllerTrace::fail(const std::string &message) {
  if (error_.empty())
    error_ = message;
  return false;
}

bool ControllerTrace::write(const char *path, uint64_t seed,
                            const std::string &configuration) {
  seed_ = seed;
  output_.open(path);
  if (!output_)
    return fail("cannot open controller transcript for writing");
  output_ << "controller-transcript-v1 " << seed_ << '\n'
          << "configuration " << std::quoted(configuration) << '\n';
  return output_ ? true : fail("controller transcript header write failed");
}

bool ControllerTrace::replay(const char *path,
                             const std::string &configuration) {
  replay_ = true;
  input_.open(path);
  std::string header, marker, configured;
  if (!std::getline(input_, header))
    return fail("missing controller transcript header");
  std::istringstream first(header);
  if (!(first >> marker >> seed_) || marker != "controller-transcript-v1" ||
      !(first >> std::ws).eof())
    return fail("invalid controller transcript header");
  if (!std::getline(input_, header))
    return fail("missing controller configuration");
  std::istringstream second(header);
  if (!(second >> marker >> std::quoted(configured)) || marker != "configuration" ||
      !(second >> std::ws).eof() || configured != configuration)
    return fail("controller replay configuration differs from the transcript");
  return true;
}

bool ControllerTrace::event(const std::string &line) {
  if (!error_.empty())
    return false;
  ++line_;
  if (replay_) {
    std::string expected;
    if (!std::getline(input_, expected))
      return fail("controller transcript truncated at event " + std::to_string(line_));
    if (expected != line)
      return fail("controller transcript diverged at event " + std::to_string(line_));
  } else {
    output_ << line << '\n';
    if (!output_)
      return fail("controller transcript write failed");
  }
  return true;
}

bool ControllerTrace::finish(int verdict) {
  if (!event("verdict " + std::to_string(verdict)))
    return false;
  if (replay_) {
    std::string extra;
    if (std::getline(input_, extra))
      return fail("controller transcript has events after the terminal verdict");
  } else {
    output_.flush();
    if (!output_)
      return fail("controller transcript flush failed");
  }
  return true;
}

bool ControllerTrace::timing(uint64_t nanos) {
  if (!error_.empty())
    return false;
  ++line_;
  if (replay_) {
    std::string line, marker;
    uint64_t recorded;
    if (!std::getline(input_, line))
      return fail("missing controller action timing");
    std::istringstream fields(line);
    if (!(fields >> marker >> recorded) || marker != "elapsed-nanos" ||
        !(fields >> std::ws).eof())
      return fail("invalid controller action timing");
    // Wall time is evidence from the recorded run, never deterministic replay
    // state or a reward input that alters selection.
  } else {
    output_ << "elapsed-nanos " << nanos << '\n';
    if (!output_)
      return fail("controller transcript timing write failed");
  }
  return true;
}

} // namespace tactix
