#!/usr/bin/env python

#import psycopg as db
import psycopg2 as db
import threading
import time
import sys

def query_worker(dsn):
    conn = db.connect(dsn)
    cursor = conn.cursor()
    while True:
        cursor.execute("select * from pg_class")
        while True:
            row = cursor.fetchone()
            if row is None:
                break

if len(sys.argv) != 2:
    print 'usage: %s DSN' % sys.argv[0]
    sys.exit(1)
th = threading.Thread(target=query_worker, args=(sys.argv[1],))
th.setDaemon(True)
th.start()
time.sleep(1)
