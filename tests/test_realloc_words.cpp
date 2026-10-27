#undef NDEBUG
#include "kernel/realloc_words.hpp"
#include "kernel/connected_occurrence.hpp"

#include <cstdio>
#include <string_view>
#include <sys/resource.h>
#include <vector>

using namespace tactix;

static_assert(sizeof(ReallocWords<uint32_t>) == sizeof(std::vector<uint32_t>));
static_assert(alignof(ReallocWords<uint32_t>) == alignof(std::vector<uint32_t>));

// Always moves successful growth, so retained data never relies on realloc
// happening in place. A deterministic null result tests failure ownership.
struct MovingAllocator {
  static inline size_t bytes = 0, live = 0, calls = 0;
  static inline bool fail = false;
  static void *resize(void *old, size_t requested) {
    ++calls;
    if (fail)
      return nullptr;
    void *fresh = std::malloc(requested);
    assert(fresh);
    if (old) {
      std::memcpy(fresh, old, std::min(bytes, requested));
      std::free(old);
    } else
      ++live;
    bytes = requested;
    return fresh;
  }
  static void release(void *p) {
    if (p) { std::free(p); --live; bytes = 0; }
  }
};

template <class T> static void equal(const ReallocWords<T> &a,
                                    const std::vector<T> &b) {
  assert(a.size() == b.size());
  // Capacity parity is specifically a libstdc++ compatibility contract.
#ifdef __GLIBCXX__
  assert(a.capacity() == b.capacity());
#endif
  assert(a.capacity() >= a.size());
  assert(a.empty() == b.empty());
  assert(std::equal(a.begin(), a.end(), b.begin(), b.end()));
}

static void deterministic_operations() {
  size_t operations = 0;
  for (size_t initial = 0; initial != 65; ++initial) {
    ReallocWords<uint32_t> a;
    std::vector<uint32_t> b;
#ifdef __GLIBCXX__
    assert(a.max_size() == b.max_size());
#endif
    a.reserve(initial); b.reserve(initial); equal(a,b);
    for (size_t i = 0; i != 257; ++i) {
      const size_t size = (i * 67 + initial * 11) % 193;
      a.resize(size); b.resize(size); equal(a,b); ++operations;
      for (size_t j = 0; j != size; ++j) a[j] = b[j] = uint32_t(i * 991 + j);
      if (i % 7 == 0) {
        const size_t capacity = size + (i % 29);
        a.reserve(capacity); b.reserve(capacity); equal(a,b); ++operations;
      }
      const uint32_t suffix[] = {0, UINT32_MAX, 17, 17};
      const size_t count = i % 5;
      a.insert(a.end(), suffix, suffix + count);
      b.insert(b.end(), suffix, suffix + count); equal(a,b); ++operations;
      ReallocWords<uint32_t> copy(a);
      std::vector<uint32_t> expected(b);
      equal(copy,expected);
      if (copy.size()) {
        copy[0] ^= UINT32_MAX;
        assert(copy[0] != a[0]);
        expected[0] ^= UINT32_MAX;
      }
      ReallocWords<uint32_t> assigned;
      std::vector<uint32_t> assigned_expected;
      assigned.reserve(300); assigned_expected.reserve(300);
      assigned = copy; assigned_expected = expected; equal(assigned,assigned_expected);
      assigned = assigned; equal(assigned,assigned_expected);
      ReallocWords<uint32_t> moved(std::move(copy));
      std::vector<uint32_t> moved_expected(std::move(expected));
      equal(moved,moved_expected);
      assert(copy.empty() && copy.capacity() == 0);
      copy.resize(3); assert(copy[0] == 0 && copy[2] == 0);
      assigned = std::move(moved); assigned_expected = std::move(moved_expected);
      equal(assigned,assigned_expected); assert(moved.capacity() == 0);
      if (i % 11 == 0) {
        a.clear(); b.clear(); equal(a,b);
      }
      if (i % 17 == 0) {
        ReallocWords<uint32_t>().swap(a); std::vector<uint32_t>().swap(b);
        equal(a,b);
      }
    }
  }
  std::printf("realloc-words operations=%zu size/value replay (capacity parity on libstdc++)\n", operations);
}

static void moved_growth_and_failure() {
  using Buffer = ReallocWords<uint32_t, MovingAllocator>;
  {
    Buffer a;
    a.resize(17);
    for (size_t i = 0; i < a.size(); ++i) a[i] = uint32_t(i + 31);
    a.reserve(100);
    for (size_t i = 0; i < a.size(); ++i) assert(a[i] == i + 31);
    a.resize(99);
    for (size_t i = 17; i < a.size(); ++i) assert(a[i] == 0);
    auto *before = a.data();
    const size_t calls = MovingAllocator::calls;
    assert(!a.try_reserve(Buffer::max_size() + 1));
    assert(MovingAllocator::calls == calls && a.data() == before);
    MovingAllocator::fail = true;
    assert(!a.try_reserve(1000));
    assert(a.data() == before && a.capacity() == 100 && a.size() == 99);
    assert(a[0] == 31 && a[16] == 47 && a[98] == 0);
    assert(a.try_reserve(100)); // no allocation when capacity already suffices
    MovingAllocator::fail = false;
    a.resize(7); a.resize(17);
    for (size_t i = 7; i < 17; ++i) assert(a[i] == 0);
  }
  assert(MovingAllocator::live == 0);
}

static void tagged_words() {
  using Entry = ConnectedOccurrences::Entry;
  ReallocWords<Entry> a;
  std::vector<Entry> b;
  for (size_t size : {size_t(0),size_t(1),size_t(127),size_t(3),size_t(511)}) {
    a.resize(size); b.resize(size); equal(a,b);
    for (size_t i = 0; i < size; ++i)
      a[i] = b[i] = i % 2 ? Entry::from_binary(Entry::MAX_PAYLOAD) : Entry::from_large(uint32_t(i));
    equal(a,b);
  }
}

int main(int argc, char **argv) {
  if (argc == 2) {
    const rlimit no_core{0,0};
    assert(setrlimit(RLIMIT_CORE,&no_core) == 0);
    const std::string_view mode(argv[1]);
    if (mode == "--fail-reserve") {
      MovingAllocator::fail = true;
      ReallocWords<uint32_t,MovingAllocator> a;
      a.reserve(1);
    } else if (mode == "--overflow-resize") {
      ReallocWords<uint32_t> a;
      a.resize(a.max_size() + 1);
    }
    return 2; // valid negative controls terminate before this point
  }
  assert(argc == 1);
  deterministic_operations(); moved_growth_and_failure(); tagged_words();
  std::puts("realloc-words forced-move/failure/overflow/tagged/copy/move/zero PASS");
}
