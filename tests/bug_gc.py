#!/usr/bin/env python

import psycopg2
import psycopg2.extensions
import time
import unittest
import gc

import sys
if sys.version_info < (3,):
    import tests
else:
    import py3tests as tests

class StolenReferenceTestCase(unittest.TestCase):
    def test_stolen_reference_bug(self):
        def fish(val, cur):
            gc.collect()
            return 42
        conn = psycopg2.connect(tests.dsn)
        UUID = psycopg2.extensions.new_type((2950,), "UUID", fish)
        psycopg2.extensions.register_type(UUID, conn)
        curs = conn.cursor()
        curs.execute("select 'b5219e01-19ab-4994-b71e-149225dc51e4'::uuid")
        curs.fetchone()

def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
