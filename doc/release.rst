How to make a psycopg2 release
==============================

- Edit ``setup.py`` and set a stable version release. Use PEP 440 as reversion
  numbers, e.g. ``2.7``.

- Push psycopg2 to master or to the maint branch. Make sure tests pass.
  in the `Travis settings`__ you may want to be sure that the varialbes
  ``TEST_PAST`` and ``TEST_FUTURE`` are set to a nonzero string to check all
  the supported postgres version.

.. __: https://travis-ci.org/psycopg/psycopg2/settings

- For an extra test merge or rebase the `test_i686`__ branch on the commit to
  release and push it too: this will test with Python 32 bits and debug
  versions.

.. __: https://github.com/psycopg/psycopg2/tree/test_i686

- Create a signed tag with the content of the relevant NEWS bit and push it.
  E.g.::

    $ git tag -a -s 2_7 

    Psycopg 2.7 released

    What's new in psycopg 2.7
    -------------------------

    New features:

    - Added `~psycopg2.sql` module to generate SQL dynamically (:ticket:`#308`).
    ...

- Update the `psycopg2-wheels`_ submodule to the tag version and push. This
  will build the packages on `Travis CI`__ and `AppVeyor`__ and upload them to
  the `initd.org upload`__ dir.

.. _psycopg2-wheels: https://github.com/psycopg/psycopg2-wheels
.. __: https://travis-ci.org/psycopg/psycopg2-wheels
.. __: https://ci.appveyor.com/project/psycopg/psycopg2-wheels
.. __: http://initd.org/psycopg/upload/

- Download the packages generated::

    $ rsync -arv initd.org:/home/upload/upload/psycopg2-2.7 .

- Sign the packages and upload the signatures back. This assumes you have a
  pkey configured to upload::

    $ for f in psycopg2-2.7/*.{exe,tar.gz,whl}; do gpg --armor --detach-sign $f; done
    $ rsync -arv -i ~/.ssh/id_rsa-initd-upload psycopg2-2.7 upload@initd.org:

- Run the ``copy-tarball.sh`` script on the server to copy the uploaded files
  in the `tarballs`__ dir::

    ssh psycoweb /home/psycoweb/copy-tarball.sh \
        /home/upload/upload/psycopg2-2.7/psycopg2-2.7.tar.gz 

.. __: http://initd.org/psycopg/tarballs/

- Remove the ``.exe`` from the dir, because we don't want to upload them on
  PyPI::

    for f in psycopg2-2.7/*.exe; do rm -v $f $f.asc; done

- Upload the packages on PyPI::

    twine upload psycopg2-2.7/*

- Create a release and release notes in the psycopg website, announce to
  psycopg and pgsql-announce mailing lists.

- Edit ``setup.py`` changing the version again (e.g. go to ``2.7.1.dev0``).
