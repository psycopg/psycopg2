import psycopg2
import psycopg2.extras

conn = psycopg2.connect("dbname=test")
curs = conn.cursor(cursor_factory=psycopg2.extras.DictCursor)

curs.execute("SELECT '2005-2-12'::date AS foo, 'boo!' as bar")
for x in curs.fetchall():
    print type(x), x[0], x[1], x['foo'], x['bar']
    
curs.execute("SELECT '2005-2-12'::date AS foo, 'boo!' as bar")
for x in curs:
    print type(x), x[0], x[1], x['foo'], x['bar']
    
