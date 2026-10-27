#pragma once

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

// Proof::open takes a pathname. Keep a readable stream for checking its output
// without relying on Linux /proc or reopening descriptor devices on macOS.
class TemporaryFile {
public:
  std::string path;
  FILE *stream;

  TemporaryFile() {
    const char *directory = std::getenv("TMPDIR");
    path = std::string(directory && *directory ? directory : "/tmp") +
           "/tactix-test-XXXXXX";
    const int fd = mkstemp(path.data());
    assert(fd >= 0);
    stream = fdopen(fd, "w+b");
    assert(stream);
  }
  ~TemporaryFile() {
    std::fclose(stream);
    std::remove(path.c_str());
  }
  TemporaryFile(const TemporaryFile &) = delete;
  TemporaryFile &operator=(const TemporaryFile &) = delete;
};
