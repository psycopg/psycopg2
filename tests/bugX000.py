#!/usr/bin/env python
import psycopg2
import time
import unittest

class DateTimeAllocationBugTestCase(unittest.TestCase):
    def test_date_time_allocation_bug(self):
        d1 = psycopg2.Date(2002,12,25)
        d2 = psycopg2.DateFromTicks(time.mktime((2002,12,25,0,0,0,0,0,0)))
        t1 = psycopg2.Time(13,45,30)
        t2 = psycopg2.TimeFromTicks(time.mktime((2001,1,1,13,45,30,0,0,0)))
        t1 = psycopg2.Timestamp(2002,12,25,13,45,30)
        t2 = psycopg2.TimestampFromTicks(
            time.mktime((2002,12,25,13,45,30,0,0,0)))


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
