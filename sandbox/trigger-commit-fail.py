import psycopg2
import traceback

# Change the table here to something the user can create tables in ...
db = psycopg2.connect('dbname=test')

cursor = db.cursor()

print 'Creating tables and sample data'

cursor.execute('''
  CREATE TEMPORARY TABLE foo (
    id int PRIMARY KEY
  )''')
cursor.execute('''
  CREATE TEMPORARY TABLE bar (
    id int PRIMARY KEY,
    foo_id int,
    CONSTRAINT bar_foo_fk FOREIGN KEY (foo_id) REFERENCES foo(id) DEFERRABLE
  )''')
cursor.execute('INSERT INTO foo VALUES (1)')
cursor.execute('INSERT INTO bar VALUES (1, 1)')

db.commit()

print 'Deferring constraint and breaking referential integrity'
cursor.execute('SET CONSTRAINTS bar_foo_fk DEFERRED')
cursor.execute('UPDATE bar SET foo_id = 42 WHERE id = 1')

print 'Committing (this should fail)'
try:
    db.commit()
except:
    traceback.print_exc()

print 'Rolling back connection'
db.rollback()

print 'Running a trivial query'
try:
    cursor.execute('SELECT TRUE')
except:
    traceback.print_exc()
print 'db.closed:', db.closed
