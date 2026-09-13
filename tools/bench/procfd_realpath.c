#define _GNU_SOURCE

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * O_TMPFILE descriptors appear as deleted inode targets, so glibc realpath
 * rejects their /proc/<pid>/fd/<fd> names even though open(2) accepts them.
 * Some unmodified competition binaries canonicalize positional arguments
 * before opening them.  Preserve realpath everywhere else and return the
 * already-absolute proc-fd spelling only for this exact numeric form.
 */

static int digits(const char **cursor) {
  const char *p = *cursor;
  if (!isdigit((unsigned char)*p))
    return 0;
  while (isdigit((unsigned char)*p))
    p++;
  *cursor = p;
  return 1;
}

static int anonymous_proc_fd(const char *path) {
  const char *p = path;
  if (!p || strncmp(p, "/proc/", 6))
    return 0;
  p += 6;
  if (!digits(&p) || strncmp(p, "/fd/", 4))
    return 0;
  p += 4;
  return digits(&p) && !*p;
}

static int proc_fd_directory(const char *path) {
  const char *p = path;
  if (!p || strncmp(p, "/proc/", 6))
    return 0;
  p += 6;
  return digits(&p) && !strcmp(p, "/fd");
}

char *realpath(const char *path, char *resolved) {
  static char *(*next_realpath)(const char *, char *);
  if (anonymous_proc_fd(path)) {
    if (resolved) {
      strcpy(resolved, path);
      return resolved;
    }
    return strdup(path);
  }
  if (!next_realpath)
    next_realpath = dlsym(RTLD_NEXT, "realpath");
  if (!next_realpath) {
    errno = ENOSYS;
    return NULL;
  }
  return next_realpath(path, resolved);
}

int access(const char *path, int mode) {
  static int (*next_access)(const char *, int);

  /*
   * Kissat checks W_OK on the descriptor's procfs directory before checking
   * and opening the already-existing descriptor itself.  Procfs directories
   * are not writable in the filesystem sense, so that preliminary test can
   * reject a usable anonymous file.  Skip only this redundant exact query;
   * stat(2), access(2) on the descriptor, and fopen(3) still enforce access.
   */
  if (mode == W_OK && proc_fd_directory(path))
    return 0;
  if (!next_access)
    next_access = dlsym(RTLD_NEXT, "access");
  if (!next_access) {
    errno = ENOSYS;
    return -1;
  }
  return next_access(path, mode);
}
