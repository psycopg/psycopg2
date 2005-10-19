=======================================
 psycopg 2 extensions to the DBAPI 2.0
=======================================

This document is a short summary of the extensions built in psycopg 2.0.x over
the standard `Python Database API Specification 2.0`__, usually called simply
DBAPI-2.0 or even PEP-249.  Before reading on this document please make sure
you already know how to program in Python using a DBAPI-2.0 compliant driver:
basic concepts like opening a connection, executing queries and commiting or
rolling back a transaction will not be explained but just used.

__ http://www.python.org/peps/pep-0249.html


Connection and cursor factories
===============================

psycopg 2 exposes two new-style classes that can be sub-classed and expanded to
adapt them to the needs of the programmer: `cursor` and `connection`.  The
`connection` class is usually sub-classed only to provide a . `cursor` is much
more interesting, because it is the class where query building, execution and
result type-casting into Python variables happens.

Row factories
-------------

tzinfo factories
----------------


Setting transaction isolation levels
====================================


Adaptation of Python values to SQL types
========================================


Type casting of SQL types into Python values
============================================

Extra type objects
------------------


Working with times and dates
============================


Receiving NOTIFYs
=================


Using COPY TO and COPY FROM
===========================


PostgreSQL status message and executed query
============================================

`cursor` objects have two special fields related to the last executed query:

  - `.query` is the textual representation (str or unicode, depending on what
    was passed to `.execute()` as first argument) of the query *after* argument
    binding and mogrification has been applied. To put it another way, `.query`
    is the *exact* query that was sent to the PostgreSQL backend.
    
  - `.statusmessage` is the status message that the backend sent upon query
    execution. It usually contains the basic type of the query (SELECT,
    INSERT, UPDATE, ...) and some additional information like the number of
    rows updated and so on. Refer to the PostgreSQL manual for more
    information.

