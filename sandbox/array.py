import psycopg

conn = psycopg.connect("dbname=test")
curs = conn.cursor()

curs.execute("SELECT ARRAY[1,2,3] AS foo")
print curs.fetchone()[0]

curs.execute("SELECT ARRAY['1','2','3'] AS foo")
print curs.fetchone()[0]

curs.execute("""SELECT ARRAY[',','"','\\\\'] AS foo""")
d = curs.fetchone()[0]
print d, '->', d[0], d[1], d[2]

curs.execute("SELECT ARRAY[ARRAY[1,2],ARRAY[3,4]] AS foo")
print curs.fetchone()[0]

curs.execute("SELECT ARRAY['20:00:01'::time] AS foo")
print curs.description
print curs.fetchone()[0]
