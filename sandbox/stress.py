import psycopg
import psycopg.extras

conn = psycopg.connect('dbname=test')
#curs = conn.cursor()
#curs.execute("CREATE TABLE itest (n int4)")

#for i in xrange(10000000):
#    curs = conn.cursor()
#    curs.execute("INSERT INTO itest VALUES (1)")
#    curs.execute("SELECT '2003-12-12 10:00:00'::timestamp AS foo")
#    curs.execute("SELECT 'xxx' AS foo")
#    curs.fetchall()
#    curs.close()

curs = conn.cursor(factory=psycopg.extras.DictCursor)
curs.execute("select 1 as foo")
x = curs.fetchone()
print x['foo']
