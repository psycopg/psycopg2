How to make a psycopg2 release
==============================

- Edit ``setup.py`` and set a stable version release. Use PEP 440 to choose
  version numbers, e.g.

  - ``2.7``: a new major release, new features
  - ``2.7.1``: a bugfix release
  - ``2.7.1.1``: a release to fix packaging problems
  - ``2.7.2.dev0``: version held during development, non-public test packages...
  - ``2.8b1``: a beta for public tests

  In the rest of this document we assume you have exported the version number
  into an environment variable, e.g.::

    $ export VERSION=2.8.4

- Push psycopg2 to master or to the maint branch. Make sure tests on `GitHub
  Actions`__ and AppVeyor__ pass.

.. __: https://github.com/psycopg/psycopg2/actions/workflows/tests.yml
.. __: https://ci.appveyor.com/project/psycopg/psycopg2

- Create a signed tag with the content of the relevant NEWS bit and push it.
  E.g.::

    # Tag name will be 2_8_4
    $ git tag -a -s ${VERSION//\./_}

    Psycopg 2.8.4 released

    What's new in psycopg 2.8.4
    ---------------------------

    New features:

    - Fixed bug blah (:ticket:`#42`).
    ...

- Create the packages:

  - On GitHub Actions run manually a `package build workflow`__.

  - On Appveyor change the `build settings`__ and replace the custom
    configuration file name from ``.appveyor/tests.yml`` to
    ``.appveyor/packages.yml`` (yeah, that sucks a bit. Remember to put it
    back to testing).

.. __: https://github.com/psycopg/psycopg2/actions/workflows/packages.yml
.. __: https://ci.appveyor.com/project/psycopg/psycopg2/settings

- When the workflows have finished download the packages using the
  ``download_packages_{github|appveyor}.py`` scripts from the
  ``scripts/build`` directory. They will be saved in a
  ``packages/psycopg2-${VERSION}`` directory.

- Only for stable packages: upload the signed packages on PyPI::

    $ twine upload -s packages/psycopg2-${VERSION}/*

- Create a release and release notes in the psycopg website, announce to
  psycopg and pgsql-announce mailing lists.

- Edit ``setup.py`` changing the version again (e.g. go to ``2.8.5.dev0``).


Releasing test packages
-----------------------

Test packages may be uploaded on the `PyPI testing site`__ using::

    $ twine upload -s -r testpypi packages/psycopg2-${VERSION}/*

assuming `proper configuration`__ of ``~/.pypirc``.

.. __: https://test.pypi.org/project/psycopg2/
.. __: https://wiki.python.org/moin/TestPyPI
