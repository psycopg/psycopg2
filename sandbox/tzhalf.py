import datetime
import time
import psycopg2

conn = psycopg2.connect("dbname=test")
curs = conn.cursor()
curs.execute("set timezone = 'Asia/Calcutta'")
curs.execute("SELECT now()")
print curs.fetchone()[0]
