import hashlib
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from root_census_task import install_observer


class RootCensusTaskTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.c = Path(self.tmp.name)
        (self.c/'logs').mkdir()
        self.binary = self.c/'candidate'
        self.protocol = SimpleNamespace(sha256=lambda p: hashlib.sha256(p.read_bytes()).hexdigest())

    def install(self, measured):
        self.protocol.measured = measured
        install_observer(self.protocol,self.c,0,self.binary)

    def test_enabled_only_around_solver_and_bound_after_return(self):
        def measured(command,*args):
            Path(os.environ['TACTIX_ROOT_CENSUS']).write_text('complete\n')
            return {'wall_seconds':3}
        self.install(measured)
        with patch.dict(os.environ,{},clear=True):
            r = self.protocol.measured([self.binary])
            self.assertNotIn('TACTIX_ROOT_CENSUS',os.environ)
        self.assertEqual(r['root_census']['bytes'],9)
        self.assertEqual(r['root_census']['path'],'logs/000.root-census.tsv')

    def test_checker_bypasses(self):
        self.install(lambda command,*args:{'command':command})
        self.assertEqual(self.protocol.measured(['checker']),{'command':['checker']})
        self.assertEqual(list((self.c/'logs').iterdir()),[])

    def test_missing_artifact_not_accepted(self):
        self.install(lambda *args:{})
        self.assertEqual(self.protocol.measured([self.binary])['root_census'],{'missing':True})

    def test_restore_environment_on_failure(self):
        def fail(*args): raise RuntimeError('solver wrapper failed')
        self.install(fail)
        with patch.dict(os.environ,{'TACTIX_ROOT_CENSUS':'prior'}):
            with self.assertRaises(RuntimeError): self.protocol.measured([self.binary])
            self.assertEqual(os.environ['TACTIX_ROOT_CENSUS'],'prior')

    def test_existing_output_preserved(self):
        p=self.c/'logs/000.root-census.tsv';p.write_text('keep')
        self.install(lambda *args:self.fail('must not execute'))
        with self.assertRaises(FileExistsError):self.protocol.measured([self.binary])
        self.assertEqual(p.read_text(),'keep')

    def test_invalid_target(self):
        self.install(lambda *args:self.fail('must not execute'))
        with self.assertRaises(ValueError):self.protocol.measured([self.binary,self.binary])
        with self.assertRaises(ValueError):install_observer(self.protocol,self.c,-1,self.binary)


if __name__=='__main__': unittest.main()
