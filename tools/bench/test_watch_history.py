import unittest
from watch_history import validate_projection, selected_blocks


class WatchHistoryTests(unittest.TestCase):
    def row(self, seq, conflict=10, clause='2,3'):
        return dict(seq=seq,event='watch',conflicts=conflict,decisions=7,propagations=20,
                    level=1,mode='focused',search_external=5,
                    payload='scan=-2|block=3|clause='+clause)

    def test_projection_complete_and_sequence_only(self):
        old=[self.row(0),self.row(1)]
        new=[self.row(0,9),self.row(1),self.row(2),self.row(3,11)]
        self.assertEqual(validate_projection(old,new,window=(10,10),scan=-2),2)

    def test_projection_rejects_missing_changed_and_unknown(self):
        old=[self.row(0),self.row(1,clause='2,4')]
        for bad in (old[:1],old[::-1],[dict(old[0],level=2),old[1]],
                    [old[0],dict(old[1],event='conflict')],[],[dict(old[0],payload='bad')]):
            with self.assertRaises(ValueError): validate_projection(old,bad,window=(10,10),scan=-2)

    def test_pair_positions_and_one_sided(self):
        rows=[self.row(0),self.row(1,clause='2,4'),self.row(2,11)]
        blocks=selected_blocks(rows,scan=-2,clause_keys={'a':'2,3','b':'2,4'})
        self.assertEqual([b['unique_pair'] for b in blocks],[True,False])
        self.assertEqual(blocks[0]['hits']['b'][0]['position'],1)

    def test_duplicates_not_deduplicated(self):
        blocks=selected_blocks([self.row(0),self.row(1),self.row(2,clause='2,4')],
                               scan=-2,clause_keys={'a':'2,3','b':'2,4'})
        self.assertFalse(blocks[0]['unique_pair'])
        self.assertEqual(len(blocks[0]['hits']['a']),2)

    def test_noncontiguous_same_state_not_merged(self):
        blocks=selected_blocks([self.row(0),self.row(1,11),self.row(2)],
                               scan=-2,clause_keys={'a':'2,3'})
        self.assertEqual(len(blocks),3)


if __name__=='__main__': unittest.main()
