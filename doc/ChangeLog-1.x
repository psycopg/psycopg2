2003-07-26  Federico Di Gregorio  <fog@debian.org>

	* Release 1.1.7.

	* ZPsycopgDA/db.py: added _cursor method that checks for self.db
	before returning a new cursor. Should fix problem reported with
	Zope 2.7.  

2003-07-23  Federico Di Gregorio  <fog@debian.org>

	* cursor.c: applied notify and fileno patch from Vsevolod Lobko.

2003-07-20  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (_mogrify_dict): applied (slightly modofied) patch from
	Tobias Sargeant: now .execute() accept not only dictionaries but
	every type that has a __getitem__ method.

2003-07-13  Federico Di Gregorio  <fog@debian.org>

	* Release 1.1.6.

	* cursor.c (psyco_curs_scroll): added scroll method, patch from
	Jason D.Hildebrand.

	* typemod.c (new_psyco_quotedstringobject): discard NUL characters
	(\0) in quoted strings (fix problem reported by Richard Taylor.)

2003-07-10  Federico Di Gregorio  <fog@debian.org>

	* Added python-taylor.txt in doc directory: very nice introduction
	to DBAPI programming by Richard Taylor.

2003-07-09  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (_psyco_curs_execute): another MT problem exposed and
	fixed by Sebastien Bigaret (self->keeper->status still LOCKED
	after a fatal error during PQexec call.)

2003-06-23  Federico Di Gregorio  <fog@debian.org>

	* Release 1.1.5.1.

	* ZPsycopgDA/db.py (DB.query): stupid error making ZPsycopgDA
	unusable fixed (else->except).

2003-06-22  Federico Di Gregorio  <fog@debian.org>

	* Release 1.1.5 candidate.

	* cursor.c (psyco_curs_copy_to): now any object with the write
	method can be used as a copy_to target.  

2003-06-20  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (psyco_curs_copy_from): applied patch to allow copy_to
	from any object having a "readline" attribute (patch from Lex
	Berezhny.) (psyco_curs_copy_from): another patch from Lex to make
	psycopg raise an error on COPY FROM errors. 

	* ZPsycopgDA/db.py (DB.query): if a query raise an exception,
	first self._abort() is called to rollback current
	"sub-transaction".  this is a backward-compatible change for
	people that think continuing to work in the same zope transaction
	after an exception is a Good Thing (TM).

	* finally updated check_types.expected. checked by hand the
	conversions work the right way.

	* doc/examples/work.py: fixed example. note that it is a long time
	(at least two releases) that psycopg does not END a transaction
	initiated explicitly by the user while in autocommit mode.

2003-06-19  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (_mogrify_dict): fixed dictionary mogrification (patch
	by Vsevolod Lobko.) (_psyco_curs_execute): fixed keeper status
	trashing problem by letting only one thread at time play with
	keeper->status (as suggested by Sebastien Bigaret.)

2003-05-07  Federico Di Gregorio  <fog@debian.org>

	* Release 1.1.4.

	* cursor.c: Added "statusmessage" attribute that holds the backend
	message (modified lots of functions, look for self->status).

2003-05-06  Federico Di Gregorio  <fog@debian.org>

	* typemod.c (new_psyco_datetimeobject): moved Py_INCREF into
	XXX_FromMx functions, to fix memory leak reported by Jim Crumpler.

2003-04-11  Federico Di Gregorio  <fog@debian.org>

	* module.h (PyObject_TypeCheck): fixed leak in python 2.1
	(Guido van Rossum).

2003-04-08  Federico Di Gregorio  <fog@debian.org>

	* buildtypes.py (basic_types): removed LXTEXT (never user, does
	not exists anymore.)

2003-04-07  Federico Di Gregorio  <fog@debian.org>

	* setup.py: added very lame setup.py script.

2003-04-02  Federico Di Gregorio  <fog@debian.org>

	* Release 1.3. 

	* psycopg.spec: Added (but modified) spec file by William
	K. Volkman (again, this change was lost somewhere in time...)

2003-04-01  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (_psyco_curs_execute): psycopg was reporting everything
	as IntegrityError; reported and fix suggested by Amin Abdulghani.

2003-03-21  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (psyco_curs_fetchone): debug statements sometimes made
	psycopg segfault: fixed by a patch by Ken Simpson.

2003-03-18  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (alloc_keeper): patch from Dieter Maurer to unlock GIL
	whaile calling PQconnectdb().

2003-03-05  Federico Di Gregorio  <fog@debian.org>

	* Release 1.1.2.

	* Applied cygwin patch from Hajime Nakagami.

2003-02-25  Federico Di Gregorio  <fog@debian.org>

	* Release 1.1.2pre1.
	
	* cursor.c: added .lastrowid attribute to cursors (lastoid is
	deprecated and will be removed sometime in the future.)

	* cursor.c (begin_pgconn): implemented various isolation levels
	(also, in abort_pgconn, commit_pgconn.)

	* Added keyword parameters to psycopg.connect(): all take strings
	(even port): database, host, port, user, password.
	
	* configure.in: fixed test for postgres version > 7.2.

	* cursor.c (_psyco_curs_execute): removed if on pgerr in default
	case (if we enter default pgerr can't be one of the cased ones.)
	Also applied slightly modified patch from  William K. Volkman.

2003-02-24  Federico Di Gregorio  <fog@debian.org>

	* Merged in changes from 1.0.15.1 (see below for merged
	ChangeLog.)

2003-02-14  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.15.1.

	* cursor.c (_mogrify_fmt): in some cases we where removing one
	character too much from the format string, resulting in BIG BAD
	BUG. <g> Fixed.

2003-02-13  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.15. <g>
	
	* connection.c (_psyco_conn_close): now call dispose_pgconn on all
	cursors, to make sure all phisical connections to the db are
	closed (problem first reported by Amin Abdulghani.)

	* DBAPI-2.0 fixed mainly due to Stuart Bishop:
	  - cursor.c (psyco_curs_setinputsizes): removed PARSEARGS, as
	    this method does nothing.
	  - cursor.c (psyco_curs_setoutputsize): .setoutputsize was
	    spelled .setoutputsizes! fixed. Also removed PARSEARGS, as this
	    method does nothing.

2003-02-12  Federico Di Gregorio  <fog@debian.org>

	* module.h (Dprintf): check on __APPLE__ to avoid variadic macros
	on macos x (as reported by Stuart Bishop, btw, why gcc seems to
	not support them on macos?)

	* cursor.c (_mogrify_fmt): non-alphabetic characters are dropped
	after the closing ")" until a real alphabetic, formatting one is
	found. (Fix bug reported by Randall Randall.)

2003-02-05  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c (psyco_INTERVAL_cast): patched again to take into
	account leading zeroes.

2003-02-02  Federico Di Gregorio  <fog@debian.org>

	* Makefile.pre.in: applied patch from Albert Chin-A-Young to
	define BLDSHARED.

	* README: added explicit permission to link with OpenSSL.

2003-01-30  Federico Di Gregorio  <fog@debian.org>

	* config.h.in: applied patch from Albert Chin-A-Young to fix
	asprintf prototype.

2003-01-29  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (_mogrify_seq): fixed little refcount leak, as
	suggested by Yves Bastide.

2003-01-24  Federico Di Gregorio  <fog@debian.org>

	* Merged-in changes from 1.0.14.2 (emacs diff mode is great..)

	* Release 1.0.14.2.

	* ZPsycopgDA/db.py (DB.query): back to allowing up to 1000 db
	errors before trying to reopen the connection by ourselves.
	
	* ZPsycopgDA/db.py: a false (None preferred, 0 allowed) max_rows
	value now means "fetch all results".

2003-01-22  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (psyco_curs_fetchone): fixed little memory leak
	reported by Dieter Maurer.

2003-01-20  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/db.py (DB.tables/columns): added registration with
	Zope's transaction machinery.

	* Release 1.0.14.1.

	* ZPsycopgDA/db.py: applied some fixes and cleanups by Dieter
	Maurer (serialization problem were no more correctly detected!)

	* Release 1.0.14.
	
	* Merged in 1.0.14.

	* Import of 1.1.1 done.
	
	* Moved everything to cvs HEAD.

2003-01-20  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/connectionAdd.dtml: fixed typo (thanks to Andrew
	Veitch.)

	* typeobj.c (psyco_INTERVAL_cast): applied patch from Karl Putland
	to fix problems with fractional seconds.

2002-12-03  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.14-pre2.

	* module.h: added macro for PyObject_TypeCheck if python version <2.2.

	* typeobj.c (psyco_DBAPITypeObject_coerce): added error message to
	coercion errors.

2002-12-02  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.14-pre1.

	* ZPsycopgDA/db.py (DB.sortKey): added sortKey().
	
	* ZPsycopgDA/DA.py: applied a patch that was lost on hard disk
	(sic), if you sent me a patch names psycopg-1.0.13.diff modifying
	DA.py imports and want your name here, send me an email. :)
	[btw, the patch fix the ImageFile import, getting it from Globals
	as it is right.]

	* typeobj.c (psyco_DBAPITypeObject_coerce): Fixed coerce segfault
	by checking explicitly for all the allowed types.

2002-11-25  Federico Di Gregorio  <fog@debian.org>

	* doc/examples/*.py: added .rollback() to all exceptions before
	deleteing the old table. 

	* cursor.c: Apllied patch from John Goerzen (fix memory leak in
	executemany).

2002-10-25  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.13.

	* connection.c (_psyco_conn_close): remove cursors from the list
	starting from last and moving backward (as suggested by Jeremy
	Hylton; this is not such a big gain because python lists are
	*linked* lists, but not removing the element 0 makes the code a
	little bit clear.)

	* cursor.c (_psyco_curs_execute): now IntegrityError is raised
	instead of ProgrammingError when adding NULL values to a non-NULL
	column (suggested by Edmund Lian) and on referential integrity
	violation (as per debian bug #165791.)

	* typeobj.c (psyco_DATE_cast): now we use 999999 instead of
	5867440 for very large (both signs) dates. This allow to re-insert
	the DateTime object into postgresql (suggested by Zahid Malik.) 

2002-09-13  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.12.

	* Removed code to support COPY FROM/TO, will be added to new 1.1
	branch to be released next week.

	* cursor.c (_mogrify_seq): Fixed memory leak reported by Menno
	Smits (values obtained by calling PySequence_GetItem are *new*
	references!)

2002-09-07  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (_psyco_curs_execute): Added skeleton to support COPY
	FROM/TO.

2002-09-06  Federico Di Gregorio  <fog@debian.org>

	* configure.in: if libcrypt can't be found we probably are on
	MacOS X: check for libcrypto, as suggested by Aparajita Fishman.

2002-09-03  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/db.py (DB.columns): Applied patch from Dieter Maurer
	to allow the DA-browser to work with mixed case table names.

2002-08-30  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/DA.py (cast_DateTime): Applied patch from Yury to fix
	timestamps (before they were returned with time always set to 0.)

2002-08-26  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.11.1 (to fix a %&£$"! bug in ZPsycopgDA not
	accepting psycopg 1.0.11 as a valid version.

	* Release 1.0.11.

2002-08-22  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.11pre2.

	* cursor.c (_psyco_curs_execute): fixed IntegrityError as reported
	by Andy Dustman. (psyco_curs_execute): converting TypeError to
	ProgrammingError on wrong number of % and/or aeguments. 

	* doc/examples/integrity.py: added example and check for
	IntegrityError.

2002-08-08  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.11pre1.

2002-08-06  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/DA.py (cast_DateTime): patched as suggested by Tom
	Jenkins; now it shouldwork with time zones too.

2002-08-01  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/DA.py (cast_DateTime): fixed problem with missing
	AM/PM, as reported by Tom Jenkins.

2002-07-23  Federico Di Gregorio  <fog@debian.org>

	* Fixed buglets reported by Mike Coleman.

2002-07-22  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.10.

2002-07-14  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.10pre2.
	
	* typeobj.c (psyco_LONGINTEGER_cast): fixed bad segfault by
	INCREFfing Py_None when it is the result of a NULL conversion.

2002-07-04  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.10pre1.
	
	* buildtypes.py (basic_types): added TIMESTAMPTZ to the types
	converted by the DATE builtin.
	
	* ZPsycopgDA/DA.py (Connection.connect): Added version check.

2002-07-03  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c (psyco_XXX_cast): fixed bug reported by multiple users
	by appliying Matt patch. 

2002-06-30  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/DA.py (Connection.set_type_casts): applied patch from
	Tom Jenkins to parse dates with TZ.

2002-06-20  Federico Di Gregorio  <fog@debian.org>

	* Preparing for release 1.0.9.

	* Makefile.pre.in (dist): now we really include psycopg.spec.

2002-06-17  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/db.py (_finish, _abort): fixed problem with
	connection left in invalid state by applying Tom Jenkins patch.

2002-06-06  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/db.py (DB._abort): fixed exception raising after an
	error in execute triggerer deletion of self.db.

2002-05-16  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (psyco_curs_fetchone): None values passed to the
	internal typecasters. 

	* typeobj.c: added management of None to all the builtin
	typecasters. 

2002-04-29  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/DA.py (cast_Time): applied 'seconds as a float' patch
	from Jelle.

2002-04-23  Federico Di Gregorio  <fog@debian.org>

        * Release 1.0.8.

	* Makefile.pre.in: we now include win32 related files in the
	distribution. 
	
	* connection.c (psyco_conn_destroy): fixed segfault reported by
	Scott Leerssen (we were double calling _psyco_conn_close().)

	* typemod.c (new_psyco_quotedstringobject): fixed memory stomping
	catched by assert(); thanks to Matt Hoskins for reporting this
	one.

2002-04-22  Federico Di Gregorio  <fog@debian.org>

	* configure.in: grmpf. we need a VERSION file for windows, we'll
	use it for configue and debian/rules too. 

	* Integrated win32 changes from Jason Erickson. Moved his
	Readme.txt to README.win32, removed VERSION and DATE, patched
	source where required. Renamed HISTORY to ChangeLog.win32, hoping
	Jason will start adding changes to the real ChangeLog file.

2002-04-07  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.7.1.
	
	* configure.in: fixed little bug as reported by ron.

2002-04-05  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.7?
	
	* typemod.c (new_psyco_bufferobject): fixed encoding problem (0xff
	now encoded as \377 and not \777.) Also encoding *all* chars as
	quoted octal values to prevent "Invalid UNICODE character sequence
	found" errors.

	* Release 1.0.7. (Real this time.) (Ok, it was a joke....)

2002-04-03  Federico Di Gregorio  <fog@debian.org>

	* configure.in: fixed problem with postgres versions in the format
	7.2.x (sic.)

	* connection.c (psyco_conn_destroy): moved most of the destroy
	stuff into its own function (_psyco_conn_close) and added a call
	to it from psyco_conn_close. This should fix the "psycopg does not
	release postgres connections on .close()" problem.

2002-03-29  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.7. Delayed.
	
	* buildtypes.py (basic_types): added TIMESTAMPTZ postgres type to
	the list of valid DATETIME types (incredible luck, no changes to
	the parse are needed!)

	* typeobj.c (psyco_DATE_cast): fixed wrong managment of sign in
	infinity.

2002-03-27  Federico Di Gregorio  <fog@debian.org>
	
	* configure.in (INSTALLOPTS): added AC_PROG_CPP test, now uses
	AC_TRY_CPP to test for _all_ required mx includes.

2002-03-19  Federico Di Gregorio  <fog@debian.org>

	* configure.in: added check for both pg_config.h and config.h to
	detect postgres version.

	* cursor.c: now None values are correctly handled when the format
	string is not %s but %d, etc.

2002-03-08  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/DA.py: added MessageDialog import suggested by
	Guido.

2002-03-07  Federico Di Gregorio  <fog@debian.org>

	* psycopg.spec: added RPM specs by William K. Volkman.

	* Release 1.0.6.
	
	* configure.in: imported changes to allow postgres 7.2 builds from
	unstable branch.

2002-03-04  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0.5.

	* applied table browser patch from Andy Dustman. 

2002-02-26  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c (psyco_DATE_cast): added management of infinity
	values, this can be done in a better way, by accessing the
	MaxDateTime and MinDateTime constants of the mx.DateTime module.

2002-02-20  Federico Di Gregorio  <fog@debian.org>

	* configure.in: Release 1.0.4.

2002-02-12  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/db.py (DB.columns): fixed select to reenable column
	expansion in table browsing.

	* ZPsycopgDA/__init__.py: removed code that made psycopg think
	double.  

2002-02-11  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (_mogrify_dict): removed Py_DECREF of Py_None,
	references returned by PyDict_Next() are borrowed (thanks to
	Michael Lausch for this one.)

2002-02-08  Federico Di Gregorio  <fog@debian.org>

	* A little bug slipped in ZPsycopgDA, releasing 1.0.3 immediately.

        * Release 1.0.2.
	
	* tests/check_types.py (TYPES): added check for hundredths of a
	second. 

2002-02-07  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c (psyco_INTERVAL_cast): patched to correct wrong
	interpretation of hundredths of a second (patch from
	A. R. Beresford, kudos!)

2002-01-31  Federico Di Gregorio  <fog@debian.org>

	* FAQ: added.

2002-01-16  Federico Di Gregorio  <fog@debian.org>

	* Preparing for release 1.0.1.
	
	* cursor.c (alloc_keeper): removed ALLOW_THREADS wrapper around
	PQconnectdb: libpq calls crypt() that is *not* reentrant.

2001-12-19  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c (psyco_DBAPITypeObject_cmp): added check to simply
	return false when two type objects are compared (type objects are
	meaned to be compared to integers!)

	* typeobj.c: fixed the memory leak reported by the guys at
	racemi, for real this time. (added about 5 DECREFS and 2 INCREFS,
	ouch!)
	
2001-12-17  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c (psyco_DBAPITypeObject_cmp): fixed memory leak by
	using PyTuple_GET_ITEM (we are sure the tuple has at least one
	element, we built it, after all...) (many thanks to Scott Leerssen
	for reporting the *exact line* for this one.)

2001-12-13  Federico Di Gregorio  <fog@debian.org>

	* cursor.c: fixed memory leak due to extra strdup (thanks
	to Leonardo Rochael Almeida.)

2001-11-14  Federico Di Gregorio  <fog@debian.org>

	* Release 1.0. 

	* doc/README: added explanation about guide work in progess but
	examples working.

	* debian/*: lots of changes to release 1.0 in debian too.

2001-11-12  Federico Di Gregorio  <fog@debian.org>

	* RELEASE-1.0: added release file, to celebrate 1.0.

	* tests/zope/typecheck.zexp: regression test on types for zope.

2001-11-11  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/DA.py (cast_Interval): removed typecast of interval
	into zope DateTime. intervals are reported as strings when using
	zope DateTime and as DateTimeDeltas when using mx.

2001-11-09  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c (psyco_INTERVAL_cast): complete rewrite of the
	interval parsing code. now we don't use sscanf anymore and all is
	done with custom code in a very tight and fast loop. 

2001-11-08  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/DA.py (Connection.set_type_casts): added mx INTERVAL
	type restore.

	* ZPsycopgDA/db.py (DB.query): now we return column names even if
	there are no rows in the result set. also, cleaned up a little bit
	the code.

2001-11-7  Federico Di Gregorio,  <fog@debian.org>

	* Makefile.pre.in: fixed small problem with zcat on True64 
	(thank you stefan.)

2001-11-06  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/db.py (DB.query): added fix for concurrent update
	from Chris Kratz.

2001-11-05  Federico Di Gregorio  <fog@debian.org>

	* cursor.c: now we include postgres.h if InvalidOid is still
	undefined after all other #includes.

	* README: clarified use of configure args related to python
	versions.

	* aclocal.m4: patched to work with symlinks installations (thanks
	to Stuart Bishop.)

	* cursor.c (_psyco_curs_execute): now reset the keeper's status to
	the old value and not to BEGIN (solve problem with autocommit not
	switching back.)

2001-11-01  Federico Di Gregorio  <fog@debian.org>

	* doc/examples/dt.py: added example on how to use the date and
	time constructors. 

	* Makefile.pre.in (dist-zope): removed dependencies on GNU install
	and tar commands. Also a little general cleanup on various targets.

	* ZPsycopgDA/DA.py: fixed mx.DateTime importing. 

2001-10-31  Federico Di Gregorio  <fog@debian.org>

	* typemod.c (psyco_xxxFromMx): fixed bug in argument parsing (we
	weren't usigng the right type object.) 

	* aclocal.m4: now builds OPT and LDFLAGS on the values of the env
	variables instead of overwriting them.

	* Makefile.pre.in (CFLAGS): removed -Wall, you can add it back at
	compile time with OPT="-Wall" ./configure ...

	* Setup.in (OPT): removed -Wall.

2001-10-30  Michele Comitini <mcm@initd.net>

	* module.h: ANSI C compatibility patch from Daniel Plagge.
	
2001-10-30  Federico Di Gregorio  <fog@debian.org>

	* README: added common building problems and solutions.

	* configure.in: removed check for install command, already done by
	james's aclocal.m4 for python. removed install-sh. removed -s from
	INSTALLOPTS.

2001-10-29  Federico Di Gregorio  <fog@debian.org>

	* Makefile.pre.in (dist): removed examples/ directory from
	distribution. 

	* merge with cvs head. preparing to fork again on PSYCOPG-1-0 (i
	admit BRANCH_1_0 was quite a silly name.)

	* doc/examples/usercast.py: now works. 

	* connection.c (curs_rollbackall): fixed little bug (exposed by
	the deadlock below) by changing KEEPER_READY to KEEPER_READY.

	* doc/examples/commit.py: deadlock problem solved, was the
	example script, _not_ psycopg. pew... :)

	* examples/*: removed the examples moved to doc/examples/.
	
	* doc/examples/commit.py,dictfetch.py: moved from examples/ and
	changed to work for 1.0. unfortunately commit.py locks psycopg!!!

2001-10-24  Federico Di Gregorio  <fog@debian.org>

	* modified all files neede for the 1.0 release.

	* configure.in (MXFLAGS): removed electric fence support.

	* Makefile.pre.in (dist): now we remove CVS working files before
	packing the tarball.

        * tests: files in this directory are not coding examples, but
	regression tests. we need a sufficient number of tests to follow
	every single code path in psycopg at least once. first test is
	about datatypes.
	
	* doc/examples: moved new example code to examples directory, old
	tests and code samples will stay in examples/ until the manual will
	be finished.

2001-10-16  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c (psyco_INTERVAL_cast): completely revised interval
	casting code. (psyco_TIME_cast): we use the unix epoch when the
	date is undefined. 

	* cursor.c (psyco_curs_executemany): modified sanity check to
	accept sequences of tuples too and not just dictionaries.

2001-10-15  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c (psyco_INTERVAL_cast): fixed bug caused by wrong
	parsing on '1 day' (no hours, minutes and seconds.)

2001-10-15  Michele Comitini  <mcm@initd.net>

	* cursor.c (_execute): use the correct cast functions even on
	retrival of binary cursors.

2001-10-12  Federico Di Gregorio  <fog@debian.org>

	* typemod.c (new_psyco_bufferobject): space not quoted anymore,
	smarter formula to calculate realloc size.

	* cursor.c (psyco_curs_fetchone): removed static tuple (using
	static variable in multithreaded code is *crazy*, why did i do it? 
	who knows...)

	* typeobj.c (psyco_init_types): exports the binary converter (will
	be used in cursor.c:_execute.)

	* typeobj.h: added export of psyco_binary_cast object.

2001-10-05  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (_psyco_curs_execute): added missing Py_XDECREF on
	casts list.

	* Makefile.pre.in (dist): added install-sh file to the
	distribution. 

	* replaced PyMem_DEL with PyObject_Del where necessary.
	
	* connection.c (psyco_conn_destroy): added missing
	pthread_mutex_destroy on keeper lock.

2001-10-01  Michele Comitini  <mcm@initd.net>

	* typemod.c(new_psyco_bufferobject()): using unsigned char for
	binary objects to avoid too many chars escaped.  A quick and
	simple formula to avoid memory wasting and too much reallocating
	for the converted object.  Needs _testing_, but it is faster.

	* cursor.c: #include <postgres.h>

	* module.h: now debugging should be active only when asked by
	./configure --enable-devel
	
2001-09-29  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (new_psyco_cursobject): added locking of connection,
	still unsure if necessary.

2001-09-26  Federico Di Gregorio  <fog@debian.org>

	* configure.in: changed DEBUG into PSYCOTIC_DEBUG, to allow other
	includes (postgres.h) to use the former. better compiler checks:
	inline, ansi, gcc specific extensions. removed MXMODULE: we don't
	need it anymore.

	* general #include cleanup, should compile on MacOS X too.

	* typeobj.c (psyco_DATE_cast): uses sscanf. should be faster too. 
	(psyco_TIME_cast): dixit.

	* applied patch from Daniel Plagge (SUN cc changes.)
	
2001-09-22  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/db.py (DB._finish, DB._begin): fix for the 
	self.db == None problem.

2001-09-19  Michele Comitini  <mcm@initd.net>

	* typemod.c (new_psyco_bufferobject): better memory managment
	(now it allocates only needed space dinamically).

	* typeobj.c (psyco_BINARY_cast): ripped a useless check, now
	it assumes that binary streams come out from the db correctly
	escaped.  Should be a lot faster.

2001-09-18  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c (psyco_INTERVAL_cast): fixed interval conversion
	(hours were incorrectly converted into seconds.)

2001-09-17  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (_mogrify_seq, _mogrify_dict): added check for None
	value and conversion of None -> NULL (fixes bug reported by Hamish
	Lawson.)

2001-09-12  Federico Di Gregorio  <fog@debian.org>

	* module.c: added handles to new date and time conversion
	functions (see below.)

	* typemod.c (psyco_XXXFromMx): added conversion functions that
	simply wrap the mxDateTime objects instead of creating
	them. DBAPI-2.0 extension, off-curse. 

2001-09-10  Federico Di Gregorio  <fog@debian.org>

	* buildtypes.py: solved hidden bug by changing from dictionary to
	list, to maintain ordering of types. sometimes (and just
	sometimes) the type definitions were printed unsorted, resulting
	is psycopg initializing the type system using the type objects in
	the wrong order. you were getting float values from an int4
	column? be happy, this is now fixed... 

	* cursor.c (psyco_curs_lastoid): added method to get oid of the
	last inserted row (it was sooo easy, it even works...) 

2001-09-08  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c (psyco_INTERVAL_cast): added casting function for the
	postgres INTERVAL and TINTERVAL types (create a DateTimeDelta
	object.)  

2001-09-05  Federico Di Gregorio  <fog@debian.org>

	* cursor.c: moved all calls to begin_pgconn to a single call in
	_psyco_curs_execute, to leave the connection in a not-idle status
	after a commit or a rollback. this should free a lot of resources
	on the backend side. kudos to the webware-discuss mailing list
	members and to Clark C. Evans who suggested a nice solution.
	
	* connection.c (curs_rollbackall, curs_commitall): removed calls
	to begin_pgconn, see above. 

	* module.c (initpsycopg): cleaned up mxDateTime importing; we now
	use the right function from mxDateTime.h. Is not necessary anymore
	to include our own mx headers. This makes psycopg to depend on
	mxDateTime >= 2.0.0.

2001-09-04  Federico Di Gregorio  <fog@debian.org>

	* doc/*.tex: added documentation directory and skeleton of the
	psycopg guide. 

2001-09-03  Federico Di Gregorio  <fog@debian.org>

	* merged in changes from HEAD (mostly mcm fixes to binary
	objects.)

	* preparing for release 0.99.6.

2001-09-03  Michele Comitini  <mcm@initd.net>

	* typemod.c: much faster Binary encoding routine.
	
	* typeobj.c: much faster Binary decoding routine.	

2001-08-28  Michele Comitini  <mcm@initd.net>

	* typemod.c: Working binary object to feed data to bytea type
	fields.

	* typeobj.c: Added BINARY typecast to extract data from
	bytea type fields.

	* cursor.c: Added handling for SQL binary  cursors.

2001-08-3  Michele Comitini <mcm@initd.net>

	* cursor.c: fixed DATESTYLE problem thanx to Steve Drees.

2001-07-26  Federico Di Gregorio  <fog@debian.org>

	* Makefile.pre.in: applied change suggested by Stefan H. Holek to
	clobber and distclean targets.

2001-07-23  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/db.py: fixed little bugs exposed by multiple select
	changes, not we correctly import ListType and we don't override
	the type() function with a variable. 

2001-07-17  Federico Di Gregorio  <fog@debian.org>

	* configure.in: Release 0.99.5.

2001-07-12  Federico Di Gregorio  <fog@debian.org>

	* debian/* fixed some little packaging problems.

2001-07-11  Federico Di Gregorio  <fog@debian.org>

	* cursor.c, typeobj.c: removed some Py_INCREF on PyDict_SetItem
	keys and values to avoid memory leaks.

2001-07-03  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (_mogrify_dict): added dictionary mogrification: all
	Strings in the dictionary are translated into QuotedStrings. it
	even works... (_mogrify_seq): added sequence mogrification and
	code to automagically mogrify all strings passed to .execute(). 

2001-07-02  Federico Di Gregorio  <fog@debian.org>

	* Release 0.99.4.

	* typemod.c: added QuotedString class and methods.

	* module.c: added QuotedString method to module psycopg.

	* typemod.c: changed Binary objects into something usefull. now
	the buffer object quotes the input by translatin every char into
	its octal representation. this consumes 4x memory but guarantees
	that even binary data containing '\0' can go into the Binary
	object. 

	* typemod.h: added definition of QuotedString object.
	
2001-06-28  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/db.py, ZPsycopgDA/DABase.py: applied patch sent by
	yury to fix little buglet. 

2001-06-22  Federico Di Gregorio  <fog@debian.org>

	* Release 0.99.3.
	
	* connection.c (new_psyco_connobject): now we strdup dsn, as a fix
	for the problem reported by Jack Moffitt.

	* Ok, this will be the stable branch from now on...

	* Merged in stuff from 0.99.3. About to re-branch with a better
	name (BRANCH_1_0)

2001-06-20  Federico Di Gregorio  <fog@debian.org>

	* Release 0.99.3. Showstoppers for 1.0 are:
	    - documentation
	    - mxDateTime module loading
	    - bug reported by Yury.
	
	* Integrated patches from Michele:
	    - searching for libcrypt in configure now works
	    - removed memory leak in asprintf.c

2001-06-15  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/__init__.py (initialize): applied patch from Jelle to
	resolve problem with Zope 2.4.0a1.

2001-06-14  Federico Di Gregorio  <fog@debian.org>

	* configure.in: added code to check for missing functions (only
	asprintf at now.)

	* asprintf.c: added compatibility code for oses that does not have
	the asprintf() function.

2001-06-10  Federico Di Gregorio  <fog@debian.org>

	* Branched PSYCOPG_0_99_3. Development will continue on the cvs
	HEAD, final adjustements and bugfixing should go to this newly
	created branch.

2001-06-08  Michele Comitini  <mcm@initd.net>

	* ZPsycopgDA/DA.py: DateTime casts simplified and corrected
	as suggested by Yury.

2001-06-05  Federico Di Gregorio  <fog@debian.org>

	* Release 0.99.2.

	* Makefile.pre.in (dist): added typemod.h and typemod.c to
	distribution.
	
	* cursor.c (commit_pgconn, abort_pgconn, begin_pgconn): resolved
	segfault reported by Andre by changing PyErr_SetString invokations
	into pgconn_set_critical. the problem was that the python
	interpreter simply segfaults when we touch its internal data (like
	exception message) inside an ALLOW_THREADS wrapper.

	* now that we are 100% DBAPI-2.0 compliant is time for the
	one-dot-o release (at last!) Para-pa-pa! This one is tagged
	PSYCOPG_0_99_1 but you can call it 1.0pre1, if you better like. 
	(A very long text just to say 'Release 0.99.1')

	* typemod.[ch]: to reach complete DBAPI-2.0 compliance we
	introduce some new objects returned by the constructors Date(),
	Time(), Binary(), etc. Those objects are module-to-database only,
	the type system still takes care of the database-to-python
	conversion.

2001-06-01  Federico Di Gregorio  <fog@debian.org>

	* Release 0.5.5.

	* module.h: better error message when trying to commit on a
	cursor derived from serialized connection.
	
	* ZPsycopgDA/db.py (DB.close): now self.cursor is set to None when
	the connection is closed.

	* module.c (initpsycopg): added missing (sic) DBAPI module
	parameters (paramstyle, apilevel, threadsafety, etc...)

2001-05-24  Michele Comitini  <mcm@initd.net>

	* ZPsycopgDA: Support for Zope's internal DateTime, option
	to leave mxDateTime is available on the management interface so
	to switch with little effort :).

	* cursor.c: more aggressive cleanup of postgres results
	to avoid the risk of memory leaking.

	* typeobj.c, connection.c: deleted some Py_INCREF which
	wasted memory.

2001-05-18  Federico Di Gregorio  <fog@debian.org>

	* Release 0.5.4.

2001-05-17  Michele Comitini  <mcm@initd.net>

	* ZPsycopgDA/db.py: The connection closed by the management
	interface of zope now raises error instead of reopening itself.

	* cursor.c (psyco_curs_close):  does not try to free the cursor
	list, as it caused a segfault on subsequent operations on the same
	cursor.

2001-05-07  Federico Di Gregorio  <fog@debian.org>

	* Release 0.5.3.
	
	* Merged in changes from me and mcm.

2001-05-06  Michele Comitini  <mcm@initd.net>

	* ZPsycopgDA/db.py (DB.close): Fixes a bug report by Andre
	Shubert, which was still open since there was a tiny typo in
	method definition.

	* ZPsycopgDA/DA.py (Connection.sql_quote__): overriding standard
	sql_quote__ method to provide correct quoting (thank to Philip
	Mayers and Casey Duncan for this bug report).

2001-05-04  Federico Di Gregorio  <fog@debian.org>

	* ZPsycopgDA/db.py: added .close() method (as suffested by Andre
	Schubert.)

2001-05-04  Michele Comitini  <mcm@initd.net>

	* module.h: working on a closed object now raises an
	InterfaceError.

	* ZPsycopgDA/db.py: fixed problems with dead connections detection.

	* ZPsycopgDA/__init__.py: corrected SOFTWARE_HOME bug for zope
	icon.

2001-05-04  Federico Di Gregorio  <fog@debian.org>

	* examples/thread_test.py: now that the serialization bug is
	fixed, it is clear that thread_test.py is bugged! added a commit()
	after the creation of the first table to avoid loosing it on the
	exception raised by the CREATE of an existing table_b.

2001-05-03  Federico Di Gregorio  <fog@debian.org>

	* connection.c (psyco_conn_cursor): reverted to old locking
	policy, the new caused a nasty deadlock. apparently the multiple
	connection problem has been solved as a side-effect of the other
	fixes... (?!)

	* module.h: removes stdkeeper field from connobject, we don't need
	it anymore.

	* cursor.c (dispose_pgconn): now sets self->keeper to NULL to
	avoid decrementing the keeper refcnt two times when the cursor is
	first closed and then destroyed.

	* connection.c (psyco_conn_cursor): fixed little bug in cursor
	creation: now the connection is locked for the entire duration of
	the cursor creation, to avoid a new cursor to be created with a
	new keeper due to a delay in assigning the stdmanager cursor.

	* cursor.c: added calls to pgconn_set_critical() and to
	EXC_IFCRITICAL() where we expect problems. Still segfaults but at
	least raise an exception...
	
	* cursor.c (psyco_curs_autocommit): added exception if the
	cursor's keeper is shared between more than 1 cursor.

	* module.h (EXC_IFCRITICAL): added this macro that call
	pgconn_resolve_critical) on critical errors.

	* cursor.c (alloc_keeper): added check for pgres == NULL. 

	* cursor.c (psyco_curs_destroy): merged psyco_curs_destroy() and
	psyco_curs_close(): now both call _psyco_curs_close() and destroy
	does only some extra cleanup.

2001-05-03  Michele Comitini  <mcm@initd.net>

	* ZPsycopgDA/db.py: Some cleanup to bring the zope product up to
	date with the python module.  Some bugs found thanks to Andre
	Schubert.  Now the ZDA should rely on the new serialized version
	of psycopg.

	* cursor.c: while looking for problems in the ZDA some come out
	here, with the inability to handle dropping connection correctly.
	This leads to segfaults and is not fixed yet for lack of time.
	Some problems found in cursors not willing to share the same
	connection even if they should.  Hopefully it should be fixed
	soon.

2001-04-26  Federico Di Gregorio  <fog@debian.org>

	* fixed bug reported by Andre Schubert by adding a new cast
	function for long integers (int8 postgresql type.) at now they are
	converted to python LongIntegers: not sure f simply convert to
	floats.

	* michele applied patch from Ivo van der Wijk to make zpsycopgda
	behave better when INSTANCE_HOME != SOFTWARE_HOME.

	* cursor.c (_psyco_curs_execute): also fill the 'columns' field.

	* module.h: added a 'columns' field to cursobject, to better
	support the new dictionary fetch functions (dictfetchone(),
	dictfetchmany(), dictfetchall().)

	* cursor.c: added the afore-mentioned functions (function names
	are not definitive, they will follow decisions on the DBAPI SIG.)

2001-04-03  Federico Di Gregorio  <fog@debian.org>

	* Release 0.5.1.

	* mcm fixed a nasty bug by correcting a typo in module.h.

2001-03-30  Federico Di Gregorio  <fog@debian.org>

	* module.c (psyco_connect): added `serialized' named argument to
	the .connect() method (takes 1 or 0 and initialize the connection
	to the right serialization state.)

	* Makefile.pre.in (dist): fixed little bug, a missing -f argument
	to rm.

	* examples/thread_test.py: removed all extension cruft.

	* examples/thread_test_x.py: this one uses extensions like the
	per-cursor commit, autocommit, etc.

	* README (psycopg): added explanation on how .serialize() works. 

	* connection.c (psyco_conn_serialize): added cursor serialization
	and .serialize() method on the connection object. now we are
	definitely DBAPI-2.0 compliant.

2001-03-20  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (_psyco_curs_execute): replaced some fields in
	description with None, as suggested on the DB-SIG ML.

	* something like one hundred of little changes to allow cursors
	share the same postgres connection. added connkeeper object and
	pthread mutexes (both in connobject and connkeeper.) apparently it
	works. this one will be 0.5.0, i think.

2001-03-19  Michele Comitini  <mcm@initd.net>

	* cursor.c: added mutexes, they do not interact well with python
	threads :(.

2001-03-16  Michele Comitini  <mcm@initd.net>

	* ZPsycopgDA/db.py (ZDA): some fixes in table browsing.

2001-03-16  Federico Di Gregorio  <fog@debian.org>

	* suite/tables.postgresql (TABLE_DESCRIPTIONS): fixed some typos
	introduced by copying by hand the type values from pg_type.h.

	* suite/*: added some (badly) structured code to test for
	DBAPI-2.0 compliance.
       
	* cursor.c (pgconn_notice_callback): now the NOTICE processor only
	prints NOTICEs when psycopg has been compiled with the
	--enable-devel switch. 

	* connection.c: removed 'autocommit' attribute, now is a method as
	specified in the DBAPI-2.0 document.

2001-03-15  Federico Di Gregorio  <fog@debian.org>

	* connection.c (curs_commitall): splitted for cycle in two to
	avoid the "bad snapshot" problem.

2001-03-14  Federico Di Gregorio  <fog@debian.org>

	* Release 0.4.6.
	
	* cursor.c (_psyco_curs_execute): fixed nasty bug, there was an
	free(query) left from before the execute/callproc split.

	* Preparing for 0.4.6.

2001-03-13  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (psyco_curs_execute): fixed some memory leaks in
	argument parsing (the query string was not free()ed.)
	(psyco_curs_callproc): implemented callproc() method on cursors.
	(_psyco_curs_execute): this is the function that does the real
	stuff for callproc() and execute().
	(pgconn_notice_*): added translation of notices into python
	exceptions (do we really want that?) 

	* configure.in: removed some cruft (old comments and strncasecmp()
	check)

2001-03-12  Federico Di Gregorio  <fog@debian.org>

	* examples/thread_test.py: added moronic argument parsing: now you
	can give the dsn string on the command line... :(

	* Release 0.4.5.

2001-03-10  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (request_pgconn): added code to set datestyle to ISO on
	new connections (many thanks to Yury <yura@vpcit.ru> for the code,
	i changed it just a little bit to raise an exception on error.)

2001-03-09  Federico Di Gregorio  <fog@debian.org>

	* Release 0.4.4.

	* ZPsycopgDA/db.py: michele fixed a nasty bug here. 

2001-03-08  Federico Di Gregorio  <fog@debian.org>

	* Release 0.4.3.

2001-03-07  Federico Di Gregorio  <fog@debian.org>

	* Makefile.pre.in (dist): typeobj_builtins.c included for people
	without pg_type.h. if you encounter type-casting problems like
	results cast to the wrong type, simply "rm typeobj_builtins.c" and
	rebuild.

	* typeobj.c (psyco_*_cast): removed RETURNIFNULL() macro from all
	the builtin casting functions. (psyco_STRING_cast) does not create
	a new string anymore, simply Py_INCREF its argument and return it.

	* cursor.c (psyco_curs_fetchone): removed strdup() call. added
	PQgetisnull() test to differentiate between real NULLs and empty
	strings.

	* Removed cursor.py (mcm, put tests in examples) and fixed some
	typos in the dtml code.

2001-03-04  Michele Comitini  <mcm@initd.net>

	* examples/commit_test.py: Modifications to test argument passing
	and string substitution to cursor functions, nothing more.

	* ZPsycopgDA/db.py: now it exploits some of the good features of
	the psycopg driver, such as connection reusage and type
	comparison.  Code is smaller although it handles (and
	reports) errors much better.

	* cursor.c: corrected a bug that left a closed cursor in the
	cursor list of the connection.  Now cursors are removed from the
	lists either when they are close or when they are destroyed.
	Better connection (TCP) error reporting and handling.


2001-03-02  Federico Di Gregorio  <fog@debian.org>

	* examples commit_test.py: added code to test autocommit.
	
	* examples/thread_test.py (ab_select): modified select thread to
	test autocommit mode.

	* Release 0.4.1.
	
	* module.h, connection.c, cursor.c: added autocommit support.

2001-02-28  Federico Di Gregorio  <fog@debian.org>

	* Release 0.4.

2001-02-27  Michele Comitini  <mcm@initd.net>

	* cursor.py: cut some unuseful code in psyco_curs_fetchmany() and
	psyco_curs_fetchall() inserted an assert in case someting goes
	wrong.

2001-02-27  Federico Di Gregorio  <fog@debian.org>

	* debian/*: various changes to build both the python module and
	the zope db adapter in different packages (respectively
	python-psycopg and zope-psycopgda.)  

	* examples/type_test.py: better and more modular tests. 

	* typeobj.c: added DATE, TIME, DATETIME, BOOLEAN, BINARY and ROWID
	types. (RETURNIFNULL) added NULL-test to builtin conversion
	functions (using the RETURNIFNULL macro.)

2001-02-26  Federico Di Gregorio  <fog@debian.org>

	* releasing 0.3 (added NEWS file.)

2001-02-26  Michele Comitini  <mcm@initd.net>

	* cursor.c: fetchmany() some cleanup done.

	* ZPsycopgDA/db.py, ZPsycopgDA/__init__.py, : fixes to make the
	ZDA work some way.  WARNING WARNING WARNING the zda is still
	alpha code, but we need some feed back on it so please give it
	a try.
	
2001-02-26  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c (psyco_STRING_cast): fixed bad bad bad bug. we
	returned the string without coping it and the type-system was more
	than happy to Py_DECREF() it and trash the whole system. fixed at
	last!

	* module.h (Dprintf): added pid to every Dprintf() call, to
	facilitate multi-threaded debug.

2001-02-26  Michele Comitini  <mcm@initd.net>

	* module.c: added code so that DateTime package need not to be
	loaded to have mxDateTime.  This should avoid clashing with
	DateTime from the zope distribution.

	* cursor.c: setting error message in fetchmany when no more tuples
	are left. This has to be fixed in fetch and fetchall to.

2001-02-26  Federico Di Gregorio  <fog@debian.org>

	* configure.in: stepped up version to 0.3, ready to release
	tomorrow morning. added check for path to DateTime module. 

	* examples/usercast_test.py: generate some random boxes and
	points, select the boxes with at least one point inside and print
	them converting the PostgeSQL output using a user-specified cast
	object. nice. 

2001-02-24  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (psyco_curs_fetchone): now an error in the python
	callback when typecasting results raise the correct exception.

	* typeobj.c (psyco_DBAPITypeObject_call): removed extra Py_INCREF().

2001-02-23  Federico Di Gregorio  <fog@debian.org>

	* replaced every single instance of the string 'pgpy' with 'psyco'
	(this was part of the general cleanup.)
	
	* type_test.py: added this little test program to the distribution
	(use the new_type() method to create new instances of the type
	objects.)  

	* typeobj.c: general cleanup. fixed some bugs related to
	refcounting (again!)

	* cursor.c: general cleanup. (request_pgconn) simplified by adding
	a support function (_extract_pgconn.)

	* connection.c: general cleanup. replaced some ifs with asserts()
	in utility functions when errors depend on programming errors and
	not on runtime exceptions. (pgpy_conn_destroy) fixed little bug
	when deleting available connections from the list.

	* module.h: general cleanup.

	* typeobj.h: general cleanup, better comments, made some function
	declarations extern. 

	* module.c: general cleanup, double-checked every function for
	memory leaks. (pgpy_connect) removed unused variable 'connection'.

2001-02-22  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c: fixed lots of bugs, added NUMBER type object. now the
	basic tests in type_test.py work pretty well.

	* cursor.c (pgpy_curs_fetchmany): fixed little bug, fetchmany()
	reported one less row than available.

	* fixed lots of bugs in typeobj.c, typeobj.h, cursor.c. apparently
	now the type system works. it is time to clean up things a little
	bit.

2001-02-21  Federico Di Gregorio  <fog@debian.org>

	* typeobj.c: separated type objects stuff from module.c
	
	* typeobj.h: separated type objects stuff from module.h 

2001-02-19  Federico Di Gregorio  <fog@debian.org>

	* cursor.c (pgpy_curs_fetchmany): now check size and adjust it to
	be lesser or equal than the nuber of available rows.

2001-02-18  Michele Comitini  <mcm@initd.net>

	* module.c, module.h: added optional args maxconn and minconn to
	connection functions

	* cursor.c: better error checking in request_pgconn.

	* connection.c: changed new_connect_obj to take as optional args
	maxconn and minconn. Added the corresponding ro attributes to
	connection objects.

	* cursor.py: added some code to stress test cursor reusage.

	* cursor.c: some fixes on closed cursors.

	* connection.c: corrections on some assert calls.

2001-02-16  Federico Di Gregorio  <fog@debian.org>

	* configure.in: added --enable-priofile sqitch. changed VERSION to
	0.2: preparing for a new release.

	* cursor.c: added a couple of asserts.

2001-02-16  Michele Comitini  <mcm@initd.net>

	* cursor.c, connection.c: fixed the assert problem: assert must
	take just the value to be tested! no assignemente must be done in
	the argument of assert() otherwise is wiped when NDEBUG is set.

	* module.h: some syntax error fixed.  Error in allocating a tuple
	corrected in macro DBAPITypeObject_NEW().
	
	* module.c: pgpy_DBAPITypeObject_init() is not declared static anymore.

	* cursor.c: executemany() now does not create and destroy tuples
	for each list item, so it is much faster.

2001-02-14  Michele Comitini  <mcm@initd.net>

	* cursor.c:  added again Py_DECREF on the cpcon after disposing
	it.  assert() with -DNDEBUG makes the driver segfault while it
	should not.
	

2001-02-13  Federico Di Gregorio  <fog@debian.org>

	* some of the memory leak were memprof errors, bleah. resumed some
	old code, fixed segfault, fixed other bugs, improved speed. almost
	ready for a new release.
	
	* connection.c (pgpy_conn_destroy): replaced some impossible ifs
	with aseert()s.

	* cursor.c (pgpy_curs_close): added Py_DECREF() to
	self->descritpion to prevent a memory leak after an execute().

	* connection.c (pgpy_conn_destroy): always access first element of
	lists inside for cycles because removing items from the list makes
	higher indices invalid.

	* cursor.c (dispose_pgconn): fixed memory leak, there was a
	missing Py_DECREF() after the addition of the C object wrapping
	the postgresql connection to the list of available connections.

	* cursor.c (dispose_pgconn): fixed another memory leak: an
	orphaned cursor should call PQfinish() on its postgresql
	connection because it has no python connection to give the
	postgresql ine back.

	* cursor.c (pgpy_curs_execute): added Py_DECREF() of description
	tuple after adding it to self->description. this one fixes the
	execute() memory leak.
	
	* cursor.c (pgpy_curs_fetchall): added missing Py_DECREF() on row
	data (obtained from fetchone().) this fixes the last memory leak.
	(thread_test.py now runs without leaking memory!)

2001-02-12  Federico Di Gregorio  <fog@debian.org>

	* INSTALL: removed wok cruft from head of this file.

	* debian/rules: debianized the sources. python-psycopg is about to
	enter debian. mxDateTime header locally included until the
	maintainer of python-mxdatetime includes them in his package
	(where they do belong.)

	* autogen.sh: added option --dont-run-configure. 

2001-02-09  Federico Di Gregorio  <fog@debian.org>

	* module.c (initpsycopg): changed name of init function to match
	new module name (also changed all the exception definitions.)

	* README: updated psycopg description (we have a new name!)

	* Ready for 0.1 release.

2001-02-07  Michele Comitini  <mcm@initd.net>

	* cursor.c: now executemany takes sequences and not just
	tuples 

2001-02-07  Federico Di Gregorio  <fog@debian.org>

	* Makefile.pre.in: now dist target includes test programs
	(thread_test.py) and README and INSTALL files. 

	* configure.in: changed --with-devel to --enable-devel. little
	cosmetical fixes to the option management.
	
	* connection.c, module.c, cursor.c, module.h: removed 'postgres/'
	from #include directive. it is ./configure task to find the right
	directory.

	* thread_test.py: added thread testing program.

2001-02-07  Michele Comitini  <mcm@initd.net>

	* cursor.c: added code to allow threads during PQexec() calls.
	
	* cursor.c: added begin_pgconn to rollback() and commit()
	so that the cursror is not in autocommit mode.

	* cursor.c: added rollback() and commit() methods to cursor
	objects.


2001-02-07  Federico Di Gregorio  <fog@debian.org>

	* connection.c (pgpy_conn_destroy): always delete item at index
	0 and not i (because items shift in the list while deleting and
	accessing items at len(list)/2 segfaults.)

2001-02-07  Michele Comitini  <mcm@initd.net>

	* connection.c: added some more checking to avoid
	clearing of already cleared pgresults.  Calling curs_closall()
	in conn_destroy() since cursors have to live even without
	their parent connection, otherwise explicit deletion of
	object referencing to those cursors can cause arbitrary code
	to be executed.

	* cursor.c: some more checking to avoid trying to close
	already close pgconnections.

2001-02-06  Federico Di Gregorio  <fog@debian.org> 

	* Makefile.pre.in (CFLAGS): added -Wall to catch bad programming
	habits. 

	* cursor.c, connection.c: lots of fixes to the destroy stuff. now
	all the cursor are destroyed *before* the connection goes away.

	* cursor.c (request_pgconn): another idiot error done by not
	replacing dsn with owner_conn->dsn. fixed.
	(dispose_pgconn): commented if to guarantee that the connection is
	returned to the pool of available connections.

	* merged changes done by mcm.

	* cursor.c: general cleanup and better debugging/error
	messages. changed xxx_conn into xxx_pgconn where still
	missing. some pretty big changes to the way pgconn_request()
	allocates new connections.

	* connection.c: removed all 'register' integers. obsolete, gcc
	does a much better job optimizing cycles than a programmer
	specifying how to use registers. 

	* module.h: some general cleanup and better definition of DPrintf
	macro. now the DEBUG variable can be specified at configure time by
	the --with-devel switch to ./configure.

2001-02-02  Michele Comitini  <mcm@initd.net>

	* cursor.c (Repository): Added functions for managing a connection
	pool. Segfaults.

	* configure.in (Repository): removed check for mxdatetime headers.

2001-01-24  Federico Di Gregorio  <fog@debian.org>

	* first checkout from shinning new init.d cvs.

	* autotoolized build system. note that the mx headers are missing
	from the cvs, you should get them someplace else (this is the
	right way to do it, just require the headers in the configure
	script.)

2001-01-21  Michele Comitini  <mcm@initd.net>

	* cursor.c (Repository): commit, abort, begin functions now check
	the right exit status of the command.

	* connection.c (Repository): working commit() and rollback()
	methods.

2001-01-20  Michele Comitini  <mcm@initd.net>

	* module.h (Repository): added member to cursor struct to handle
	queries without output tuples.

	* cursor.c (Repository): new working methods: executemany,
	fetchone, fetchmany, fetchall.

2001-01-18  Michele Comitini  <mcm@initd.net>

	* cursor.c (Repository): close working. destroy calling close.
	close frees pg structures correctly.

	* connection.c (Repository): close method working.  destroy seems
	working.

2001-01-17  Michele Comitini  <mcm@initd.net>

	* cursor.c (Repository): now each python cursor has its own
	connection.  Each cursor works in a transaction block.

	* connection.c (Repository): added cursor list to connection
	object

2001-01-14  Michele Comitini  <mcm@initd.net>

	* cursor.c (Repository): Beginning of code to implement cursor
	functionalities as specified in DBA API 2.0, through the use of
	transactions not cursors.

	* connection.c (Repository): Added some error checking code for pg
	connection (will be moved to cursor?).

2001-01-13  Michele Comitini  <mcm@initd.net>

	* connection.c (Repository): Added error checking in connection
	code to fail if connection to the db could not be opened.

	* module.h (Repository): New macro to help creating
	DBAPITypeObjects.

	* module.c (Repository): DBAPITypeObject __cmp__ function is now
	very simplified using recursion.

	* module.h (Repository): "DBAPIObject" changed to
	"DBAPITypeObject".

	* module.c (Repository): Fixes for coerce function of DBAPIObjects
	by Federico Di Gregorio <fog@initd.net>.
	(Repository): Clean up and better naming for DBAPITypeObjects.

2001-01-08  Michele Comitini  <mcm@initd.net>

	* module.c (Repository): Corrected the exception hierarcy

	* connection.c (Repository): Begun to use the connection objects
	of libpq

2001-01-07  Michele Comitini  <mcm@initd.net>

	* module.c (Repository): Added the Date/Time functions.

2001-01-06  Michele Comitini  <mcm@initd.net>

	* cursor.c (Repository): Skeleton of cursor interface.  All
	methods and attributes of cursor objects are now available
	in python.  They do nothing now.

2001-01-05  Michele Comitini  <mcm@initd.net>

	* module.c (Repository): Test version; module loaded with 
	exception defined.
	
2001-01-05  Michele Comitini  <mcm@initd.net>

	* Setup.in (Repository): Setup file.

	* Makefile.pre.in (Repository): from the python source.

2001-01-05  Michele Comitini  <mcm@initd.net>

	* module.c: Written some code for defining exceptions.
	
	* module.h: Static variable for exceptions.
	
2001-01-04  Michele Comitini  <mcm@initd.net>

	* Changelog: pre-release just a few prototypes to get started.
	

