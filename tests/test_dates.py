#!/usr/bin/env python
import math
import unittest

import psycopg2
import tests


class CommonDatetimeTestsMixin:

    def execute(self, *args):
        conn = psycopg2.connect(tests.dsn)
        curs = conn.cursor()
        curs.execute(*args)
        return curs.fetchone()[0]

    def test_parse_date(self):
        value = self.DATE('2007-01-01', None)
        self.assertNotEqual(value, None)
        self.assertEqual(value.year, 2007)
        self.assertEqual(value.month, 1)
        self.assertEqual(value.day, 1)

    def test_parse_null_date(self):
        value = self.DATE(None, None)
        self.assertEqual(value, None)

    def test_parse_incomplete_date(self):
        self.assertRaises(psycopg2.DataError, self.DATE, '2007', None)
        self.assertRaises(psycopg2.DataError, self.DATE, '2007-01', None)

    def test_parse_time(self):
        value = self.TIME('13:30:29', None)
        self.assertNotEqual(value, None)
        self.assertEqual(value.hour, 13)
        self.assertEqual(value.minute, 30)
        self.assertEqual(value.second, 29)

    def test_parse_null_time(self):
        value = self.TIME(None, None)
        self.assertEqual(value, None)

    def test_parse_incomplete_time(self):
        self.assertRaises(psycopg2.DataError, self.TIME, '13', None)
        self.assertRaises(psycopg2.DataError, self.TIME, '13:30', None)

    def test_parse_datetime(self):
        value = self.DATETIME('2007-01-01 13:30:29', None)
        self.assertNotEqual(value, None)
        self.assertEqual(value.year, 2007)
        self.assertEqual(value.month, 1)
        self.assertEqual(value.day, 1)
        self.assertEqual(value.hour, 13)
        self.assertEqual(value.minute, 30)
        self.assertEqual(value.second, 29)

    def test_parse_null_datetime(self):
        value = self.DATETIME(None, None)
        self.assertEqual(value, None)

    def test_parse_incomplete_time(self):
        self.assertRaises(psycopg2.DataError,
                          self.DATETIME, '2007', None)
        self.assertRaises(psycopg2.DataError,
                          self.DATETIME, '2007-01', None)
        self.assertRaises(psycopg2.DataError,
                          self.DATETIME, '2007-01-01 13', None)
        self.assertRaises(psycopg2.DataError,
                          self.DATETIME, '2007-01-01 13:30', None)
        self.assertRaises(psycopg2.DataError,
                          self.DATETIME, '2007-01-01 13:30:29+00:10:50', None)

    def test_parse_null_interval(self):
        value = self.INTERVAL(None, None)
        self.assertEqual(value, None)


class DatetimeTests(unittest.TestCase, CommonDatetimeTestsMixin):
    """Tests for the datetime based date handling in psycopg2."""

    def setUp(self):
        self.DATE = psycopg2._psycopg.PYDATE
        self.TIME = psycopg2._psycopg.PYTIME
        self.DATETIME = psycopg2._psycopg.PYDATETIME
        self.INTERVAL = psycopg2._psycopg.PYINTERVAL

    def test_parse_bc_date(self):
        # datetime does not support BC dates
        self.assertRaises(ValueError, self.DATE, '00042-01-01 BC', None)

    def test_parse_bc_datetime(self):
        # datetime does not support BC dates
        self.assertRaises(ValueError, self.DATETIME,
                          '00042-01-01 13:30:29 BC', None)

    def test_parse_time_microseconds(self):
        value = self.TIME('13:30:29.123456', None)
        self.assertEqual(value.second, 29)
        self.assertEqual(value.microsecond, 123456)

    def test_parse_datetime_microseconds(self):
        value = self.DATETIME('2007-01-01 13:30:29.123456', None)
        self.assertEqual(value.second, 29)
        self.assertEqual(value.microsecond, 123456)

    def test_parse_interval(self):
        value = self.INTERVAL('42 days 12:34:56.123456', None)
        self.assertNotEqual(value, None)
        self.assertEqual(value.days, 42)
        self.assertEqual(value.seconds, 45296)
        self.assertEqual(value.microseconds, 123456)

    def test_parse_negative_interval(self):
        value = self.INTERVAL('-42 days -12:34:56.123456', None)
        self.assertNotEqual(value, None)
        self.assertEqual(value.days, -43)
        self.assertEqual(value.seconds, 41103)
        self.assertEqual(value.microseconds, 876544)

    def test_adapt_date(self):
        from datetime import date
        value = self.execute('select (%s)::date::text',
                             [date(2007, 1, 1)])
        self.assertEqual(value, '2007-01-01')

    def test_adapt_time(self):
        from datetime import time
        value = self.execute('select (%s)::time::text',
                             [time(13, 30, 29)])
        self.assertEqual(value, '13:30:29')

    def test_adapt_datetime(self):
        from datetime import datetime
        value = self.execute('select (%s)::timestamp::text',
                             [datetime(2007, 1, 1, 13, 30, 29)])
        self.assertEqual(value, '2007-01-01 13:30:29')

    def test_adapt_timedelta(self):
        from datetime import timedelta
        value = self.execute('select extract(epoch from (%s)::interval)',
                             [timedelta(days=42, seconds=45296,
                                        microseconds=123456)])
        seconds = math.floor(value)
        self.assertEqual(seconds, 3674096)
        self.assertEqual(int(round((value - seconds) * 1000000)), 123456)

    def test_adapt_megative_timedelta(self):
        from datetime import timedelta
        value = self.execute('select extract(epoch from (%s)::interval)',
                             [timedelta(days=-42, seconds=45296,
                                        microseconds=123456)])
        seconds = math.floor(value)
        self.assertEqual(seconds, -3583504)
        self.assertEqual(int(round((value - seconds) * 1000000)), 123456)


# Only run the datetime tests if psycopg was compiled with support.
if not hasattr(psycopg2._psycopg, 'PYDATETIME'):
    del DatetimeTests


class mxDateTimeTests(unittest.TestCase, CommonDatetimeTestsMixin):
    """Tests for the mx.DateTime based date handling in psycopg2."""

    def setUp(self):
        self.DATE = psycopg2._psycopg.MXDATE
        self.TIME = psycopg2._psycopg.MXTIME
        self.DATETIME = psycopg2._psycopg.MXDATETIME
        self.INTERVAL = psycopg2._psycopg.MXINTERVAL

    def test_parse_bc_date(self):
        value = self.DATE('00042-01-01 BC', None)
        self.assertNotEqual(value, None)
        # mx.DateTime numbers BC dates from 0 rather than 1.
        self.assertEqual(value.year, -41)
        self.assertEqual(value.month, 1)
        self.assertEqual(value.day, 1)

    def test_parse_bc_datetime(self):
        value = self.DATETIME('00042-01-01 13:30:29 BC', None)
        self.assertNotEqual(value, None)
        # mx.DateTime numbers BC dates from 0 rather than 1.
        self.assertEqual(value.year, -41)
        self.assertEqual(value.month, 1)
        self.assertEqual(value.day, 1)
        self.assertEqual(value.hour, 13)
        self.assertEqual(value.minute, 30)
        self.assertEqual(value.second, 29)

    def test_parse_time_microseconds(self):
        value = self.TIME('13:30:29.123456', None)
        self.assertEqual(math.floor(value.second), 29)
        self.assertEqual(
            int((value.second - math.floor(value.second)) * 1000000), 123456)

    def test_parse_datetime_microseconds(self):
        value = self.DATETIME('2007-01-01 13:30:29.123456', None)
        self.assertEqual(math.floor(value.second), 29)
        self.assertEqual(
            int((value.second - math.floor(value.second)) * 1000000), 123456)

    def test_parse_interval(self):
        value = self.INTERVAL('42 days 05:50:05', None)
        self.assertNotEqual(value, None)
        self.assertEqual(value.day, 42)
        self.assertEqual(value.hour, 5)
        self.assertEqual(value.minute, 50)
        self.assertEqual(value.second, 5)

    def test_adapt_time(self):
        from mx.DateTime import Time
        value = self.execute('select (%s)::time::text',
                             [Time(13, 30, 29)])
        self.assertEqual(value, '13:30:29')

    def test_adapt_datetime(self):
        from mx.DateTime import DateTime
        value = self.execute('select (%s)::timestamp::text',
                             [DateTime(2007, 1, 1, 13, 30, 29.123456)])
        self.assertEqual(value, '2007-01-01 13:30:29.123456')

    def test_adapt_bc_datetime(self):
        from mx.DateTime import DateTime
        value = self.execute('select (%s)::timestamp::text',
                             [DateTime(-41, 1, 1, 13, 30, 29.123456)])
        self.assertEqual(value, '0042-01-01 13:30:29.123456 BC')

    def test_adapt_timedelta(self):
        from mx.DateTime import DateTimeDeltaFrom
        value = self.execute('select extract(epoch from (%s)::interval)',
                             [DateTimeDeltaFrom(days=42,
                                                seconds=45296.123456)])
        seconds = math.floor(value)
        self.assertEqual(seconds, 3674096)
        self.assertEqual(int(round((value - seconds) * 1000000)), 123456)

    def test_adapt_megative_timedelta(self):
        from mx.DateTime import DateTimeDeltaFrom
        value = self.execute('select extract(epoch from (%s)::interval)',
                             [DateTimeDeltaFrom(days=-42,
                                                seconds=45296.123456)])
        seconds = math.floor(value)
        self.assertEqual(seconds, -3583504)
        self.assertEqual(int(round((value - seconds) * 1000000)), 123456)


# Only run the mx.DateTime tests if psycopg was compiled with support.
if not hasattr(psycopg2._psycopg, 'MXDATETIME'):
    del mxDateTimeTests


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

