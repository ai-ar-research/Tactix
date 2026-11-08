"""Bounded cleanup for completed test fixtures on the shared filesystem."""
import errno
import sys
import tempfile
import time

class RetryingTemporaryDirectory(tempfile.TemporaryDirectory):
    def __init__(self, *args, cleanup_timeout=30, **kwargs):
        super().__init__(*args, **kwargs)
        self.cleanup_timeout = cleanup_timeout

    def __exit__(self, kind, value, traceback):
        deadline = time.monotonic() + self.cleanup_timeout
        retried = False
        while True:
            try:
                self.cleanup()
                return False
            except OSError as error:
                if error.errno == errno.ENOTEMPTY and time.monotonic() < deadline:
                    if not retried:
                        print(f"Retrying ENOTEMPTY fixture cleanup: {self.name}",
                              file=sys.stderr, flush=True)
                        retried = True
                    time.sleep(0.05)
                    continue
                if kind is not None:
                    # A teardown error must not replace a solver/test failure.
                    print(f"Fixture cleanup also failed: {error!r}",
                          file=sys.stderr, flush=True)
                    return False
                raise
