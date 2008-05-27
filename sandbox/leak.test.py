"""
script: test_leak.py

This script attempts to repeatedly insert the same list of rows into
the database table, causing a duplicate key error to occur. It will
then roll back the transaction and try again.

Database table schema:
    -- CREATE TABLE t (foo TEXT PRIMARY KEY);

There are two ways to run the script, which will launch one of the
two functions:

# leak() will cause increasingly more RAM to be used by the script.
$ python <script_nam> leak

# noleak() does not have the RAM usage problem. The only difference 
# between it and leak() is that 'rows' is created once, before the loop.
$ python <script_name> noleak

Use Control-C to quit the script.
"""
import sys
import psycopg2

DB_NAME = 'test'

connection = psycopg2.connect(database=DB_NAME)
cursor = connection.cursor()
# Uncomment the following if table 't' does not exist
create_table = """CREATE TABLE t (foo TEXT PRIMARY KEY)"""
cursor.execute(create_table)

insert = """INSERT INTO t VALUES (%(foo)s)"""

def leak():
    """rows created in each loop run"""
    count = 0
    while 1:
        try:
            rows = []
            for i in range(1, 100):
                row = {'foo': i}
                rows.append(row)
            count += 1
            print "loop count:", count
            cursor.executemany(insert, rows)
            connection.commit()
        except psycopg2.IntegrityError:
            connection.rollback()

def noleak():
    """rows created once, before the loop"""
    rows = []
    for i in range(1, 100):
        row = {'foo': i}
        rows.append(row)
    count = 0
    while 1:
        try:
            count += 1
            print "loop count:", count
            cursor.executemany(insert, rows)
            connection.commit()
        except psycopg2.IntegrityError:
            connection.rollback()

usage = "%s requires one argument: 'leak' or 'noleak'" % sys.argv[0]
try:
    if 'leak' == sys.argv[1]:
        run_function = leak
    elif 'noleak' == sys.argv[1]:
        run_function = noleak
    else:
        print usage
        sys.exit()
except IndexError:
    print usage
    sys.exit()

# Run leak() or noleak(), whichever was indicated on the command line
run_function()

