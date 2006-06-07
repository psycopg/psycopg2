import psycopg2

con = psycopg2.connect("dbname=test")

cur = con.cursor()
cur.execute("SELECT %s::regtype::oid", ('bytea', ))
print cur.fetchone()[0]
# 17

cur.execute("CREATE DOMAIN thing AS bytea")
cur.execute("SELECT %s::regtype::oid", ('thing', ))
print cur.fetchone()[0]
#62148

cur.execute("CREATE TABLE thingrel (thingcol thing)")
cur.execute("SELECT * FROM thingrel")
print cur.description
#(('thingcol', 17, None, -1, None, None, None),)
