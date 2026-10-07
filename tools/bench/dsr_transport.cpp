#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// Transport only, never a proof checker. Exit0 = witness-free valid syntax,
// 2 = witnessed addition, 3 = malformed syntax, 4 = I/O or invocation error.
// Match proof_oracle.py's bounded binary DSR token grammar, including accepted
// noncanonical varints. Keep literal bytes intact; rewrite only line tags.
static int scan(const char *path, FILE *output, bool early_witness) {
  FILE *input = std::fopen(path, "rb");
  if (!input) { std::perror("input"); return 4; }
  std::array<unsigned char, 1 << 20> bytes{};
  uint32_t value = 0, first = 0;
  unsigned shift = 0, kind = 0;
  bool have_first = false, witness = false;
  uint64_t offset = 0, token_start = 0;
  int result = 0;
  while (const size_t count = std::fread(bytes.data(), 1, bytes.size(), input)) {
    for (size_t i = 0; i < count; ++i, ++offset) {
      const unsigned byte = bytes[i];
      if (!kind) {
        if (byte == 1 || byte == 'a') kind = 1;
        else if (byte == 2 || byte == 'd') kind = 2;
        else {
          std::fprintf(stderr, "invalid tag at %llu\n", (unsigned long long)offset);
          result = 3; goto done;
        }
        if (output) bytes[i] = kind == 1 ? 'a' : 'd';
        have_first = false;
        token_start = offset + 1;
      } else {
        if (shift == 28 && (byte & 240)) {
          std::fprintf(stderr, "oversized token at %llu\n", (unsigned long long)token_start);
          result = 3; goto done;
        }
        value |= uint32_t(byte & 127) << shift;
        if (byte & 128) shift += 7;
        else {
          if (!value) kind = 0;
          else if (!have_first) { first = value; have_first = true; }
          else if (kind == 1 && value == first) {
            witness = true;
            if (early_witness) { result = 2; goto done; }
          }
          value = shift = 0;
          token_start = offset + 1;
        }
      }
    }
    if (output && std::fwrite(bytes.data(), 1, count, output) != count) {
      std::perror("write"); result = 4; goto done;
    }
  }
  if (std::ferror(input)) { std::perror("read"); result = 4; }
  else if (kind) {
    std::fprintf(stderr, "truncated token at %llu\n", (unsigned long long)token_start);
    result = 3;
  } else result = witness ? 2 : 0;
done:
  if (std::fclose(input) && !result) result = 4;
  return result;
}

int main(int argc, char **argv) {
  const bool convert = (argc == 4 || argc == 5) && !std::strcmp(argv[1], "convert");
  if (!convert && !(argc == 3 && !std::strcmp(argv[1], "scan"))) {
    std::fprintf(stderr, "usage: dsr-transport scan INPUT | convert INPUT OUTPUT [OWNED-STAGING]\n");
    return 4;
  }
  const int checked = scan(argv[2], nullptr, true);
  if (checked || !convert) return checked;
  struct stat source{}, destination{};
  if (stat(argv[2], &source)) { std::perror("input stat"); return 4; }
  const bool have_destination = !stat(argv[3], &destination);
  if (have_destination && source.st_dev == destination.st_dev &&
      source.st_ino == destination.st_ino) {
    std::fprintf(stderr, "input and output must differ\n");
    return 4;
  }
  // AnonymousScratch exposes unlinked regular inodes through /proc/PID/fd/N.
  // They must keep that inode: rename cannot replace a descriptor handle.
  // Validate before opening either kind of output; on descriptor write errors
  // the caller aborts the pipeline and never submits partial bytes to a checker.
  const bool descriptor = have_destination && !destination.st_nlink &&
                          S_ISREG(destination.st_mode);
  // Managed callers own this exact staging path and remove it after killing
  // and reaping us. Standalone conversion keeps its potentially large partial
  // output unnamed; only a complete output gets a transient publication name.
  std::string temporary;
  FILE *output = nullptr;
  int publish_fd = -1;
  if (descriptor) output = std::fopen(argv[3], "wb");
  else {
    int fd = -1;
    if (argc == 5) {
      temporary = argv[4];
      fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
      // Never unlink someone else's file after an exclusive-create failure.
      if (fd < 0) temporary.clear();
    } else {
      const std::string target = argv[3];
      const size_t slash = target.find_last_of('/');
      const std::string directory = slash == std::string::npos ? "." :
                                    slash == 0 ? "/" : target.substr(0, slash);
      fd = open(directory.c_str(), O_TMPFILE | O_RDWR, 0600);
      if (fd >= 0) publish_fd = dup(fd);
      if (fd >= 0 && publish_fd < 0) { close(fd); fd = -1; }
    }
    if (fd >= 0) {
      output = fdopen(fd, "wb");
      if (!output) close(fd);
    }
  }
  if (!output) {
    std::perror("output");
    if (!temporary.empty()) unlink(temporary.c_str());
    if (publish_fd >= 0) close(publish_fd);
    return 4;
  }
  int result = scan(argv[2], output, false);
  if (std::fclose(output) && !result) result = 4;
  if (!result && publish_fd >= 0) {
    temporary = std::string(argv[3]) + ".dsr-transport-XXXXXX";
    const int reservation = mkstemp(temporary.data());
    if (reservation < 0) { temporary.clear(); result = 4; }
    else {
      close(reservation);
      const std::string handle = "/proc/self/fd/" + std::to_string(publish_fd);
      if (unlink(temporary.c_str()) ||
          linkat(AT_FDCWD, handle.c_str(), AT_FDCWD, temporary.c_str(),
                 AT_SYMLINK_FOLLOW)) result = 4;
    }
  }
  if (!temporary.empty()) {
    if (!result && rename(temporary.c_str(), argv[3])) {
      std::perror("publish"); result = 4;
    }
    if (result) unlink(temporary.c_str());
  }
  if (publish_fd >= 0) close(publish_fd);
  return result;
}
