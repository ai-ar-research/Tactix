import unittest

from bounded_trace_config import control_window, validate_trace_keys, UNREACHABLE_FINITE_STOP


class BoundedTraceConfigTests(unittest.TestCase):
    suffixes = ('EVENT_TRACE EVENT_TRACE_STOP_AFTER_CONFLICT EVENT_TRACE_CONTROL_ONLY '
        'EVENT_TRACE_CONFLICT_FROM EVENT_TRACE_CONFLICT_TO EVENT_TRACE_SKIP_WATCH '
        'EVENT_TRACE_SKIP_IMPLY EVENT_TRACE_SKIP_HEAP EVENT_TRACE_SKIP_RANDEC_DRAWS '
        'EVENT_TRACE_LIMIT').split()

    def source(self, prefix):
        return '\n'.join('std::getenv (\n "'+prefix+'_'+s+'");' for s in self.suffixes)

    def build(self, prefix='TACTIX', **kwargs):
        args = dict(path='trace.tsv',window=(10,13),prefix_stop=50,cap=250000,source=self.source(prefix))
        args.update(kwargs)
        return control_window(prefix, **args)

    def test_both_solvers_exact_endpoint(self):
        for prefix in ('TACTIX','KISSAT'):
            env = self.build(prefix)
            self.assertEqual(len(env),10)
            self.assertEqual(env[prefix+'_EVENT_TRACE_STOP_AFTER_CONFLICT'],str(UNREACHABLE_FINITE_STOP))
            self.assertNotIn(prefix+'_EVENT_TRACE_STOP_CONFLICT',env)

    def test_misspelled_setting_is_rejected(self):
        env = self.build()
        env['TACTIX_EVENT_TRACE_STOP_CONFLICT'] = env.pop('TACTIX_EVENT_TRACE_STOP_AFTER_CONFLICT')
        with self.assertRaisesRegex(ValueError,'STOP_CONFLICT'):
            validate_trace_keys(env,'TACTIX',self.source('TACTIX'))

    def test_string_without_getenv_does_not_count(self):
        source = self.source('TACTIX').replace('std::getenv (\n "TACTIX_EVENT_TRACE_CONTROL_ONLY");','"TACTIX_EVENT_TRACE_CONTROL_ONLY";')
        with self.assertRaisesRegex(ValueError,'CONTROL_ONLY'): self.build(source=source)

    def test_missing_key_and_wrong_source(self):
        with self.assertRaises(ValueError): self.build(source=self.source('KISSAT'))
        with self.assertRaises(ValueError): self.build(source='')

    def test_window_and_endpoint(self):
        for kwargs in (dict(window=(13,10)),dict(window=(-1,10)),dict(window=(10,)),
                       dict(prefix_stop=13),dict(prefix_stop=UNREACHABLE_FINITE_STOP)):
            with self.subTest(kwargs=kwargs),self.assertRaises(ValueError): self.build(**kwargs)

    def test_path_cap_and_prefix(self):
        for kwargs in (dict(path=''),dict(cap=0)):
            with self.assertRaises(ValueError): self.build(**kwargs)
        with self.assertRaises(ValueError): self.build('OTHER')

    def test_only_control_mode_no_second_reachable_stop(self):
        env = self.build()
        self.assertFalse(any('SEARCH_ONLY' in k or 'TRAJECTORY_ONLY' in k for k in env))
        self.assertEqual(env['TACTIX_EVENT_TRACE_CONFLICT_FROM'],'10')
        self.assertEqual(env['TACTIX_EVENT_TRACE_CONFLICT_TO'],'13')
        self.assertEqual(env['TACTIX_EVENT_TRACE_LIMIT'],'250000')


if __name__ == '__main__': unittest.main()
