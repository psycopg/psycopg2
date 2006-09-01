import psycopg2

o = psycopg2.connect("dbname=test")
c = o.cursor()

def sql():
    c.execute("SELECT 1.23 AS foo")
    print 1, c.fetchone()
    #print c.description
    c.execute("SELECT 1.23::float AS foo")
    print 2, c.fetchone()
    #print c.description

print "BEFORE"
sql()
import gtk
print "AFTER"
sql()

