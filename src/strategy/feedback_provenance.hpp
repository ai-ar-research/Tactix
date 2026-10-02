#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace tactix::strategy::controller {

// Content identities for optional feedback provenance. No solver state or
// external hashing process is involved; callers keep their inputs immutable.
class FeedbackSha256 {
public:
  void append(const void *data, size_t size) {
    const auto *bytes = static_cast<const unsigned char *>(data);
    length_ += size;
    while (size) {
      const size_t count = std::min(size, block_.size() - used_);
      std::memcpy(block_.data() + used_, bytes, count);
      used_ += count;
      bytes += count;
      size -= count;
      if (used_ == block_.size()) { compress(); used_ = 0; }
    }
  }
  std::string finish() const {
    auto padded = *this;
    const uint64_t bits = length_ * 8;
    const unsigned char marker = 0x80, zero = 0;
    padded.append(&marker, 1);
    while (padded.used_ != 56) padded.append(&zero, 1);
    unsigned char suffix[8];
    for (unsigned i = 0; i < 8; ++i) suffix[i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
    padded.append(suffix, sizeof suffix);
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (uint32_t value : padded.state_)
      for (int shift = 28; shift >= 0; shift -= 4) result.push_back(hex[(value >> shift) & 15]);
    return result;
  }
private:
  void compress() {
    static constexpr uint32_t constants[64] = {
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t words[64];
    for (unsigned i = 0; i < 16; ++i) {
      words[i] = 0;
      for (unsigned j = 0; j < 4; ++j) words[i] = (words[i] << 8) | block_[4 * i + j];
    }
    for (unsigned i = 16; i < 64; ++i) {
      const uint32_t a = words[i - 15], b = words[i - 2];
      const uint32_t first = std::rotr(a, 7) ^ std::rotr(a, 18) ^ (a >> 3);
      const uint32_t second = std::rotr(b, 17) ^ std::rotr(b, 19) ^ (b >> 10);
      words[i] = words[i - 16] + first + words[i - 7] + second;
    }
    auto [a,b,c,d,e,f,g,h] = state_;
    for (unsigned i = 0; i < 64; ++i) {
      const uint32_t first = h + (std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25)) +
          ((e & f) ^ (~e & g)) + constants[i] + words[i];
      const uint32_t second = (std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22)) +
          ((a & b) ^ (a & c) ^ (b & c));
      h=g; g=f; f=e; e=d+first; d=c; c=b; b=a; a=first+second;
    }
    const std::array<uint32_t, 8> advanced{a,b,c,d,e,f,g,h};
    for (unsigned i = 0; i < state_.size(); ++i) state_[i] += advanced[i];
  }
  std::array<uint32_t, 8> state_{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                                0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  std::array<unsigned char, 64> block_{};
  uint64_t length_ = 0;
  size_t used_ = 0;
};

inline std::string feedback_sha256(std::string_view bytes) {
  FeedbackSha256 digest;
  digest.append(bytes.data(), bytes.size());
  return digest.finish();
}

inline bool feedback_file_sha256(const char *path, std::string &result) {
  FILE *input = std::fopen(path, "rb");
  if (!input) return false;
  FeedbackSha256 digest;
  std::array<unsigned char, 16384> buffer;
  while (const size_t size = std::fread(buffer.data(), 1, buffer.size(), input))
    digest.append(buffer.data(), size);
  const bool read = !std::ferror(input);
  const bool closed = std::fclose(input) == 0;
  if (!read || !closed) return false;
  result = digest.finish();
  return true;
}

} // namespace tactix::strategy::controller
