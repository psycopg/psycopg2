import psycopg2
import psycopg2.extras

conn = psycopg2.connect("dbname=test")
curs = conn.cursor()
curs.execute("SELECT true AS foo WHERE 'a' in %s", (("aa", "bb"),))
print curs.fetchall()
print curs.query

