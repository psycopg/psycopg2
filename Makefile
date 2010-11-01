PYTHON = python$(PYTHON_VERSION)
#PYTHON_VERSION = 2.4

TESTDB = psycopg2_test

all:
	@:

check:
	@echo "* Creating $(TESTDB)"
	@if psql -l | grep -q " $(TESTDB) "; then \
	    dropdb $(TESTDB) >/dev/null; \
	fi
	createdb $(TESTDB)
	# Note to packagers: this requires the postgres user running the test
	# to be a superuser.  You may change this line to use the superuser only
	# to install the contrib.  Feel free to suggest a better way to set up the
	# testing environment (as the current is enough for development).
	psql -f `pg_config --sharedir`/contrib/hstore.sql $(TESTDB)
	PSYCOPG2_TESTDB=$(TESTDB) $(PYTHON) runtests.py --verbose
