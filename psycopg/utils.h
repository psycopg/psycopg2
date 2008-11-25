/* utils.h - miscellaneous utility functions
 *
 */

#ifndef PSYCOPG_UTILS_H
#define PSYCOPG_UTILS_H 1

#include "psycopg/config.h"
#include "psycopg/connection.h"


HIDDEN char *psycopg_internal_escape_string(connectionObject *conn, const char *string);

#endif /* !defined(PSYCOPG_UTILS_H) */


