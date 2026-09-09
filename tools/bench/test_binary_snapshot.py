import io
import unittest
from binary_snapshot import read_snapshots,represented_values


class SnapshotTest(unittest.TestCase):
    boundaries=(('before','-'),('after','-'))
    def text(self,solver='tactix',empty=False):
        text='# '+solver+'-binary-set-snapshot\t1\n# snapshot\ta\tb\n'
        for i,(event,payload) in enumerate(self.boundaries):
            text+=f'# begin\t{i}\t{solver}\t{event}\t7\t{0 if empty else 2}\t{payload}\n'
            if not empty: text+=f'{i}\t-3\t4\n{i}\t-3\t4\n'
            text+=f'# end\t{i}\n'
        return text
    def parse(self,text,solver='tactix'):
        return read_snapshots(io.StringIO(text),solver,7,self.boundaries)
    def test_both_solvers_duplicates(self):
        for solver in ('tactix','kissat'):
            self.assertEqual(self.parse(self.text(solver),solver)[0]['rows'],[(-3,4),(-3,4)])
    def test_empty_frames(self):
        self.assertEqual(self.parse(self.text(empty=True))[0]['rows'],[])
    def test_repeated_invocations(self):
        body='\n'.join(self.text().splitlines()[2:])+'\n'
        for a,b in (('begin\t0','begin\t2'),('begin\t1','begin\t3'),('end\t0','end\t2'),('end\t1','end\t3'),('\n0\t','\n2\t'),('\n1\t','\n3\t')):
            body=body.replace(a,b)
        self.assertEqual(len(self.parse(self.text()+body)),4)
    def test_bad_schema_and_frame(self):
        text=self.text()
        for bad in (text.replace('snapshot\t1','snapshot\t2'),text[:-1],text.replace('# end\t1\n',''),
                    text.replace('before','after'),text.replace('\t7\t','\t8\t',1),text.replace('# end\t0','# end\t1')):
            with self.subTest(bad=bad[-70:]),self.assertRaises((AssertionError,ValueError)):
                self.parse(bad)
    def test_bad_pair_and_count(self):
        text=self.text()
        for bad in (text.replace('0\t-3','1\t-3',1),text.replace('\t7\t2','\t7\t1',1),text.replace('-3\t4','0\t4',1),
                    text.replace('-3\t4','5\t4',1),text.replace('-3\t4','-2\t4',1)):
            with self.subTest(bad=bad[-70:]),self.assertRaises((AssertionError,ValueError)):
                self.parse(bad)
    def test_missing_invocation(self):
        with self.assertRaises(AssertionError): self.parse(self.text().split('# begin\t1')[0])
    def test_represented_values(self):
        formula=[((0,(-2,1,3)),(3,1,-2),(1,0,-1)),((1,(-3,2,4)),(2,4,-3),(1,-1,0))]
        self.assertEqual(represented_values(formula),{1:0,2:-1,3:-1,4:0})
    def test_inconsistent_values(self):
        formula=[((0,(-2,1,3)),(3,1,-2),(1,0,-1)),((1,(-3,2,4)),(2,4,-3),(-1,-1,0))]
        with self.assertRaises(AssertionError): represented_values(formula)


if __name__=='__main__': unittest.main()
