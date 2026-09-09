import io
import unittest
from congruence_stages import read_stages, read_formula
from congruence_trace import BINARY_FIELDS


class StagesTest(unittest.TestCase):
    def frames(self):
        text = '# tactix-congruence-binaries\t1\n# '+'\t'.join(BINARY_FIELDS)+'\n'
        for stage in ('entry','post-binary-extraction','installed','gates-built','exit'):
            text += f'tactix\tboundary\t{stage}\t7\t1\t-\t-\t-\t-\t9\n'
            text += f'tactix\tpair\t{stage}\t7\t0\t-2\t3\t3\t4\t9\n'
        return text
    def test_complete(self):
        self.assertEqual(len(read_stages(io.StringIO(self.frames()),7)),5)
    def test_repeated(self):
        body = '\n'.join(self.frames().splitlines()[2:])+'\n'
        self.assertEqual(len(read_stages(io.StringIO(self.frames()+body),7)),10)
    def test_reject_invalid_frames(self):
        s = self.frames()
        for bad in (s[:-1],s.replace('installed','entry'),s.replace('\t7\t','\t8\t',1),
                    s.replace('\t0\t-2','\t1\t-2',1),s.replace('\t3\t4\t9','\t3\t4\t10',1),
                    s[:s.index('tactix\tboundary\texit')]):
            with self.subTest(bad=bad[-80:]), self.assertRaises((AssertionError,ValueError)):
                read_stages(io.StringIO(bad),7)


class FormulaTest(unittest.TestCase):
    row = '0\t3\t0123456789abcdef\t-2,1,3\tpose=3,-2,1\tvalues=-2:0,1:-1,3:1\n'
    def parse(self,row,solver='tactix'):
        return list(read_formula(io.StringIO('# '+solver+'-formula-set\t2\n# redundant\tsize\thash\tliterals\n'+row),solver))
    def test_both_solvers(self):
        for solver in ('tactix','kissat'):
            self.assertEqual(self.parse(self.row,solver),[((0,(-2,1,3)),(3,-2,1),(0,-1,1))])
    def test_empty(self):
        self.assertEqual(self.parse(''),[])
    def test_duplicates_preserved(self):
        self.assertEqual(len(self.parse(self.row*2)),2)
    def test_invalid_metadata(self):
        for row in (self.row[:-1],self.row.replace('0\t3','2\t3'),self.row.replace('0\t3','0\t4'),self.row.replace('0123','G123')):
            with self.subTest(row=row), self.assertRaises((AssertionError,ValueError)):
                self.parse(row)
    def test_invalid_identity(self):
        for row in (self.row.replace('-2,1,3','1,-2,3'),self.row.replace('pose=3','pose=4'),self.row.replace('1:-1','4:-1'),self.row.replace('3:1','3:2')):
            with self.subTest(row=row), self.assertRaises((AssertionError,ValueError)):
                self.parse(row)
    def test_invalid_schema(self):
        with self.assertRaises(AssertionError):
            list(read_formula(io.StringIO('# tactix-formula-set\t1\n'),'tactix'))


if __name__ == '__main__':
    unittest.main()
