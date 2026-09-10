import io
import unittest
from perf_callstack import Frame, Sample, samples, phase, call_sites


TEXT = ' 7\n\t100 inner(int) (/pin/bin)\n\t200 phase() (/pin/bin)\n\t304 caller() (/pin/bin)\n\n'
ASM = '''0000000000000100 <caller()>:
 100: xor %edx,%edx
 102: mov %r12,%rcx
 105: mov %r13,%rdi
 108: callq 500 <phase()>
 10d: cmp $0x14,%eax
'''


class PerfCallstackTests(unittest.TestCase):
    def sample(self, text=TEXT):
        return list(samples(io.StringIO(text)))

    def test_parse_conserve_and_keep_unknown(self):
        s = self.sample(TEXT + ' 9007199254740993\n\tffff [unknown] ([unknown])\n\n')
        self.assertEqual(sum(v.period for v in s), 9007199254741000)
        self.assertEqual(s[0].frames[0], Frame(0x100, 'inner(int)', '/pin/bin'))
        self.assertEqual(s[1].frames[0].symbol, '[unknown]')

    def test_bad_records(self):
        for text in ('7\n\n', TEXT.rstrip(), TEXT.replace(' 7', ' 0'),
                     TEXT.replace(' 7', ' -1'), TEXT.replace(' 7', ' 1.2'),
                     TEXT.replace('100 inner', 'xxx inner'), TEXT.replace(' 7\n', ''),
                     TEXT.replace('\n\n', '\n9\n\n')):
            with self.subTest(text=text), self.assertRaises(ValueError): self.sample(text)

    def test_blank_stream(self):
        self.assertEqual(self.sample('\n\n'), [])

    def test_exact_phase_and_caller_pc(self):
        s = self.sample()[0]
        self.assertEqual(phase(s, '/pin/bin', {'phase()': None}, 'caller()', {0x304:'probe'}), ('probe',1))
        self.assertEqual(phase(s, '/pin/bin', {'phase()': 'eliminate'}), ('eliminate',1))
        self.assertEqual(phase(s, '/wrong', {'phase()': 'probe'}), ('no_phase_frame',None))
        self.assertEqual(phase(s, '/pin/bin', {'phase': 'probe'}), ('no_phase_frame',None))

    def test_unknown_caller_not_assigned_to_search(self):
        s = self.sample()[0]
        for v in (s, Sample(7,s.frames[:2]), Sample(7,s.frames[:2]+(Frame(0x304,'wrong','/pin/bin'),))):
            self.assertEqual(phase(v,'/pin/bin',{'phase()':None},'caller()',{}), ('unresolved_phase_caller',1))

    def test_nested_entries_rejected(self):
        with self.assertRaises(ValueError):
            phase(self.sample()[0], '/pin/bin', {'phase()':'probe','caller()':'eliminate'})

    def test_verified_calls(self):
        found, evidence = call_sites(ASM,'phase()',{0x108:('probe','xor %edx,%edx')})
        self.assertEqual(found,{0x10c:'probe'})
        self.assertEqual(evidence[0]['call_pc'],0x108)

    def test_bad_calls(self):
        for asm in (ASM.replace('10d:', '10e:'), ASM.replace('xor %edx,%edx','mov %eax,%edx'),
                    ASM.replace('<phase()>','<wrong()>'), ASM[:ASM.index(' 10d:')],
                    ASM.replace('102:', '100:')):
            with self.subTest(asm=asm), self.assertRaises(ValueError):
                call_sites(asm,'phase()',{0x108:('probe','xor %edx,%edx')})
        with self.assertRaises(ValueError): call_sites(ASM,'phase()',{})


if __name__ == '__main__': unittest.main()
