import psycopg

conn = psycopg.connect("dbname=test")
curs = conn.cursor()

curs.execute("SELECT ARRAY[1,2,3] AS foo")
print curs.fetchone()

curs.execute("SELECT ARRAY['1','2','3'] AS foo")
print curs.fetchone()

curs.execute("""SELECT ARRAY['','"',''] AS foo""")
d = curs.fetchone()
print d, '->', d[0][0], d[0][1], d[0][2]

