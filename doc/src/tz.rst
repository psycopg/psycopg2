`psycopg2.tz` --  ``tzinfo`` implementations for Psycopg 2
===============================================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. module:: psycopg2.tz

This module holds two different tzinfo implementations that can be used as the
`tzinfo` argument to `~datetime.datetime` constructors, directly passed to
Psycopg functions or used to set the `cursor.tzinfo_factory` attribute in
cursors. 

.. autoclass:: psycopg2.tz.FixedOffsetTimezone

.. autoclass:: psycopg2.tz.LocalTimezone

