import io
import unittest
from congruence_trace import BINARY_FIELDS, CANDIDATE_FIELDS, read_binaries, read_candidates


def binary_text(solver='tactix'):
    text = '# tactix-congruence-binaries\t1\n# ' + '\t'.join(BINARY_FIELDS) + '\n'
    stages = (('sparse-entry', 'entry', 'post-binary-extraction', 'exit')
              if solver == 'kissat' else ('entry', 'post-binary-extraction', 'exit'))
    for stage in stages:
        text += f'{solver}\tboundary\t{stage}\t12\t1\t-\t-\t-\t-\t9\n'
        text += f'{solver}\tpair\t{stage}\t12\t0\t-3\t5\t4\t8\t9\n'
    return text


def candidate_text(solver='tactix'):
    return ('# tactix-congruence-candidates\t1\n# ' + '\t'.join(CANDIDATE_FIELDS) + '\n'
        + f'{solver}\tbegin\t12\t-\t-\t-\t-\t-\t-\t-\t-\t-\t0\t0\t0\t-\n'
        + f'{solver}\tcandidate\t12\t0\t5\t-3\t7\t-3\t5\t7\t-3\t7\t0\t3\t3\textract\n'
        + f'{solver}\tend\t12\t1\t-\t-\t-\t-\t-\t-\t-\t-\t0\t3\t3\tcomplete=1;extracted=1;production_ticks=9\n')


class CongruenceTraceTests(unittest.TestCase):
    def test_both_solvers_and_multiple_invocations(self):
        for solver in ('tactix','kissat'):
            for maker,reader,count in ((binary_text,read_binaries,8 if solver=='kissat' else 6),(candidate_text,read_candidates,2)):
                text=maker(solver)
                text+='\n'.join(text.splitlines()[2:])+'\n'
                self.assertEqual(len(reader(io.StringIO(text),solver,12)),count)

    def test_binary_identity_and_order_preserved(self):
        groups=read_binaries(io.StringIO(binary_text()),'tactix',12)
        self.assertEqual(groups[0]['rows'],[(-3,5,4,8)])

    def test_candidate_pose_key_and_pair_preserved(self):
        groups=read_candidates(io.StringIO(candidate_text()),'tactix',12)
        self.assertEqual(groups[0]['rows'],[dict(pose=(5,-3,7),key=(-3,5,7),pair=(-3,7),disposition='extract')])

    def test_binary_missing_or_extra_row_and_stage_rejected(self):
        good=binary_text()
        for bad in (good.rsplit('\n',2)[0]+'\n', good.replace('\t1\t-\t','\t2\t-\t'),
                    good.replace('post-binary-extraction','entry'),good.replace('\t0\t-3\t5','\t1\t-3\t5')):
            with self.assertRaises(AssertionError):read_binaries(io.StringIO(bad),'tactix',12)

    def test_candidate_count_effort_and_metadata_rejected(self):
        good=candidate_text()
        for bad in (good.replace('\tend\t12\t1','\tend\t12\t2'),good.replace('\t0\t3\t3\textract','\t1\t3\t3\textract'),
                    good.replace('complete=1','complete=2'),good.replace('extracted=1','extracted=-1')):
            with self.assertRaises(AssertionError):read_candidates(io.StringIO(bad),'tactix',12)

    def test_candidate_clause_and_pair_rejected(self):
        for bad in (candidate_text().replace('\t-3\t5\t7','\t-3\t7\t5'),
                    candidate_text().replace('\t-3\t7\t0\t3','\t-\t7\t0\t3')):
            with self.assertRaises(AssertionError):read_candidates(io.StringIO(bad),'tactix',12)

    def test_candidate_truncated_invocation_rejected(self):
        text=candidate_text()
        with self.assertRaises(AssertionError):read_candidates(io.StringIO('\n'.join(text.splitlines()[:-1])+'\n'),'tactix',12)

    def test_schema_identity_and_newline_rejected(self):
        for maker,reader in ((binary_text,read_binaries),(candidate_text,read_candidates)):
            good=maker()
            for bad in (good.replace('\t1\n#','\t2\n#'),good[:-1],good.replace('\t12\t','\t13\t')):
                with self.assertRaises(AssertionError):reader(io.StringIO(bad),'tactix',12)
            with self.assertRaises(AssertionError):reader(io.StringIO(good),'kissat',12)

    def test_native_sparse_and_dense_entry_are_distinct(self):
        groups=read_binaries(io.StringIO(binary_text('kissat')),'kissat',12)
        self.assertEqual([g['stage'] for g in groups],['sparse-entry','entry','post-binary-extraction','exit'])
        with self.assertRaises(AssertionError):
            read_binaries(io.StringIO(binary_text('kissat').replace('sparse-entry','entry')),'kissat',12)

    def test_empty_complete_frames_are_not_missing_frames(self):
        text='\n'.join(line for line in binary_text().splitlines() if '\tpair\t' not in line)+'\n'
        text=text.replace('\t1\t-\t','\t0\t-\t')
        self.assertTrue(all(not g['rows'] for g in read_binaries(io.StringIO(text),'tactix',12)))
        text='\n'.join(line for line in candidate_text().splitlines() if '\tcandidate\t' not in line)+'\n'
        text=text.replace('\tend\t12\t1','\tend\t12\t0').replace('\t0\t3\t3\t','\t0\t0\t0\t').replace('extracted=1','extracted=0')
        self.assertEqual(read_candidates(io.StringIO(text),'tactix',12)[0]['rows'],[])


if __name__=='__main__':unittest.main()
