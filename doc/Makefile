.PHONY: env help clean html doctest

docs: html

check: doctest

# The environment is currently required to build the documentation.
# It is not clean by 'make clean'

PYTHON := python$(PYTHON_VERSION)
PYTHON_VERSION ?= $(shell $(PYTHON) -c 'import sys; print ("%d.%d" % sys.version_info[:2])')

SPHINXBUILD ?= $$(pwd)/env/bin/sphinx-build
SPHOPTS = SPHINXBUILD=$(SPHINXBUILD)

html: src/sqlstate_errors.rst
	$(MAKE) PYTHON=$(PYTHON) -C .. package
	$(MAKE) $(SPHOPTS) -C src $@
	cp -r src/_build/html .

src/sqlstate_errors.rst: ../psycopg/sqlstate_errors.h
	env/bin/python src/tools/make_sqlstate_docs.py $< > $@

doctest:
	$(MAKE) PYTHON=$(PYTHON) -C .. package
	$(MAKE) $(SPHOPTS) -C src $@

upload:
	# this command requires ssh configured to the proper target
	tar czf - -C html . | ssh psycoweb tar xzvf - -C docs/current

clean:
	$(MAKE) $(SPHOPTS) -C src $@
	rm -rf html src/sqlstate_errors.rst

env: requirements.txt
	virtualenv -p $(PYTHON) env
	./env/bin/pip install -r requirements.txt
	echo "$$(pwd)/../build/lib.$(PYTHON_VERSION)" \
		> env/lib/python$(PYTHON_VERSION)/site-packages/psycopg.pth
