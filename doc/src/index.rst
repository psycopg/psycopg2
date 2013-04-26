=================================================
Psycopg -- PostgreSQL database adapter for Python
=================================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

Psycopg_ is the most popular PostgreSQL_ database adapter for the Python_
programming language.  Its main features are the complete implementation of
the Python |DBAPI|_ specification and the thread safety (several threads can
share the same connection). It was designed for heavily multi-threaded
applications that create and destroy lots of cursors and make a large number
of concurrent :sql:`INSERT`\s or :sql:`UPDATE`\s.

Psycopg 2 is mostly implemented in C as a libpq_ wrapper, resulting in being
both efficient and secure. It features client-side and :ref:`server-side
<server-side-cursors>` cursors, :ref:`asynchronous communication
<async-support>` and :ref:`notifications <async-notify>`, |COPY-TO-FROM|__
support.  Many Python types are supported out-of-the-box and :ref:`adapted to
matching PostgreSQL data types <python-types-adaptation>`; adaptation can be
extended and customized thanks to a flexible :ref:`objects adaptation system
<adapting-new-types>`.

Psycopg 2 is both Unicode and Python 3 friendly.


.. _Psycopg: http://initd.org/psycopg/
.. _PostgreSQL: http://www.postgresql.org/
.. _Python: http://www.python.org/
.. _libpq: http://www.postgresql.org/docs/current/static/libpq.html
.. |COPY-TO-FROM| replace:: :sql:`COPY TO/COPY FROM`
.. __: http://www.postgresql.org/docs/current/static/sql-copy.html


.. rubric:: Contents

.. toctree::
   :maxdepth: 2

   install
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
   news


.. ifconfig:: builder != 'text'

    .. rubric:: Indices and tables

    * :ref:`genindex`
    * :ref:`search`


.. ifconfig:: todo_include_todos

    .. note::

        **To Do items in the documentation**

        .. todolist::

