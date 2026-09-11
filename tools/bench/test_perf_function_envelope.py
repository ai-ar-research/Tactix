import unittest
from perf_callstack import Frame, Sample
from perf_function_envelope import function_envelope


class FunctionEnvelopeTests(unittest.TestCase):
    def summarize(self, samples):
        return function_envelope(samples, '/bin/pin', 'vivify(', {'phase()': None},
                                 'caller()', {0x100: 'probe'})

    def test_complete_descendants_and_self(self):
        tail = (Frame(3, 'phase()', '/bin/pin'), Frame(0x100, 'caller()', '/bin/pin'))
        target = Frame(2, 'vivify(Kernel&)', '/bin/pin')
        samples = [Sample(7, (Frame(1, 'malloc()', '/lib/libc'), target, *tail)),
                   Sample(11, (target, *tail)), Sample(13, tail)]
        r = self.summarize(samples)
        self.assertEqual(r['total_period'], 31)
        self.assertEqual(r['selected_period'], 18)
        self.assertEqual(r['outside_period'], 13)
        self.assertEqual(r['by_phase'], {'probe': 18})
        self.assertEqual(r['selected_samples'], 2)

    def test_present_target_missing_phase_is_retained(self):
        r = self.summarize([Sample(17, (Frame(1, 'vivify(Kernel&)', '/bin/pin'),))])
        self.assertEqual(r['selected_period'], 17)
        self.assertEqual(r['by_phase'], {'no_phase_frame': 17})

    def test_unresolved_phase_caller_is_not_search(self):
        r = self.summarize([Sample(19, (Frame(1, 'vivify(Kernel&)', '/bin/pin'),
                                      Frame(2, 'phase()', '/bin/pin')))])
        self.assertEqual(r['by_phase'], {'unresolved_phase_caller': 19})

    def test_recursion_and_clone_are_not_double_counted(self):
        r = self.summarize([Sample(23, (Frame(1, 'vivify(Kernel&) [clone .isra.0]', '/bin/pin'),
                                      Frame(2, 'vivify(Kernel&)', '/bin/pin')))])
        self.assertEqual(r['selected_period'], 23)
        self.assertEqual(r['by_entry'], {'vivify(Kernel&)': 23})

    def test_foreign_binary_and_similar_name_are_excluded(self):
        r = self.summarize([Sample(29, (Frame(1, 'vivify(Kernel&)', '/bin/other'),)),
                           Sample(31, (Frame(2, 'vivify_one()', '/bin/pin'),))])
        self.assertEqual(r['selected_period'], 0)
        self.assertEqual(r['outside_period'], 60)

    def test_empty_huge_and_invalid_inputs(self):
        self.assertEqual(self.summarize([])['samples'], 0)
        big = 2**60 + 1
        self.assertEqual(self.summarize([Sample(big, (Frame(1, 'vivify()', '/bin/pin'),))])['selected_period'], big)
        for sample in (Sample(0, (Frame(1, 'vivify()', '/bin/pin'),)), Sample(1, ())):
            with self.assertRaises(ValueError): self.summarize([sample])
        with self.assertRaises(ValueError): function_envelope([], '/bin/pin', 'vivify', {}, None, {})


if __name__ == '__main__': unittest.main()
