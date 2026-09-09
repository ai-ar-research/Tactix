import unittest
from watch_observer import watch_payload, watch_replay


class WatchObserverTests(unittest.TestCase):
    def payload(self):
        return 'scan=-2|block=3|clause=1,2,3|searched=2|pose=2,3,1|values=-1,-1,0'

    def test_binary_and_large(self):
        self.assertEqual(watch_payload('scan=-2|block=3|clause=2,3', -2)['block'], '3')
        self.assertEqual(watch_payload(self.payload(), -2)['pose'], '2,3,1')

    def test_payload_framing(self):
        for p in (self.payload()+'|scan=-2', self.payload()+'|unknown=1', self.payload()+'|bad',
                  self.payload().replace('scan=-2', 'scan=-3'), self.payload().replace('|block=3', '')):
            with self.assertRaises(ValueError): watch_payload(p, -2)

    def test_pose_values_and_metadata(self):
        for p in (self.payload().replace('1,2,3', '2,1,3'), self.payload().replace('pose=2,3,1', 'pose=1,3,2'),
                  self.payload().replace('pose=2,3,1', 'pose=2,3,3'), self.payload().replace('-1,-1,0', '-1,0'),
                  self.payload().replace('-1,-1,0', '-1,-1,2'), self.payload().replace('searched=2', 'searched=-1'),
                  self.payload().replace('|values=-1,-1,0', '')):
            with self.assertRaises(ValueError): watch_payload(p, -2)

    def rows(self):
        a = dict(seq=0, event='imply', conflicts=10, payload='lit=1')
        b = dict(seq=1, event='conflict', conflicts=11, payload='set=1,2')
        w = dict(seq=1, event='watch', conflicts=10, payload=self.payload())
        return [a, b], [a, w, dict(b, seq=2)]

    def test_exact_replay_with_duplicate_watches(self):
        old, new = self.rows()
        self.assertEqual(len(watch_replay(old, new, scan=-2, window=(10,11))), 1)
        new.insert(2, dict(new[1], seq=2)); new[-1]['seq'] = 3
        self.assertEqual(len(watch_replay(old, new, scan=-2, window=(10,11))), 2)

    def test_missing_changed_reordered_unknown(self):
        old, new = self.rows()
        for bad in (new[:-1], [dict(new[0], payload='lit=2'), *new[1:]],
                    [new[-1], *new[:-1]], [new[0], dict(new[1], event='watch_post'), new[-1]]):
            with self.assertRaises(ValueError): watch_replay(old, bad, scan=-2, window=(10,11))

    def test_empty_wrong_window_and_control(self):
        old, new = self.rows()
        for a,b,window in ((old,old,(10,11)), ([],new,(10,11)), (new,new,(10,11)), (old,new,(11,12))):
            with self.assertRaises(ValueError): watch_replay(a,b,scan=-2,window=window)


if __name__ == '__main__':
    unittest.main()
