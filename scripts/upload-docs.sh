#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DOCDIR="$DIR/../doc"

# this command requires ssh configured to the proper target
tar czf - -C "$DOCDIR/html" . | ssh psycoweb tar xzvf - -C docs/current

# download the script to upload the docs to PyPI
test -e "$DIR/pypi_docs_upload.py" \
    || wget -O "$DIR/pypi_docs_upload.py" \
        https://gist.githubusercontent.com/dvarrazzo/dac46237070d69dbc075/raw

# this command requires a ~/.pypirc with the right privileges
python "$DIR/pypi_docs_upload.py" psycopg2 "$DOCDIR/html"
