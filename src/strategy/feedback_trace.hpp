#pragma once

#include "strategy/policy_feedback.hpp"

#include <fstream>
#include <optional>
#include <string>

#include <zlib.h>

namespace tactix::strategy::controller {

// JSON-lines value records, independent of the legacy controller transcript.
// Replay regenerates execution and verifies every deterministic field; elapsed
// records are validated structurally without becoming selection/reward input.
class FeedbackTrace final : public FeedbackReceiver {
public:
  ~FeedbackTrace();
  bool write(const char *path, const std::string &metadata);
  bool replay(const char *path, const std::string &metadata);
  const std::string &error() const { return error_; }
  bool terminal() const { return terminal_; }
  void session_begin(const Observation &) override;
  void after_action(const ActionFeedback &) override;
  void search_interval(const SearchIntervalFeedback &) override;
  void session_end(const Observation &, int, StopReason) override;

private:
  bool fail(const std::string &);
  bool close_stream();
  bool read_line(std::string &);
  bool write_line(const std::string &);
  void event(const std::string &);
  void timing(uint64_t);
  bool opened_ = false, begun_ = false, replay_ = false, terminal_ = false;
  uint32_t wire_version_ = FEEDBACK_VERSION;
  uint64_t line_ = 0, action_sequence_ = 0, interval_sequence_ = 0;
  std::ifstream input_;
  std::ofstream output_;
  gzFile compressed_ = nullptr;
  bool compressed_output_ = false;
  std::optional<Observation> previous_observation_;
  std::string error_;
};

std::string serialize_feedback(const ActionFeedback &);
std::string serialize_feedback(const SearchIntervalFeedback &);
std::string serialize_observation(const Observation &);

} // namespace tactix::strategy::controller
