import psycopg2
import psycopg2.extensions

DEC2FLOAT = psycopg2.extensions.new_type(
  psycopg2._psycopg.DECIMAL.values,
  'DEC2FLOAT',
  psycopg2.extensions.FLOAT)

psycopg2.extensions.register_type(DEC2FLOAT)

o = psycopg2.connect("dbname=test")
c = o.cursor()
c.execute("SELECT NULL::decimal(10,2)")
n = c.fetchone()[0]
print n, type(n)
