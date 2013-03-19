# zopectl run script to test the DA/threading behavior
#
# Usage: bin/zopectl run test_da.py "dbname=xxx"
#
from Products.ZPsycopgDA.DA import ZDATETIME
from Products.ZPsycopgDA.db import DB
import sys
import threading


dsn = sys.argv[1]


typecasts = [ZDATETIME]


def DA_connect():
    db = DB(dsn, tilevel=2, typecasts=typecasts)
    db.open()
    return db


def assert_casts(conn, name):
    connection = conn.getcursor().connection
    if (connection.string_types ==
            {1114: ZDATETIME, 1184: ZDATETIME}):
        print '%s pass\n' % name
    else:
        print '%s fail (%s)\n' % (name, connection.string_types)


def test_connect(name):
    assert_casts(conn1, name)


conn1 = DA_connect()
t1 = threading.Thread(target=test_connect, args=('t1',))
t1.start()
t2 = threading.Thread(target=test_connect, args=('t2',))
t2.start()
t1.join()
t2.join()
