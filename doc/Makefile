.PHONY: env help clean html package doctest

docs: html

check: doctest

# The environment is currently required to build the documentation.
# It is not clean by 'make clean'

PYTHON := python$(PYTHON_VERSION)
PYTHON_VERSION ?= $(shell $(PYTHON) -c 'import sys; print ("%d.%d" % sys.version_info[:2])')
BUILD_DIR = $(shell pwd)/../build/lib.$(PYTHON_VERSION)

SPHINXBUILD ?= $$(pwd)/env/bin/sphinx-build
SPHOPTS = SPHINXBUILD=$(SPHINXBUILD)

html: package src/sqlstate_errors.rst
	$(MAKE) $(SPHOPTS) -C src $@
	cp -r src/_build/html .

src/sqlstate_errors.rst: ../psycopg/sqlstate_errors.h $(BUILD_DIR)
	env/bin/python src/tools/make_sqlstate_docs.py $< > $@

$(BUILD_DIR):
	$(MAKE) PYTHON=$(PYTHON) -C .. package

doctest:
	$(MAKE) PYTHON=$(PYTHON) -C .. package
	$(MAKE) $(SPHOPTS) -C src $@

clean:
	$(MAKE) $(SPHOPTS) -C src $@
	rm -rf html src/sqlstate_errors.rst

env: requirements.txt
	virtualenv -p $(PYTHON) env
	./env/bin/pip install -r requirements.txt
	echo "$$(pwd)/../build/lib.$(PYTHON_VERSION)" \
		> env/lib/python$(PYTHON_VERSION)/site-packages/psycopg.pth
