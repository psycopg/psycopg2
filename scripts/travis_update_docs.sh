#!/bin/bash

# Trigger a rebuild of the psycopg.org website to update the documentation.
# The script is meant to run by Travis CI.

set -euo pipefail

# The travis token can be set at https://github.com/psycopg/psycopg2/settings/secrets/actions
# and can be set on a selected branch only (which should match the DOC_BRANCH
# in the psycopg-website Makefile, or it won't refresh a thing).
if [ -z "${TRAVIS_TOKEN:-}" ]; then
    echo "skipping docs update: travis token not set" >&2
    exit 0
fi

echo "triggering psycopg-website rebuild" >&2
curl -s -X POST \
    -H "Content-Type: application/json" \
    -H "Accept: application/json" \
    -H "Travis-API-Version: 3" \
    -H "Authorization: token ${TRAVIS_TOKEN}" \
    -d "{\"request\": {\"branch\": \"${TRAVIS_BRANCH}\"}}" \
    https://api.travis-ci.com/repo/psycopg%2Fpsycopg-website/requests
