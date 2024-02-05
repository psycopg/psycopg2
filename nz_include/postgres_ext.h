#ifndef POSTGRES_EXT_H
#define POSTGRES_EXT_H
/// @file postgres_ext.h
/// @brief Definitions shared between postgres and other parts of NPS
///
//////////////////////////////////////////////////////////////////////
/// Copyright (c) 2008 Netezza Corporation.
//////////////////////////////////////////////////////////////////////
///
/// Description:
///
///
///
/// postgres_ext.h
///
///       This file contains declarations of things that are visible everywhere
///  in PostgreSQL *and* are visible to clients of frontend interface libraries.
///    For example, the Oid type is part of the API of libpq and other libraries.
///
///       Declarations which are specific to a particular interface should
///    go in the header file for that interface (such as libpq-fe.h).    This
///    file is only for fundamental Postgres declarations.
///
///       User-written C functions don't count as "external to Postgres."
///    Those function much as local modifications to the backend itself, and
///    use header files that are otherwise internal to Postgres to interface
///    with the backend.
///
///
///
/// dd - 2008-02-07
//////////////////////////////////////////////////////////////////////

#include "limits.h"

// :host64:
#include "comtypes.h"

#define InvalidOid ((Oid) 0)

/*
 * NAMEDATALEN is the max length for system identifiers (e.g. table names,
 * attribute names, function names, etc.)
 *
 * NOTE that databases with different NAMEDATALEN's cannot interoperate!
 */
#define NAMEDATALEN         256 // supported length for database object names is 128 UTF-8 characters
#define MAX_IDENTIFIER      128 // supported number of characters for database object names
#define MAX_CFIELD          512 // supported number of characters for client info field names
#define MAX_BYTES_PER_NCHAR 4
#define CFIELDDATALEN \
 (MAX_CFIELD * MAX_BYTES_PER_NCHAR) // supported length for client information fields
#define MAX_SYSOID           200000
#define NUM_BASE_VIEW_ATTRS  6
#define NUM_ROW_SECURE_ATTRS 4
#define MAX_PASSWORD_LENGTH  2048 // maximum password length

/*
 * StrNCpy
 *    Like standard library function strncpy(), except that result string
 *    is guaranteed to be null-terminated --- that is, at most N-1 bytes
 *    of the source string will be kept.
 *    Also, the macro returns no result (too hard to do that without
 *    evaluating the arguments multiple times, which seems worse).
 *
 *    BTW: when you need to copy a non-null-terminated string (like a text
 *    datum) and add a null, do not do it with StrNCpy(..., len+1). That
 *    might seem to work, but it fetches one byte more than there is in the
 *    text object. One fine day you'll have a SIGSEGV because there isn't
 *    another byte before the end of memory. Don't laugh, we've had real
 *    live bug reports from real live users over exactly this mistake.
 *    Do it honestly with "memcpy(dst,src,len); dst[len] = '\0';", instead.
 */
#ifdef StrNCpy
# undef StrNCpy
#endif
#define StrNCpy(dst, src, len) \
 do {                          \
  char* _dst = (dst);          \
  int _len = (len);            \
                               \
  if (_len > 0) {              \
   strncpy(_dst, (src), _len); \
   _dst[_len - 1] = '\0';      \
  }                            \
 } while (0)

#endif
