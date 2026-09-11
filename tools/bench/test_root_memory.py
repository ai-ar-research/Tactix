import struct
import unittest
from root_memory import populations, snapshot, vector


class RootMemoryTests(unittest.TestCase):
    def test_vectors(self):
        self.assertEqual(vector(struct.pack('<QQQ', 100, 108, 116), 0, 4)['capacity'], 4)
        self.assertEqual(vector(bytes(24), 0, 4)['size'], 0)
        for pointers in ((100, 99, 108), (100, 108, 104), (0, 4, 8), (100, 105, 108)):
            with self.assertRaises(ValueError):
                vector(struct.pack('<QQQ', *pointers), 0, 4)

    def test_populations(self):
        values = struct.pack('8b', 1, -1, -1, 1, 0, 0, 1, -1)
        assigned = b''.join(struct.pack('<I12x', v) for v in (0, 3, 0, 0))
        result = populations(values, assigned, bytes((0, 0, 0, 2)), 4, 16, 0)
        self.assertEqual(result, dict(vars=4, fixed=2, fixed_active=1, retired=1,
                                     unassigned=1, nonroot_active=1))
        self.assertEqual(populations(values, assigned, b'', 4, 16, 0)['fixed_active'], 2)

    def test_bad_population(self):
        for values, assigned, status in ((b'\0', bytes(16), b'\0'),
                                         (b'\0\0', bytes(15), b'\0'),
                                         (b'\0\0', bytes(16), b'\3'),
                                         (b'\1\1', bytes(16), b'\0')):
            with self.assertRaises(ValueError):
                populations(values, assigned, status, 1, 16, 0)

    def test_short_read(self):
        with self.assertRaises(ValueError):
            snapshot(lambda at, size: bytes(size-1), 100, dict(kernel_bytes=16))

    def test_snapshot(self):
        names = ('values', 'assigned', 'var_status', 'watch_heads', 'jumped_over',
                 'unit_ids', 'touched', 'removed', 'touched_kept', 'reconstruction')
        sizes = (1, 16, 1, 16, 4, 8, 8, 8, 8, 4)
        counts = (2, 1, 1, 2, 1, 1, 1, 1, 1, 0)
        header = bytearray(264)
        struct.pack_into('<I4xQQ', header, 0, 1, 7, 9)
        memory, specs = {100:header}, {}
        for i, (name, size, count) in enumerate(zip(names, sizes, counts)):
            addr = 1000+i*100
            memory[addr] = bytes(size*count)
            specs[name] = [24+i*24, size]
            struct.pack_into('<QQQ', header, 24+i*24, addr, addr+size*count, addr+size*count)
        layout = dict(kernel_bytes=264, nvars_offset=0, conflicts_offset=8, decisions_offset=16,
                      assigned_bytes=16, level_offset=0, vectors=specs)
        result = snapshot(lambda at, size: memory[at][:size], 100, layout)
        self.assertEqual(result['populations']['unassigned'], 1)
        self.assertEqual(result['selected_capacity_bytes'], 86)
        self.assertEqual((result['conflicts'], result['decisions']), (7, 9))


if __name__ == '__main__':
    unittest.main()
