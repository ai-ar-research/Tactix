import hashlib
from pathlib import Path
import tempfile
import unittest
from certificate_prefix import hash_prefix


class CertificatePrefixTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.path = Path(self.folder.name) / 'certificate'

    def test_exact_prefix_ignores_later_bytes(self):
        self.path.write_bytes(b'abc\x00later')
        self.assertEqual(hash_prefix(self.path, 4), dict(bytes=4, sha256=hashlib.sha256(b'abc\x00').hexdigest()))

    def test_full_and_empty_prefixes(self):
        self.path.write_bytes(b'ab')
        for count in (0, 2):
            self.assertEqual(hash_prefix(self.path, count)['sha256'], hashlib.sha256(b'ab'[:count]).hexdigest())

    def test_truncation_rejected(self):
        self.path.write_bytes(b'a')
        with self.assertRaises(ValueError): hash_prefix(self.path, 2)

    def test_invalid_count_rejected(self):
        for count in (-1, True, 1.0, '1'):
            with self.assertRaises(ValueError): hash_prefix(self.path, count)

    def test_missing_file_not_empty_certificate(self):
        with self.assertRaises(FileNotFoundError): hash_prefix(self.path, 0)

    def test_multiple_blocks_and_partial_final_block(self):
        data = b'x' * ((1 << 20) + 17)
        self.path.write_bytes(data + b'not included')
        self.assertEqual(hash_prefix(self.path, len(data))['sha256'], hashlib.sha256(data).hexdigest())


if __name__ == '__main__': unittest.main()
