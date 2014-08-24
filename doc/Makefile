.PHONY: env help clean html text doctest

docs: html text

check: doctest

# The environment is currently required to build the documentation.
# It is not clean by 'make clean'

help:
	$(MAKE) SPHINXBUILD=$$(pwd)/env/bin/sphinx-build -C src $@

html:
	$(MAKE) SPHINXBUILD=$$(pwd)/env/bin/sphinx-build -C src $@
	cp -r src/_build/html .

text:
	$(MAKE) SPHINXBUILD=$$(pwd)/env/bin/sphinx-build -C src $@
	cd src && tools/stitch_text.py index.rst _build/text > ../psycopg2.txt

doctest:
	$(MAKE) SPHINXBUILD=$$(pwd)/env/bin/sphinx-build -C src $@

clean:
	$(MAKE) SPHINXBUILD=$$(pwd)/env/bin/sphinx-build -C src $@
	rm -rf html psycopg2.txt

env: requirements.txt
	virtualenv env
	./env/bin/pip install -r requirements.txt
