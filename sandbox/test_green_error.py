#!/usr/bin/env python
"""Test for issue #113: test with error during green processing
"""

DSN = 'dbname=test'

# import eventlet.patcher
# eventlet.patcher.monkey_patch()

import os
import signal
import psycopg2
from psycopg2 import extensions
from eventlet.hubs import trampoline

panic = []

def wait_cb(conn):
    """A wait callback useful to allow eventlet to work with Psycopg."""
    while 1:
        if panic:
            raise Exception('whatever')

        state = conn.poll()
        if state == extensions.POLL_OK:
            break
        elif state == extensions.POLL_READ:
            trampoline(conn.fileno(), read=True)
        elif state == extensions.POLL_WRITE:
            trampoline(conn.fileno(), write=True)
        else:
            raise psycopg2.OperationalError(
                "Bad result from poll: %r" % state)

extensions.set_wait_callback(wait_cb)

def handler(signum, frame):
    panic.append(True)

signal.signal(signal.SIGHUP, handler)

conn = psycopg2.connect(DSN)
curs = conn.cursor()
print "PID", os.getpid()
try:
    curs.execute("select pg_sleep(1000)")
except BaseException, e:
    print "got exception:", e.__class__.__name__, e

conn.rollback()
curs.execute("select 1")
print curs.fetchone()


# You can unplug the network cable etc. here.
# Kill -HUP will raise an exception in the callback.
