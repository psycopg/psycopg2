"""
A script to reproduce the race condition described in ticket #58

from https://bugzilla.redhat.com/show_bug.cgi?id=711095

Results in the error:

  python: Modules/gcmodule.c:277: visit_decref: Assertion `gc->gc.gc_refs != 0'
  failed.

on unpatched library.
"""

import threading
import gc
import time

import psycopg2
from StringIO import StringIO

done = 0

class GCThread(threading.Thread):
    # A thread that sits in an infinite loop, forcing the garbage collector
    # to run
    def run(self):
        global done
        while not done:
            gc.collect()
            time.sleep(0.1) # give the other thread a chance to run

gc_thread = GCThread()


# This assumes a pre-existing db named "test", with:
#   "CREATE TABLE test (id serial PRIMARY KEY, num integer, data varchar);"

conn = psycopg2.connect("dbname=test user=postgres")
cur = conn.cursor()

# Start the other thread, running the GC regularly
gc_thread.start()

# Now do lots of "cursor.copy_from" calls:
print "copy_from"
for i in range(1000):
    f = StringIO("42\tfoo\n74\tbar\n")
    cur.copy_from(f, 'test', columns=('num', 'data'))
    # Assuming the other thread gets a chance to run during this call, expect a
    # build of python (with assertions enabled) to bail out here with:
    #    python: Modules/gcmodule.c:277: visit_decref: Assertion `gc->gc.gc_refs != 0' failed.

# Also exercise the copy_to code path
print "copy_to"
cur.execute("truncate test")
f = StringIO("42\tfoo\n74\tbar\n")
cur.copy_from(f, 'test', columns=('num', 'data'))
for i in range(1000):
    f = StringIO()
    cur.copy_to(f, 'test', columns=('num', 'data'))

# And copy_expert too
print "copy_expert"
cur.execute("truncate test")
for i in range(1000):
    f = StringIO("42\tfoo\n74\tbar\n")
    cur.copy_expert("copy test to stdout", f)

# Terminate the GC thread's loop:
done = 1

cur.close()
conn.close()


