import unittest
from choice_state import FIELDS, read_choice_state


class ChoiceStateTests(unittest.TestCase):
    def fixture(self, **changes):
        row = dict.fromkeys(FIELDS, '0')
        row.update(conflicts='7', decisions='12', level='3', chosen='-9', observed='10',
                   assignment_level=str((1 << 32)-1), heap_position='42', score='1.25', reason='unassigned')
        row.update({key:str(value) for key,value in changes.items()})
        return '\t'.join(row[key] for key in FIELDS)+'\n'

    def read(self, text):
        return read_choice_state(text, conflict=7, variable=10)

    def test_unassigned_and_assigned(self):
        self.assertEqual(self.read(self.fixture())[0]['value'], 0)
        for reason in ('decision', 'unit', 'binary:-3', 'large:1,-2,3'):
            row = self.read(self.fixture(value=-1, assignment_level=2, reason=reason))[0]
            self.assertEqual(row['reason'], reason)

    def test_framing_and_width(self):
        for text in ('', self.fixture()[:-1], self.fixture()+'\n', self.fixture().replace('\t', ' ', 1)):
            with self.assertRaises(ValueError): self.read(text)

    def test_selection_and_numeric_fields(self):
        for changes in ({'conflicts':8}, {'observed':11}, {'chosen':0}, {'stable':2}, {'value':2},
                        {'queue_stamp':-1}, {'decisions':'bad'}, {'score':'nan'}, {'score':'inf'}, {'score':-1}):
            with self.assertRaises(ValueError): self.read(self.fixture(**changes))

    def test_unassigned_contract(self):
        for changes in ({'assignment_level':0}, {'reason':'decision'}):
            with self.assertRaises(ValueError): self.read(self.fixture(**changes))

    def test_assigned_contract(self):
        for reason in ('unassigned', 'binary:0', 'binary:1,2', 'large:', 'large:1,0', 'unknown'):
            with self.assertRaises(ValueError):
                self.read(self.fixture(value=1, assignment_level=2, reason=reason))
        with self.assertRaises(ValueError):
            self.read(self.fixture(value=1, assignment_level=4, reason='decision'))

    def test_order(self):
        self.assertEqual(len(self.read(self.fixture()+self.fixture(decisions=13))), 2)
        for decision in (11, 12):
            with self.assertRaises(ValueError): self.read(self.fixture()+self.fixture(decisions=decision))

    def test_selected_arguments(self):
        for conflict, variable in ((-1,10), (7,0)):
            with self.assertRaises(ValueError): read_choice_state(self.fixture(), conflict=conflict, variable=variable)
