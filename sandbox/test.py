import datetime
import psycopg

#d = datetime.timedelta(12, 100, 9876)
#print d.days, d.seconds, d.microseconds
#print psycopg.adapt(d).getquoted()

o = psycopg.connect("dbname=test")
c = o.cursor()
c.execute("SELECT 1.0 AS foo")
print c.fetchmany(2)
print c.fetchall()
