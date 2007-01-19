import psycopg2

conn = psycopg2.connect("port=5433 dbname=test")
curs = conn.cursor()

#curs.execute("SELECT ARRAY[1,2,3] AS foo")
#print curs.fetchone()[0]

#curs.execute("SELECT ARRAY['1','2','3'] AS foo")
#print curs.fetchone()[0]

#curs.execute("""SELECT ARRAY[',','"','\\\\'] AS foo""")
#d = curs.fetchone()[0]
#print d, '->', d[0], d[1], d[2]

#curs.execute("SELECT ARRAY[ARRAY[1,2],ARRAY[3,4]] AS foo")
#print curs.fetchone()[0]

#curs.execute("SELECT ARRAY[ARRAY[now(), now()], ARRAY[now(), now()]] AS foo")
#print curs.description
#print curs.fetchone()[0]

#curs.execute("SELECT 1 AS foo, ARRAY[1,2] AS bar")
#print curs.fetchone()

#curs.execute("SELECT * FROM test()")
#print curs.fetchone()

curs.execute("SELECT %s", ([1,2,None],))
print curs.fetchone()

