import datetime
import time
import psycopg2

#d = datetime.timedelta(12, 100, 9876)
#print d.days, d.seconds, d.microseconds
#print psycopg.adapt(d).getquoted()

conn = psycopg2.connect("dbname=test_unicode")
conn.set_client_encoding("xxx")
curs = conn.cursor()
#curs.execute("SELECT 1.0 AS foo")
#print curs.fetchmany(2)
#print curs.fetchall()

def sleep(curs):
    while not curs.isready():
        print "."
        time.sleep(.1)
        
#curs.execute("""
#    DECLARE zz INSENSITIVE SCROLL CURSOR WITH HOLD FOR
#    SELECT now();
#    FOR READ ONLY;""", async = 1)
curs.execute("SELECT now() AS foo", async=1);
sleep(curs)
print curs.fetchall()

#curs.execute("""
#    FETCH FORWARD 1 FROM zz;""", async = 1)
curs.execute("SELECT now() AS bar", async=1);
print curs.fetchall()

curs.execute("SELECT now() AS bar");
sleep(curs)

