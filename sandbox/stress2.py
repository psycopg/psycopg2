import psycopg2

import threading, os, time, gc

super_lock = threading.Lock()

def f():
    try:
        conn = psycopg2.connect('dbname=testx')
        #c = db.cursor()
        #c.close()
        #conn.close()
        del conn
    except:
        pass
        #print "ERROR"

def g():
    n = 30
    k = 0
    i = 1
    while i > 0:
        while n > 0:
            threading.Thread(target=f).start()
            time.sleep(0.001)
            threading.Thread(target=f).start()
            time.sleep(0.001)
            threading.Thread(target=f).start()
            n -= 1
        while threading.activeCount() > 1:
            time.sleep(0.01)
        datafile = os.popen('ps -p %s -o rss' % os.getpid())
        line = datafile.readlines(2)[1].strip()
        datafile.close()
        n = 30
        print str(k*n) + '\t' + line
        k += 1

    while threading.activeCount()>1:
        pass

g()