import unittest
from multi_scan_watch import parse_watch,multi_scan_projection


class MultiScanWatchTests(unittest.TestCase):
    def row(self, seq=0, scan=-2, clause='2,3', conflict=10):
        return dict(seq=seq,event='watch',conflicts=conflict,payload=f'scan={scan}|block=3|clause={clause}')

    def project(self, old, new):
        return multi_scan_projection(old,new,window=(10,11),variable=3,prior_scan=-2)

    def test_new_scan_and_exact_projection(self):
        old=[self.row()]
        self.assertEqual(self.project(old,[self.row(scan=-3),self.row(seq=1)]),1)

    def test_duplicate_projection_preserved(self):
        old=[self.row(),self.row(seq=1)]
        self.assertEqual(self.project(old,old),2)
        with self.assertRaises(ValueError): self.project(old,old[:1])

    def test_wrong_scope_window_and_event(self):
        old=[self.row()]
        for bad in ([self.row(clause='2,4')],[self.row(conflict=9)],[dict(old[0],event='watch_post')],[]):
            with self.assertRaises(ValueError): self.project(old,bad)

    def test_changed_metadata_and_order(self):
        old=[self.row(),dict(self.row(seq=1),extra='x')]
        for bad in (old[::-1],[old[0],dict(old[1],extra='y')]):
            with self.assertRaises(ValueError): self.project(old,bad)

    def test_negative_selected_literal_and_excluded_controls(self):
        old=[self.row(clause='-3,2'),self.row(seq=1,clause='2,4'),self.row(seq=2,conflict=12)]
        self.assertEqual(self.project(old,[self.row(clause='-3,2')]),1)

    def test_bad_scan_and_arguments(self):
        for payload in ('scan=0|block=3|clause=2,3','block=3|scan=-2|clause=2,3',
                        'scan=-2|scan=-2|block=3|clause=2,3'):
            with self.assertRaises(ValueError): parse_watch(dict(self.row(),payload=payload))
        for variable,scan in ((0,-2),(True,-2),(3,0),(3,True)):
            with self.assertRaises(ValueError): multi_scan_projection([self.row()],[self.row()],window=(10,11),variable=variable,prior_scan=scan)


if __name__=='__main__': unittest.main()
