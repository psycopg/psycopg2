/*-------------------------------------------------------------------------
 *
 * libpq-fe.h
 *	  This file contains definitions for structures and
 *	  externs for functions used by frontend postgres applications.
 *
 * Portions Copyright (c) 1996-2001, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef LIBPQ_FE_H
#define LIBPQ_FE_H

#ifdef __cplusplus
extern		"C"
{
#endif

#include <stdio.h>
#include <openssl/ssl.h>

/* postgres_ext.h defines the backend's externally visible types,
 * such as Oid.
 */
#include "postgres_ext.h"
#include "c.h"
#include "acPwcache.h"

/* Application-visible enum types */

	typedef enum
	{

		/*
		 * Although you may decide to change this list in some way, values
		 * which become unused should never be removed, nor should
		 * constants be redefined - that would break compatibility with
		 * existing code.
		 */
		CONNECTION_OK,
		CONNECTION_BAD,
        CONNECTION_TERM,        /* connection terminated by host */
		/* Non-blocking mode only below here */

		/*
		 * The existence of these should never be relied upon - they
		 * should only be used for user feedback or similar purposes.
		 */
		CONNECTION_STARTED,		/* Waiting for connection to be made.    */
		CONNECTION_MADE,		/* Connection OK; waiting to send.	     */
		CONNECTION_STARTUP,		/* Connection OK; use startup packet     */
		CONNECTION_HANDSHAKE,	/* Connection OK; use handshake protocol */
		CONNECTION_CLIENT_REQUEST, 	/* Connection OK; use handshake protocol */
		CONNECTION_CLIENT_RESPONSE,	/* Connection OK; use handshake protocol */
		CONNECTION_SERVER_REQUEST, 	/* Connection OK; use handshake protocol */
		CONNECTION_SERVER_RESPONSE,	/* Connection OK; use handshake protocol */
		CONNECTION_AWAITING_RESPONSE,	/* Waiting for a response from the
										 * postmaster.		  */
        CONNECTION_SSL_REQUEST,     /* Connection OK; use handshake protocol */
        CONNECTION_SSL_RESPONSE,    /* Connection OK; use handshake protocol */
		CONNECTION_SSL_CONNECTING,  /* Connection OK; use handshake protocol */
		CONNECTION_AUTH_OK,		/* Received authentication; waiting for
								 * backend startup. */
		CONNECTION_SETENV		/* Negotiating environment.    */
	} ConnStatusType;

	typedef enum
	{
		PGRES_POLLING_FAILED = 0,
		PGRES_POLLING_READING,	/* These two indicate that one may	  */
		PGRES_POLLING_WRITING,	/* use select before polling again.   */
		PGRES_POLLING_OK,
		PGRES_POLLING_ACTIVE	/* Can call poll function immediately. */
	} PostgresPollingStatusType;

	typedef enum
	{
		PGRES_EMPTY_QUERY = 0,
		PGRES_COMMAND_OK,		/* a query command that doesn't return
								 * anything was executed properly by the
								 * backend */
		PGRES_TUPLES_OK,		/* a query command that returns tuples was
								 * executed properly by the backend,
								 * PGresult contains the result tuples */
		PGRES_COPY_OUT,			/* Copy Out data transfer in progress */
		PGRES_COPY_IN,			/* Copy In data transfer in progress */
		PGRES_BAD_RESPONSE,		/* an unexpected response was recv'd from
								 * the backend */
		PGRES_NONFATAL_ERROR,
		PGRES_FATAL_ERROR,
		PGRES_FATAL_ERROR_TERM
	} ExecStatusType;

typedef enum
{
	PQSHOW_CONTEXT_NEVER,		/* never show CONTEXT field */
	PQSHOW_CONTEXT_ERRORS,		/* show CONTEXT for errors only (default) */
	PQSHOW_CONTEXT_ALWAYS		/* always show CONTEXT field */
} PGContextVisibility;

typedef enum
{
	PQERRORS_TERSE,				/* single-line error messages */
	PQERRORS_DEFAULT,			/* recommended style */
	PQERRORS_VERBOSE,			/* all the facts, ma'am */
	PQERRORS_SQLSTATE			/* only error severity and SQLSTATE code */
} PGVerbosity;

/* PGconn encapsulates a connection to the backend.
 * The contents of this struct are not supposed to be known to applications.
 */
	typedef struct pg_conn PGconn;

/* PGresult encapsulates the result of a query (or more precisely, of a single
 * SQL command --- a query string given to PQsendQuery can contain multiple
 * commands and thus return multiple PGresult objects).
 * The contents of this struct are not supposed to be known to applications.
 */
	typedef struct pg_result PGresult;

/* PGnotify represents the occurrence of a NOTIFY message.
 * Ideally this would be an opaque typedef, but it's so simple that it's
 * unlikely to change.
 * NOTE: in Postgres 6.4 and later, the be_pid is the notifying backend's,
 * whereas in earlier versions it was always your own backend's PID.
 */
	typedef struct pgNotify
	{
		char		relname[NAMEDATALEN];		/* name of relation
												 * containing data */
		int			be_pid;		/* process id of backend */
	} PGnotify;

/* PQnoticeProcessor is the function type for the notice-message callback.
 */
	typedef void (*PQnoticeProcessor) (void *arg, const char *message);

/* Print options for PQprint() */
	typedef char pqbool;

	typedef struct _PQprintOpt
	{
		pqbool		header;		/* print output field headings and row
								 * count */
		pqbool		align;		/* fill align the fields */
		pqbool		standard;	/* old brain dead format */
		pqbool		html3;		/* output html tables */
		pqbool		expanded;	/* expand tables */
		pqbool		pager;		/* use pager for output if needed */
		char	   *fieldSep;	/* field separator */
		char	   *tableOpt;	/* insert to HTML <table ...> */
		char	   *caption;	/* HTML <caption> */
		char	  **fieldName;	/* null terminated array of repalcement
								 * field names */
	} PQprintOpt;

/* ----------------
 * Structure for the conninfo parameter definitions returned by PQconndefaults
 *
 * All fields except "val" point at static strings which must not be altered.
 * "val" is either NULL or a malloc'd current-value string.  PQconninfoFree()
 * will release both the val strings and the PQconninfoOption array itself.
 * ----------------
 */
	typedef struct _PQconninfoOption
	{
		const char	   *keyword;	/* The keyword of the option			*/
		const char	   *envvar;		/* Fallback environment variable name	*/
		const char	   *compiled;	/* Fallback compiled in default value	*/
		char	   *val;		/* Option's current value, or NULL		 */
		const char	   *label;		/* Label for field in connect dialog	*/
		const char	   *dispchar;	/* Character to display for this field in
								 * a connect dialog. Values are: ""
								 * Display entered value as is "*"
								 * Password field - hide value "D"	Debug
								 * option - don't show by default */
		int			dispsize;	/* Field size in characters for dialog	*/
	} PQconninfoOption;

/* ----------------
 * PQArgBlock -- structure for PQfn() arguments
 * ----------------
 */
	typedef struct
	{
		int			len;
		int			isint;
		union
		{
			int		   *ptr;	/* can't use void (dec compiler barfs)	 */
			int			integer;
		}			u;
	} PQArgBlock;

/* ----------------
 * AddOpt -- Additional options passed by nzsql
 * ----------------
 */
typedef struct
{
	int         securityLevel;
	char        *caCertFile;
} AddOpt;

/* ----------------
 * Exported functions of libpq
 * ----------------
 */

/* ===	in fe-connect.c === */

	/* make a new client connection to the backend */

	/* Asynchronous (non-blocking) */
	extern PGconn *PQconnectStart(const char *conninfo);
	extern void    PQfreeconnection(PGconn *conn);
	extern PostgresPollingStatusType PQconnectPoll(PGconn *conn);
	/* Synchronous (blocking) */
	extern PGconn *PQconnectdb(const char *conninfo);
	extern PGconn *PQsetdbLogin(const char *pghost, const char *pgport,
				    const char *pgoptions, const char *pgtty,
				    const char *dbName, const char *login,
				    const char *pwd, const AddOpt* add_opt,
				    const bool quiet, const bool admin_mode,
				    const bool noPassword);
#define PQsetdb(M_PGHOST,M_PGPORT,M_PGOPT,M_PGTTY,M_DBNAME)  \
	PQsetdbLogin(M_PGHOST, M_PGPORT, M_PGOPT, M_PGTTY, M_DBNAME, NULL, NULL)

	extern PGconn *PQsetdbLoginTermOld(const char *pghost, const char *pgport,
                                       const char *pgoptions, const char *pgtty,
                                       const char *dbName,
                                       const char *login, const char *pwd, const int prevPid,
                                       const int secLevel, const char *caCertfile,
                                       const char *priorUser, const char *priorPwd,
                                       const bool quiet);

	/* close the current connection and free the PGconn data structure */
	extern void PQfinish(PGconn *conn);

	/* get info about connection options known to PQconnectdb */
	extern PQconninfoOption *PQconndefaults(void);

	/* free the data structure returned by PQconndefaults() */
	extern void PQconninfoFree(PQconninfoOption *connOptions);

	/*
	 * close the current connection and restablish a new one with the same
	 * parameters
	 */
	/* Asynchronous (non-blocking) */
	extern int	PQresetStart(PGconn *conn);
	extern PostgresPollingStatusType PQresetPoll(PGconn *conn);
	/* Synchronous (blocking) */
	extern void PQreset(PGconn *conn);

	/* issue a cancel request */
	extern int	PQrequestCancel(PGconn *conn);

	/* Accessor functions for PGconn objects */
	extern char *PQdb(const PGconn *conn);
	extern char *PQuser(const PGconn *conn);
	extern void  PQsetdbname(PGconn *conn, const char* dbname);
	extern void  PQsetusername(PGconn *conn, const char* username);
	extern char *PQpass(const PGconn *conn);
	extern char *PQhost(const PGconn *conn);
	extern char *PQport(const PGconn *conn);
	extern char *PQtty(const PGconn *conn);
	extern char *PQoptions(const PGconn *conn);
	extern ConnStatusType PQstatus(const PGconn *conn);
	extern const char *PQerrorMessage(const PGconn *conn);
    extern void PQresetErrorMessage(PGconn *conn);
	extern int	PQsocket(const PGconn *conn);
	extern int	PQbackendPID(const PGconn *conn);
	extern int	PQsetNzEncoding(PGconn *conn, const int nz_encoding);
    extern int  PQsetLoadReplayRegion(PGconn *conn, const int64 regionSize);
	extern int	PQclientEncoding(const PGconn *conn);
	extern int	PQsetClientEncoding(PGconn *conn, const char *encoding);
	/* Get the SSL structure associated with a connection */
	extern SSL  *PQgetssl(PGconn *conn);
	/* To check for a NULL return */
	const char* SSLcheckError();

	/* Enable/disable tracing */
	extern void PQtrace(PGconn *conn, FILE *debug_port);
	extern void PQuntrace(PGconn *conn);

	/* Override default notice processor */
	extern PQnoticeProcessor PQsetNoticeProcessor(PGconn *conn, PQnoticeProcessor proc, void *arg);

/* === in fe-exec.c === */

	/* Simple synchronous query */
	extern PGresult *PQexec(PGconn *conn, const char *query);
    extern PGresult *PQbatchexec(PGconn *conn, const char *query,
                                 int batch_rowset);
	extern PGnotify *PQnotifies(PGconn *conn);

	/* Interface for multiple-result or asynchronous queries */
	extern int	PQsendQuery(PGconn *conn, const char *query);
	extern PGresult *PQgetResult(PGconn *conn);

	/* Routines for managing an asychronous query */
	extern int	PQisBusy(PGconn *conn);
	extern int	PQconsumeInput(PGconn *conn);

	/* Routines for copy in/out */
	extern int	PQgetline(PGconn *conn, char *string, int length);
	extern int	PQputline(PGconn *conn, const char *string);
	extern int	PQgetlineAsync(PGconn *conn, char *buffer, int bufsize);
	extern int	PQputnbytes(PGconn *conn, const char *buffer, int nbytes);
	extern int	PQendcopy(PGconn *conn);

	/* Set blocking/nonblocking connection to the backend */
	extern int	PQsetnonblocking(PGconn *conn, int arg);
	extern int	PQisnonblocking(const PGconn *conn);

	/* Force the write buffer to be written (or at least try) */
	extern int	PQflush(PGconn *conn);

    /* Batching */
    extern void PQresetbatchdex(PGconn *conn);
    extern int  PQgetbatchdex(PGconn *conn);
    extern void PQincrementbatchdex(PGconn *conn);
    extern bool PQcommand_complete(PGconn *conn);

	/*
	 * "Fast path" interface --- not really recommended for application
	 * use
	 */
	extern PGresult *PQfn(PGconn *conn,
									  int fnid,
									  int *result_buf,
									  int *result_len,
									  int result_is_int,
									  const PQArgBlock *args,
									  int nargs);
    extern PGresult *PQset_plan_output_file(PGconn *conn,
                                            char *plan_output_file,
                                            bool is_dir);

	/* Accessor functions for PGresult objects */
	extern ExecStatusType PQresultStatus(const PGresult *res);
	extern const char *PQresStatus(ExecStatusType status);
	extern const char *PQresultErrorMessage(const PGresult *res);
	extern int	PQntuples(const PGresult *res);
    extern void PQsetntuples(PGresult *res, int ntups);
	extern int	PQnfields(const PGresult *res);
	extern int	PQbinaryTuples(const PGresult *res);
	extern char *PQfname(const PGresult *res, int field_num);
	extern int	PQfnumber(const PGresult *res, const char *field_name);
	extern Oid	PQftype(const PGresult *res, int field_num);
	extern int	PQfsize(const PGresult *res, int field_num);
	extern int	PQfmod(const PGresult *res, int field_num);
	extern char *PQcmdStatus(PGresult *res);
	extern const char *PQoidStatus(const PGresult *res);		/* old and ugly */
	extern Oid	PQoidValue(const PGresult *res);		/* new and improved */
	extern const char *PQcmdTuples(PGresult *res);
	extern char *PQgetvalue(const PGresult *res, int tup_num, int field_num);
	extern int	PQgetlength(const PGresult *res, int tup_num, int field_num);
	extern int	PQgetisnull(const PGresult *res, int tup_num, int field_num);
    extern void PQresult_inc_total_ntups(PGresult *res);
    extern uint64  PQresult_get_total_ntups(const PGresult *res);
    extern void PQresult_reset_ntups(PGresult *res);
    extern bool PQresult_is_batching(const PGresult *res);
    extern void PQresetcommandcomplete(PGconn *conn);
    extern void PQresetCancelPending(PGconn *conn);

	/* Delete a PGresult */
	extern void PQclear(PGresult *res);

	/*
	 * Make an empty PGresult with given status (some apps find this
	 * useful). If conn is not NULL and status indicates an error, the
	 * conn's errorMessage is copied.
	 */
	extern PGresult *PQmakeEmptyPGresult(PGconn *conn, ExecStatusType status);

        /* NETEZZA - add three functions to resolve undefines when linked with php-pgsql-4.3.9-3.6 (LAS4) */

        /* Quoting strings before inclusion in queries. */
        extern size_t PQescapeString(char *to, const char *from, size_t length);
        extern char *PQescapeIdentifier(PGconn *conn, const char *str, size_t len, bool as_ident);
        extern unsigned char *PQescapeBytea(const unsigned char *bintext, size_t binlen,
                                size_t *bytealen);
        extern unsigned char *PQunescapeBytea(const unsigned char *strtext,
                                size_t *retbuflen);

    /*
     * Dbos tuple mode
     */
    extern void SetDbosTupleHandler(int (*cbfun) (PGconn *conn));

    /*
     * Functions for get/se commandNumber
     */
    extern void PQsetCommandNumber(PGconn *conn, int cn);
    extern int PQgetCommandNumber(PGconn *conn);


/* === in fe-print.c === */

	extern void PQprint(FILE *fout,		/* output stream */
									const PGresult *res,
									const PQprintOpt *ps);		/* option structure */

	/*
	 * really old printing routines
	 */
	extern void PQdisplayTuples(const PGresult *res,
											FILE *fp,	/* where to send the
														 * output */
											int fillAlign,		/* pad the fields with
																 * spaces */
											const char *fieldSep,		/* field separator */
											int printHeader,	/* display headers? */
											int quiet);

	extern void PQprintTuples(const PGresult *res,
										  FILE *fout,	/* output stream */
										  int printAttName,		/* print attribute names */
										  int terseOutput,		/* delimiter bars */
										  int width);	/* width of column, if
														 * 0, use variable width */


/* === in fe-lobj.c === */

	/* Large-object access routines */
	extern int	lo_open(PGconn *conn, Oid lobjId, int mode);
	extern int	lo_close(PGconn *conn, int fd);
	extern int	lo_read(PGconn *conn, int fd, char *buf, size_t len);
	extern int	lo_write(PGconn *conn, int fd, char *buf, size_t len);
	extern int	lo_lseek(PGconn *conn, int fd, int offset, int whence);
	extern Oid	lo_creat(PGconn *conn, int mode);
	extern int	lo_tell(PGconn *conn, int fd);
	extern int	lo_unlink(PGconn *conn, Oid lobjId);
	extern Oid	lo_import(PGconn *conn, const char *filename);
	extern int	lo_export(PGconn *conn, Oid lobjId, const char *filename);

/* === in fe-misc.c === */

	/* Determine length of multibyte encoded char at *s */
	extern int	PQmblen(const unsigned char *s, int encoding);

	/* Determine display length of multibyte encoded char at *s */
	extern int	PQdsplen(const unsigned char *s, int encoding);

	/* Get encoding id from environment variable PGCLIENTENCODING */
	extern int	PQenv2encoding(void);

 /* === for arrow adbc: start === */
 /*
 * Note: The following functions will have incremental changes as and when required
 * in supporting ADBC drivers.
 */
    extern PGresult *PQprepare(PGconn *conn, const char *stmtName,
                               const char *query, int nParams,
                               const Oid *paramTypes);

    extern PGresult *PQexecParams(PGconn *conn,
                                  const char *command,
                                  int nParams,
                                  const Oid *paramTypes,
                                  const char *const *paramValues,
                                  const int *paramLengths,
                                  const int *paramFormats,
                                  int resultFormat);
    
    extern PGresult *PQexecPrepared(PGconn *conn,
	                                const char *stmtName,
	                                int nParams,
	                                const Oid *paramTypes,
	                                const char *const *paramValues,
	                                const int *paramLengths,
	                                const int *paramFormats,
	                                int resultFormat);
    
    extern PGresult *PQdescribePrepared(PGconn *conn, const char *stmt);

    extern int	PQsendQueryPrepared(PGconn *conn,
                                    const char *stmtName,
                                    int nParams,
                                    const Oid *paramTypes,
                                    const char *const *paramValues,
                                    const int *paramLengths,
                                    const int *paramFormats,
                                    int resultFormat);

    extern int PQsendQueryParams(PGconn *conn,
                                 const char *command,
                                 int nParams,
                                 const Oid *paramTypes,
                                 const char *const *paramValues,
                                 const int *paramLengths,
                                 const int *paramFormats,
                                 int resultFormat);

/* === for arrow adbc: end === */
                    
#ifdef __cplusplus
}

#endif

#endif	 /* LIBPQ_FE_H */
