import gc
import sys
import os
import signal
import warnings
import psycopg2

print "Testing psycopg2 version %s" % psycopg2.__version__

dbname = os.environ.get('PSYCOPG2_TESTDB', 'psycopg2_test')
conn = psycopg2.connect("dbname=%s" % dbname)
curs = conn.cursor()
curs.isready()

print "Now restart the test postgresql server to drop all connections, press enter when done."
raw_input()

try:
    curs.isready() # No need to test return value
    curs.isready()
except:
    print "Test passed"
    sys.exit(0)

if curs.isready():
    print "Warning: looks like the connection didn't get killed. This test is probably in-effective"
    print "Test inconclusive"
    sys.exit(1)

gc.collect() # used to error here
print "Test Passed"
