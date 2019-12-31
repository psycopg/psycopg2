How to build psycopg documentation
----------------------------------

Building the documentation usually requires building the library too for
introspection, so you will need the same prerequisites_.  The only extra
prerequisite is virtualenv_: the packages needed to build the docs will be
installed when building the env.

.. _prerequisites: https://www.psycopg.org/docs/install.html#install-from-source
.. _virtualenv: https://virtualenv.pypa.io/en/latest/

Build the env once with::

    make env

Then you can build the documentation with::

    make

You should find the rendered documentation in the ``html`` directory.
