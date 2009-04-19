# =======================================================================
# $Source: /sources/gnumed/gnumed/gnumed/client/testing/test-psycopg2-datetime-systematic.py,v $
__version__ = "$Revision: 1.1 $"
__author__  = "K.Hilbert <Karsten.Hilbert@gmx.net>"
__license__ = 'GPL (details at http://www.gnu.org)'
# =======================================================================

print "testing psycopg2 date/time parsing"

import psycopg2
print "psycopg2:", psycopg2.__version__

#dsn = u'dbname=gnumed_v10 user=any-doc password=any-doc'
dsn = u'dbname=test'
print dsn

conn = psycopg2.connect(dsn=dsn)

curs = conn.cursor()
cmd = u"""
select
	name,
	abbrev,
	utc_offset::text,
	case when
		is_dst then 'DST'
		else 'non-DST'
	end
from pg_timezone_names"""
curs.execute(cmd)
rows = curs.fetchall()
curs.close()
conn.rollback()

for row in rows:

	curs = conn.cursor()

	tz = row[0]
	cmd = u"set timezone to '%s'" % tz
	try:
		curs.execute(cmd)
	except StandardError, e:
		print "cannot use time zone", row
		raise e
		curs.close()
		conn.rollback()
		continue

	cmd = u"""select '1920-01-19 23:00:00+01'::timestamp with time zone"""
	try:
		curs.execute(cmd)
		curs.fetchone()
	except StandardError, e:
		print "%s (%s / %s / %s) failed:" % (tz, row[1], row[2], row[3])
		print " ", e

	curs.close()
	conn.rollback()

conn.close()

# =======================================================================
# $Log: test-psycopg2-datetime-systematic.py,v $
# Revision 1.1  2009/02/10 18:45:32  ncq
# - psycopg2 cannot parse a bunch of settable time zones
#
# Revision 1.1  2009/02/10 13:57:03  ncq
# - test for psycopg2 on Ubuntu-Intrepid
#
