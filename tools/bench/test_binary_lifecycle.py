import io
import unittest
from binary_lifecycle import read_lifecycle


class LifecycleTest(unittest.TestCase):
    def text(self,solver='tactix'):
        fields='solver seq event conflicts search_ticks probing_ticks a b value_a value_b id copies payload'.split()
        row=[solver,'0','add','7','9','11','-3','4','0','-1','0','2','internal=5,6']
        if solver=='tactix': fields.insert(3,'rule'); row.insert(3,'Congruence')
        return '# tactix-binary-lifecycle\t3\n# '+'\t'.join(fields)+'\n'+'\t'.join(row)+'\n'
    def parse(self,text,solver='tactix'):
        return read_lifecycle(io.StringIO(text),solver,{(-3,4)})
    def test_both_schemas(self):
        for solver in ('tactix','kissat'):
            self.assertEqual(self.parse(self.text(solver),solver)[0]['copies'],2)
    def test_empty(self):
        self.assertEqual(self.parse('\n'.join(self.text().splitlines()[:2])+'\n'),[])
    def test_invalid_identity(self):
        s=self.text()
        for bad in (s.replace('lifecycle\t3','lifecycle\t2'),s[:-1],s.replace('tactix\t0','kissat\t0'),s.replace('\t0\tadd','\t1\tadd')):
            with self.subTest(bad=bad),self.assertRaises((AssertionError,ValueError)): self.parse(bad)
    def test_invalid_pair_and_value(self):
        s=self.text()
        for bad in (s.replace('-3\t4','-2\t4'),s.replace('-3\t4','4\t-3'),s.replace('\t0\t-1','\t2\t-1'),s.replace('\t9\t11','\t-1\t11')):
            with self.subTest(bad=bad),self.assertRaises((AssertionError,ValueError)): self.parse(bad)
    def test_sequence_and_conflict(self):
        s=self.text(); row=s.splitlines()[-1].replace('tactix\t0','tactix\t1')+'\n'
        self.assertEqual(len(self.parse(s+row)),2)
        with self.assertRaises(AssertionError): self.parse(s+row.replace('\t7\t9','\t6\t9'))


if __name__=='__main__': unittest.main()
