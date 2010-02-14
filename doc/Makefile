.PHONY: help clean html text doctest

docs: html text

check: doctest

help:
	cd src && $(MAKE) $@

html:
	cd src && $(MAKE) $@
	cp -r src/_build/html .

text:
	cd src && $(MAKE) $@
	cd src && tools/stitch_text.py index.rst _build/text > ../psycopg2.txt

doctest:
	cd src && $(MAKE) $@

clean:
	cd src && $(MAKE) $@
	rm -rf html psycopg2.txt
