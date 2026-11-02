#pragma once
// Byte readers for proof, journal and record streams written by a test.
// Every helper asserts that the stream or file it reads is usable.
#ifdef NDEBUG
#error "test_io.hpp asserts are checks; #undef NDEBUG before including it"
#endif

#include <cassert>
#include <cstdio>
#include <string>

inline long position(FILE *file) {
  const long result = std::ftell(file);
  assert(result >= 0);
  return result;
}

// The bytes written so far; the stream is left where it was (its end).
inline std::string written_bytes(FILE *file) {
  assert(!std::fflush(file));
  const long end = position(file);
  std::rewind(file);
  std::string result(static_cast<size_t>(end), '\0');
  assert(std::fread(result.data(), 1, result.size(), file) == result.size());
  assert(!std::fseek(file, end, SEEK_SET));
  return result;
}

// Flush, then read the whole stream from its start through EOF.
inline std::string stream_contents(FILE *file) {
  assert(!std::fflush(file));
  assert(!std::fseek(file, 0, SEEK_SET));
  std::string result;
  char buffer[4096];
  while (const size_t count = std::fread(buffer, 1, sizeof buffer, file))
    result.append(buffer, count);
  assert(!std::ferror(file));
  return result;
}

// The whole file at path, which must exist.
inline std::string read_file(const std::string &path) {
  FILE *file = std::fopen(path.c_str(), "rb");
  assert(file);
  std::string output;
  char bytes[4096];
  while (const size_t count = std::fread(bytes, 1, sizeof bytes, file))
    output.append(bytes, count);
  assert(!std::ferror(file));
  assert(!std::fclose(file));
  return output;
}

inline void write_file(const char *path, const std::string &text) {
  FILE *file = std::fopen(path, "w");
  assert(file);
  assert(std::fwrite(text.data(), 1, text.size(), file) == text.size());
  std::fclose(file);
}
