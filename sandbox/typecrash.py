import psycopg2.extensions

print dir(psycopg2._psycopg)
print psycopg2.extensions.new_type(
    (600,), "POINT", lambda oids, name, fun: None)
print "ciccia ciccia"
print psycopg2._psycopg
