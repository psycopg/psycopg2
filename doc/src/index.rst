=================================================
Psycopg -- PostgreSQL database adapter for Python
=================================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

Psycopg_ is a PostgreSQL_ database adapter for the Python_ programming
language.  Its main features are that it supports the full Python |DBAPI|_
and it is thread safe (threads can share the connections). It was designed for
heavily multi-threaded applications that create and destroy lots of cursors and
make a large number of concurrent :sql:`INSERT`\ s or :sql:`UPDATE`\ s.
The Psycopg distribution includes ZPsycopgDA, a Zope_ Database Adapter.

Psycopg 2 is mostly implemented in C as a libpq_ wrapper, resulting in being
both efficient and secure. It features client-side and :ref:`server-side
<server-side-cursors>` cursors, :ref:`asynchronous communication
<async-support>` and :ref:`notifications <async-notify>`, |COPY-TO-FROM|__
support, and a flexible :ref:`objects adaptation system
<python-types-adaptation>`. Many basic Python types are supported
out-of-the-box and mapped to matching PostgreSQL data types, such as strings
(both bytes and Unicode), numbers (ints, longs, floats, decimals), booleans and
datetime objects (both built-in and `mx.DateTime`_), several types of
:ref:`binary objects <adapt-binary>`. Also available are mappings between lists
and PostgreSQL arrays of any supported type, between :ref:`dictionaries and
PostgreSQL hstores <adapt-hstore>`, and between :ref:`tuples/namedtuples and
PostgreSQL composite types <adapt-composite>`.

Psycopg 2 is both Unicode and Python 3 friendly.


.. _Psycopg: http://initd.org/psycopg/
.. _PostgreSQL: http://www.postgresql.org/
.. _Python: http://www.python.org/
.. _Zope: http://www.zope.org/
.. _libpq: http://www.postgresql.org/docs/current/static/libpq.html
.. |COPY-TO-FROM| replace:: :sql:`COPY TO/COPY FROM`
.. __: http://www.postgresql.org/docs/current/static/sql-copy.html


.. rubric:: Contents

.. toctree::
   :maxdepth: 2

   usage
   module
   connection
   cursor
   advanced
   extensions
   tz
   pool
   extras
   errorcodes
   faq


.. ifconfig:: builder != 'text'

    .. rubric:: Indices and tables

    * :ref:`genindex`
    * :ref:`search`


.. ifconfig:: todo_include_todos

    .. note::

        **To Do items in the documentation**

        .. todolist::

