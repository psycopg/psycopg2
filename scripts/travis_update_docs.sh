#!/bin/bash

# Trigger a rebuild of the psycopg.org website to update the documentation.
# The script is meant to run by Travis CI.

set -euo pipefail

# The travis token can be set at https://travis-ci.org/psycopg/psycopg2/settings
# and can be set on a selected branch only (which should match the DOC_BRANCH
# in the psycopg-website Makefile, or it won't refresh a thing).
if [ -z "${TRAVIS_TOKEN:-}" ]; then
    echo "skipping docs update: travis token not set" >&2
    exit 0
fi

# Avoid to rebuild the website for each matrix entry.
want_python="3.6"
if [ "${TRAVIS_PYTHON_VERSION}" != "${want_python}" ]; then
    echo "skipping docs update: only updated on Python ${want_python} build" >&2
    exit 0
fi

echo "triggering psycopg-website rebuild" >&2
curl -s -X POST \
    -H "Content-Type: application/json" \
    -H "Accept: application/json" \
    -H "Travis-API-Version: 3" \
    -H "Authorization: token ${TRAVIS_TOKEN}" \
    -d '{ "request": { "branch":"master" }}' \
    https://api.travis-ci.org/repo/psycopg%2Fpsycopg-website/requests
