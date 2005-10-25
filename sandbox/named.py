import psycopg2
import psycopg2.extensions

class Portal(psycopg2.extensions.cursor):
    def __init__(self, name, curs):
        psycopg2.extensions.cursor.__init__(
            self, curs.connection, '"'+name+'"')
        
CURSOR = psycopg2.extensions.new_type((1790,), "CURSOR", Portal)
psycopg2.extensions.register_type(CURSOR)

conn = psycopg2.connect("dbname=test")

curs = conn.cursor()
curs.execute("SELECT reffunc2()")

portal = curs.fetchone()[0]
print portal.fetchone()
print portal.fetchmany(2)
portal.scroll(0, 'absolute')
print portal.fetchall()


#print curs.rowcount
#print curs.statusmessage
#print curs.fetchone()
#print curs.rowcount
#print curs.statusmessage
#print curs.fetchone()
#print curs.rowcount
#print curs.statusmessage
