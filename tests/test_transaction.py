#!/usr/bin/env python
import psycopg2
import unittest
import tests

from psycopg2.extensions import (
    ISOLATION_LEVEL_SERIALIZABLE, STATUS_BEGIN, STATUS_READY)

class TransactionTestCase(unittest.TestCase):

    def setUp(self):
        self.conn = psycopg2.connect("dbname=%s" % tests.dbname)
        self.conn.set_isolation_level(ISOLATION_LEVEL_SERIALIZABLE)
        curs = self.conn.cursor()
        curs.execute('''
            CREATE TEMPORARY TABLE table1 (
              id int PRIMARY KEY
            )''')
        # The constraint is set to deferrable for the commit_failed test
        curs.execute('''
            CREATE TEMPORARY TABLE table2 (
              id int PRIMARY KEY,
              table1_id int,
              CONSTRAINT table2__table1_id__fk
                FOREIGN KEY (table1_id) REFERENCES table1(id) DEFERRABLE)''')
        curs.execute('INSERT INTO table1 VALUES (1)')
        curs.execute('INSERT INTO table2 VALUES (1, 1)')
        self.conn.commit()

    def tearDown(self):
        self.conn.close()

    def test_rollback(self):
        # Test that rollback undoes changes
        curs = self.conn.cursor()
        curs.execute('INSERT INTO table2 VALUES (2, 1)')
        # Rollback takes us from BEGIN state to READY state
        self.assertEqual(self.conn.status, STATUS_BEGIN)
        self.conn.rollback()
        self.assertEqual(self.conn.status, STATUS_READY)
        curs.execute('SELECT id, table1_id FROM table2 WHERE id = 2')
        self.assertEqual(curs.fetchall(), [])

    def test_commit(self):
        # Test that commit stores changes
        curs = self.conn.cursor()
        curs.execute('INSERT INTO table2 VALUES (2, 1)')
        # Rollback takes us from BEGIN state to READY state
        self.assertEqual(self.conn.status, STATUS_BEGIN)
        self.conn.commit()
        self.assertEqual(self.conn.status, STATUS_READY)
        # Now rollback and show that the new record is still there:
        self.conn.rollback()
        curs.execute('SELECT id, table1_id FROM table2 WHERE id = 2')
        self.assertEqual(curs.fetchall(), [(2, 1)])

    def test_failed_commit(self):
        # Test that we can recover from a failed commit.
        # We use a deferred constraint to cause a failure on commit.
        curs = self.conn.cursor()
        curs.execute('SET CONSTRAINTS table2__table1_id__fk DEFERRED')
        curs.execute('INSERT INTO table2 VALUES (2, 42)')
        # The commit should fail, and move the cursor back to READY state
        self.assertEqual(self.conn.status, STATUS_BEGIN)
        self.assertRaises(psycopg2.IntegrityError, self.conn.commit)
        self.assertEqual(self.conn.status, STATUS_READY)
        # The connection should be ready to use for the next transaction:
        curs.execute('SELECT 1')
        self.assertEqual(curs.fetchone()[0], 1)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()

