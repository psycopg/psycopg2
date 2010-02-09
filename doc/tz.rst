:mod:`psycopg2.tz` --  ``tzinfo`` implementations for Psycopg 2
===============================================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

.. module:: psycopg2.tz

This module holds two different tzinfo implementations that can be used as the
:obj:`tzinfo` argument to datetime constructors, directly passed to psycopg
functions or used to set the :attr:`cursor.tzinfo_factory` attribute in
cursors. 

.. todo:: tz module

.. autoclass:: psycopg2.tz.FixedOffsetTimezone

.. autoclass:: psycopg2.tz.LocalTimezone

