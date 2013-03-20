.PHONY: env help clean html text doctest

docs: html text

check: doctest

# The environment is currently required to build the documentation.
# It is not clean by 'make clean'

PYTHON := python$(PYTHON_VERSION)
PYTHON_VERSION ?= $(shell $(PYTHON) -c 'import sys; print ("%d.%d" % sys.version_info[:2])')

SPHOPTS=PYTHONPATH=$$(pwd)/../build/lib.$(PYTHON_VERSION)/ SPHINXBUILD=$$(pwd)/env/bin/sphinx-build

html:
	$(MAKE) PYTHON=$(PYTHON) -C .. package
	$(MAKE) $(SPHOPTS) -C src $@
	cp -r src/_build/html .

text:
	$(MAKE) PYTHON=$(PYTHON) -C .. package
	$(MAKE) $(SPHOPTS) -C src $@
	cd src && tools/stitch_text.py index.rst _build/text > ../psycopg2.txt

doctest:
	$(MAKE) PYTHON=$(PYTHON) -C .. package
	$(MAKE) $(SPHOPTS) -C src $@

upload:
	# this command requires ssh configured to the proper target
	tar czf - -C html . | ssh psycoweb tar xzvf - -C docs/current
	# this command requires a .pypirc with the right privileges
	python src/tools/pypi_docs_upload.py psycopg2 $$(pwd)/html

clean:
	$(MAKE) $(SPHOPTS) -C src $@
	rm -rf html psycopg2.txt

env: requirements.txt
	virtualenv env
	./env/bin/pip install -r requirements.txt
