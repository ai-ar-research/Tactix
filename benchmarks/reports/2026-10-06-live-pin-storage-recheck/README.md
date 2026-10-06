# Live-pin storage recheck

After the cumulative fuzz owner created its private Release, sanitizer and
fixture pins, `du -s --block-size=1 .` completed with exit 0 and reported
**220,945,273,344 allocated bytes** (220.95 GB). Its error output was empty.
This is below the standing 700,000,000,000-byte storage gate; no additional
cleanup is needed at this checkpoint. Active and queued campaign dependencies
remain preserved.

The preceding [cleanup verification](../2026-10-06-final-storage-verification/README.md)
owns removed paths and reclaimed bytes. This later measurement does not revise
that frozen evidence or establish fuzz acceptance.
