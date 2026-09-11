import unittest
from memory_syscalls import parse
from memory_snapshot_join import join, mapping_state
from test_memory_syscalls import trace


class MemorySnapshotJoinTest(unittest.TestCase):
    def example(self, calls, regions, before=10000100000, after=10000200000):
        events, unknown = parse(trace('brk(NULL) = 0x1000', *calls))
        snapshot = dict(smaps_before_ns=before, smaps_after_ns=after,
                        regions=[dict(start=a, end=b, name=n, kib={'Rss': rss})
                                 for a, b, n, rss in regions])
        return events, unknown, snapshot

    def test_single_and_heap_not_attributed(self):
        events, unknown, snapshot = self.example([
            'mmap(NULL, 4096, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x10000'],
            [(0x10000, 0x11000, '', 4), (0x1000, 0x3000, '[heap]', 8)])
        result = join(events, unknown, '/pin', snapshot)
        self.assertEqual(result['total_rss_kib'], 12)
        self.assertEqual(result['regions'][0]['covered_bytes'], 4096)
        self.assertEqual(result['regions'][1]['origins'], [])
        self.assertTrue(result['topology_stable_by_entry_timestamps'])

    def test_merged_region_never_apportions_rss(self):
        e, u, s = self.example([
            'mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, 4, 0) = 0x10000',
            'mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, 4, 0) = 0x11000'],
            [(0x10000, 0x12000, '', 4)])
        row = join(e, u, '/pin', s)['regions'][0]
        self.assertEqual(len(row['origins']), 2)
        self.assertEqual(row['kib']['Rss'], 4)
        self.assertTrue(all('rss' not in o and 'kib' not in o for o in row['origins']))

    def test_remap_partial_unmap_and_fixed(self):
        e, u, s = self.example([
            'mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, 4, 0) = 0x10000',
            'mremap(0x10000, 4096, 12288, MREMAP_MAYMOVE) = 0x20000',
            'munmap(0x21000, 4096) = 0',
            'mmap(0x22000, 4096, PROT_READ, MAP_PRIVATE|MAP_FIXED, 4, 0) = 0x22000'],
            [(0x20000, 0x23000, '', 4)])
        row = join(e, u, '/pin', s)['regions'][0]
        self.assertEqual([o['event'] for o in row['origins']], [2, 4])
        self.assertEqual(row['untracked_bytes'], 4096)

    def test_time_cutoff_and_concurrent_call(self):
        e, u, s = self.example([
            'mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, 4, 0) = 0x10000'],
            [(0x10000, 0x11000, '', 4)], before=10000001000, after=10000002000)
        result = join(e, u, '/pin', s)
        self.assertEqual(result['concurrent_syscalls'], [1])
        self.assertFalse(result['topology_stable_by_entry_timestamps'])
        self.assertEqual(result['regions'][0]['origins'], [])

    def test_failed_call_no_mapping(self):
        e, u, s = self.example([
            'mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, 4, 0) = -1 ENOMEM'], [])
        result = join(e, u, '/pin', s)
        self.assertEqual(len(result['failed_syscalls']), 1)
        self.assertEqual(mapping_state(e, s['smaps_before_ns']).size(), 0)

    def test_invalid_trace_and_snapshot_rejected(self):
        e, u, s = self.example([], [(1, 0, '', 0)])
        with self.assertRaises(ValueError): join(e, u, '/pin', s)
        s['regions'] = []
        with self.assertRaises(ValueError): join(e, ['lost'], '/pin', s)
        s['smaps_after_ns'] = 0
        with self.assertRaises(ValueError): join(e, u, '/pin', s)


if __name__ == '__main__':
    unittest.main()
