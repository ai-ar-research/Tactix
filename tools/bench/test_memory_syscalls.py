import unittest
from memory_syscalls import Intervals, analyze, owner, parse


def trace(*calls):
    return '\n'.join(f'7 10.{i:06d} {call}\n > /pin(tactix::grow()+0x2) [0x400002]'
                     for i, call in enumerate(calls))


def run(text):
    events, unknown = parse(text)
    return analyze(events, unknown, '/pin', 4096)


class MemorySyscallsTest(unittest.TestCase):
    def test_loss_accounting(self):
        events, unknown = parse(trace('brk(NULL) = 0x1000') + '\n7 --- SIGTERM ---')
        self.assertEqual(len(events), 1)
        self.assertEqual(unknown, [dict(line=3, text='7 --- SIGTERM ---')])
        with self.assertRaises(ValueError):
            analyze(events, unknown, '/pin', 4096)

    def test_frame_and_gap_preserved(self):
        text = trace('brk(NULL) = 0x1000') + '\n > no matching address range'
        events, _ = parse(text)
        self.assertEqual(owner(events[0], '/pin'), 'tactix::grow()')
        self.assertEqual(events[0].frames[-1], 'no matching address range')
        self.assertEqual(run(text)['incomplete_stack_events'], [0])

    def test_brk_growth_shrink_failed(self):
        result = run(trace('brk(NULL) = 0x1000', 'brk(0x5000) = 0x5000',
                           'brk(0x9000) = 0x5000', 'brk(0x3000) = 0x3000'))
        self.assertEqual(result['peak']['heap_extent_bytes'], 16384)
        self.assertEqual(result['final']['heap_extent_bytes'], 8192)
        self.assertEqual(len(result['failed_syscalls']), 1)

    def test_mapping_rounding_and_partial_fixed(self):
        result = run(trace('brk(NULL) = 0x1000',
            'mmap(NULL, 9000, PROT_READ, MAP_PRIVATE, 4, 0) = 0x10000',
            'mmap(0x11000, 4096, PROT_READ, MAP_PRIVATE|MAP_FIXED, 4, 0) = 0x11000',
            'munmap(0x11000, 4096) = 0'))
        self.assertEqual(result['peak']['traced_mapping_bytes'], 12288)
        self.assertEqual(result['final']['traced_mapping_bytes'], 8192)

    def test_remap_move_expand_and_shrink(self):
        result = run(trace('brk(NULL) = 0x1000',
            'mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x10000',
            'mremap(0x10000, 4096, 8192, MREMAP_MAYMOVE) = 0x20000',
            'mremap(0x20000, 8192, 4096, 0) = 0x20000', 'munmap(0x20000, 4096) = 0'))
        self.assertEqual(result['peak']['traced_mapping_bytes'], 8192)
        self.assertEqual(result['final']['traced_mapping_bytes'], 0)
        self.assertTrue(result['peak']['mapping_owners'][0]['anonymous'])

    def test_failed_mmap_no_mapping(self):
        result = run(trace('brk(NULL) = 0x1000',
            'mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, 4, 0) = -1 ENOMEM (Cannot allocate memory)'))
        self.assertEqual(result['final']['traced_mapping_bytes'], 0)
        self.assertEqual(len(result['failed_syscalls']), 1)

    def test_invalid_contracts(self):
        bad = ['', trace('brk(0x1000) = 0x1000'),
            trace('brk(NULL) = 0x1000', 'munmap(0x8000, 4096) = 0'),
            trace('brk(NULL) = 0x1000', 'mremap(0x8000, 4096, 8192, MREMAP_DONTUNMAP) = 0x9000'),
            trace('brk(NULL) = 0x1000', 'brk(0) = 0'),
            trace('brk(NULL) = 0x1000') + '\n8 11.000001 brk(NULL) = 0x1000']
        for text in bad:
            with self.subTest(text=text), self.assertRaises(ValueError):
                run(text)

    def test_interval_overlap_rejected(self):
        ranges = Intervals()
        ranges.add(100, 200, 0, True)
        with self.assertRaises(ValueError):
            ranges.add(150, 250, 1, True)
        ranges.add(150, 250, 1, False, replace=True)
        self.assertEqual(ranges.ranges, [(100, 150, 0, True), (150, 250, 1, False)])

    def test_timestamp_and_page_size_rejected(self):
        events, unknown = parse(trace('brk(NULL) = 0x1000', 'brk(NULL) = 0x1000'))
        for page_size in (True, 0, 3):
            with self.assertRaises(ValueError):
                analyze(events, unknown, '/pin', page_size)
        events[1].time_us = 0
        with self.assertRaises(ValueError):
            analyze(events, unknown, '/pin', 4096)


if __name__ == '__main__':
    unittest.main()
