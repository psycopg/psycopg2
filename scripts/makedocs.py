#!/usr/bin/env python
"""Build documentation and api."""

import os

EPYDOC = "python c:/programmi/python23/scripts/epydoc.py"
PSYCOPG = "c:/programmi/python23/lib/site-packages/psycopg2"

os.system("python ext2html.py ../doc/extensions.rst > ../doc/extensions.html")
os.system("%s "
          "-o ../doc/api "
          "--css ../doc/api-screen.css "
          "--docformat restructuredtext " 
          "%s"
    % (EPYDOC,PSYCOPG,))
