"""A harness to run the psycopg test suite.

If the distutils build directory exists, it will be inserted into the
path so that the tests run against that version of psycopg.
"""

from distutils.util import get_platform
import os
import sys
import unittest

# Insert the distutils build directory into the path, if it exists.
platlib = os.path.join(os.path.dirname(__file__), 'build',
                       'lib.%s-%s' % (get_platform(), sys.version[0:3]))
if os.path.exists(platlib):
    sys.path.insert(0, platlib)

import psycopg2
import tests

def test_suite():
    return tests.test_suite()

if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')

