This file is a simple placeholder for forcing the appveyor build cache
to invalidate itself since appveyor.yml changes more frequently then
the cache needs updating.  Note, the versions list here can be
different than what is indicated in appveyor.yml.

To invalidate the cache, update this file and check it into git.


Currently used modules built in the cache:

OpenSSL
        Version: 1.1.1d

PostgreSQL
        Version: 11.5


NOTE: to zap the cache manually you can also use:

	curl -X DELETE -H "Authorization: Bearer $APPVEYOR_TOKEN" -H "Content-Type: application/json" https://ci.appveyor.com/api/projects/psycopg/psycopg2/buildcache

with the token from https://ci.appveyor.com/api-token
