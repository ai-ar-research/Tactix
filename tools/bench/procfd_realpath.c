#define _GNU_SOURCE

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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
