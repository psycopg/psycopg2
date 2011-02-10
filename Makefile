# Makefile for psycopg2. Do you want to...
#
# Build the library::
#
#   make
#
# Build the documentation::
#
#   make env
#   make docs
#
# Create a source package::
#
#   make env  # required to build the documentation
#   make sdist
#
# Run the test::
#
#   make check  # this requires setting up a test database with the correct user

PYTHON := python$(PYTHON_VERSION)
PYTHON_VERSION ?= $(shell $(PYTHON) -c 'import sys; print ("%d.%d" % sys.version_info[:2])')
BUILD_DIR = $(shell pwd)/build/lib.$(PYTHON_VERSION)
ENV_DIR = $(shell pwd)/env/py-$(PYTHON_VERSION)
ENV_BIN = $(ENV_DIR)/bin
ENV_LIB = $(ENV_DIR)/lib

SOURCE_C := $(wildcard psycopg/*.c psycopg/*.h)
SOURCE_PY := $(wildcard lib/*.py)
SOURCE_TESTS := $(wildcard tests/*.py)
SOURCE_DOC := $(wildcard doc/src/*.rst)
SOURCE := $(SOURCE_C) $(SOURCE_PY) $(SOURCE_TESTS) $(SOURCE_DOC)

PACKAGE := $(BUILD_DIR)/psycopg2
PLATLIB := $(PACKAGE)/_psycopg.so
PURELIB := $(patsubst lib/%,$(PACKAGE)/%,$(SOURCE_PY)) \
           $(patsubst tests/%,$(PACKAGE)/tests/%,$(SOURCE_TESTS))

BUILD_OPT := --build-lib=$(BUILD_DIR)
BUILD_EXT_OPT := --build-lib=$(BUILD_DIR)
SDIST_OPT := --formats=gztar

ifdef PG_CONFIG
	BUILD_EXT_OPT += --pg-config=$(PG_CONFIG)
endif

VERSION := $(shell grep PSYCOPG_VERSION setup.py | head -1 | sed -e "s/.*'\(.*\)'/\1/")
SDIST := dist/psycopg2-$(VERSION).tar.gz

EASY_INSTALL = PYTHONPATH=$(ENV_LIB) $(ENV_BIN)/easy_install-$(PYTHON_VERSION) -d $(ENV_LIB) -s $(ENV_BIN)
EZ_SETUP = $(ENV_BIN)/ez_setup.py

.PHONY: env check clean

default: package

all: package sdist

package: $(PLATLIB) $(PURELIB)

docs: docs-html docs-txt

docs-html: doc/html/genindex.html

docs-txt: doc/psycopg2.txt

# for PyPI documentation
docs-zip: doc/docs.zip

sdist: $(SDIST)

# The environment is currently required to build the documentation.
# It is not clean by 'make clean'

env: easy_install
	mkdir -p $(ENV_BIN)
	mkdir -p $(ENV_LIB)
	$(EASY_INSTALL) docutils
	$(EASY_INSTALL) sphinx

easy_install: ez_setup
	PYTHONPATH=$(ENV_LIB) $(PYTHON) $(EZ_SETUP) -d $(ENV_LIB) -s $(ENV_BIN) setuptools

ez_setup:
	mkdir -p $(ENV_BIN)
	mkdir -p $(ENV_LIB)
	wget -O $(EZ_SETUP) http://peak.telecommunity.com/dist/ez_setup.py

check:
	PYTHONPATH=$(BUILD_DIR):$(PYTHONPATH) $(PYTHON) -c "from psycopg2 import tests; tests.unittest.main(defaultTest='tests.test_suite')" --verbose

testdb:
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


$(PLATLIB): $(SOURCE_C)
	$(PYTHON) setup.py build_ext $(BUILD_EXT_OPT)

$(PACKAGE)/%.py: lib/%.py
	$(PYTHON) setup.py build_py $(BUILD_OPT)
	touch $@

$(PACKAGE)/tests/%.py: tests/%.py
	$(PYTHON) setup.py build_py $(BUILD_OPT)
	touch $@

$(SDIST): docs MANIFEST $(SOURCE)
	$(PYTHON) setup.py sdist $(SDIST_OPT)

MANIFEST: MANIFEST.in $(SOURCE)
	# Run twice as MANIFEST.in includes MANIFEST
	$(PYTHON) setup.py sdist --manifest-only
	$(PYTHON) setup.py sdist --manifest-only

# docs depend on the build as it partly use introspection.
doc/html/genindex.html: $(PLATLIB) $(PURELIB) $(SOURCE_DOC)
	PYTHONPATH=$(ENV_LIB):$(BUILD_DIR) $(MAKE) SPHINXBUILD=$(ENV_BIN)/sphinx-build -C doc html

doc/psycopg2.txt: $(PLATLIB) $(PURELIB) $(SOURCE_DOC)
	PYTHONPATH=$(ENV_LIB):$(BUILD_DIR) $(MAKE) SPHINXBUILD=$(ENV_BIN)/sphinx-build -C doc text

doc/docs.zip: doc/html/genindex.html
	(cd doc/html && zip -r ../docs.zip *)

clean:
	rm -rf build MANIFEST
	$(MAKE) -C doc clean
