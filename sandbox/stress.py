import psycopg2
import threading, os, time, gc

for i in range(20000):
    conn = psycopg2.connect('dbname=test')
    del conn
    if i%200 == 0:
        datafile = os.popen('ps -p %s -o rss' % os.getpid())
        line = datafile.readlines(2)[1].strip()
        datafile.close()
        print str(i) + '\t' + line
