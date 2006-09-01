import gtk
import psycopg2

o = psycopg2.connect("dbname=test")
c = o.cursor()
c.execute("SELECT 1.23::float AS foo")
x = c.fetchone()[0]
print x, type(x)

