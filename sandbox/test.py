import datetime
import time
import psycopg2

#d = datetime.timedelta(12, 100, 9876)
#print d.days, d.seconds, d.microseconds
#print psycopg.adapt(d).getquoted()

conn = psycopg2.connect("dbname=test")
#conn.set_client_encoding("xxx")
curs = conn.cursor()
curs.execute("SELECT '2005-2-12'::date AS foo")
print curs.fetchall()
curs.execute("SELECT '10:23:60'::time AS foo")
print curs.fetchall()
curs.execute("SELECT '10:23:59.895342'::time AS foo")
print curs.fetchall()
curs.execute("SELECT '0:0:12.31423'::time with time zone AS foo")
print curs.fetchall()
curs.execute("SELECT '0:0:12+01:30'::time with time zone AS foo")
print curs.fetchall()
curs.execute("SELECT '2005-2-12 10:23:59.895342'::timestamp AS foo")
print curs.fetchall()
curs.execute("SELECT '2005-2-12 10:23:59.895342'::timestamp with time zone AS foo")
print curs.fetchall()

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
#curs.execute("SELECT now() AS foo", async=1);
#sleep(curs)
#print curs.fetchall()

#curs.execute("""
#    FETCH FORWARD 1 FROM zz;""", async = 1)
#curs.execute("SELECT now() AS bar", async=1);
#print curs.fetchall()

#curs.execute("SELECT now() AS bar");
#sleep(curs)

