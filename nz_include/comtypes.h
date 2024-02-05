// comtypes.h
//
// definitions of fundamental types shared across pg and nz
//
// This header file is included in many other include files system-wide, so
// any additions to it must be applicable to a very wide variety of
// situations.
//
// NOTE TO DEVELOPERS:
//
// Please be careful about what you add to this file. Changes to this file
// impact almost every other source file in the system.
//
// (C) Copyright IBM Corp. 2002, 2011  All Rights Reserved.

#ifndef COMTYPES_H_
#define COMTYPES_H_

#include <inttypes.h>

/*
 * :host64: When Oid was declared as unsigned int here we are allocation
 * always an array of 12 chars.
 * The value numeric_limits<unsigned int>::max() is: 4294967295. We might have
 * negative values, so:
 * - 1 for the sign,
 * - 10 for the number
 * - 1 from '\0'
 * TOTAL: 12.
 *
 * Now that we have unsigned long int we need to change this value.
 * - numeric_limits<unsigned long>::max() is: 18446744073709551615 ==> 20 (no sign)
 * - numeric_limits<long>::max() is:           9223372036854775807 ==> 19.(with sign)
 * - we need one for the sign (if signed)
 * - we need one for '\0'
 * TOTAL: 21.
 */

// :host64:
typedef unsigned int INTERNALOID;
typedef INTERNALOID Oid;

// :host64: we moved it from postgres_ext.h to have Oid and OID_MAX in one place
#define OID_MAX UINT_MAX

#define OIDMAXLENGTH 12 /* ((1)+(10)+(1)) */

// :host64: we want to add a standard way to print-scan basic types in Postgres
// in a sort of standard way. We use the same approach used by inttypes.h.

#define __PRI0id_PREFIX /* For 64-bit: "l" */

#define PRI0id __PRI0id_PREFIX "u"
// "PRI0id"

#define __PRIx_PREFIXZERO "8" /* For 64bit: "16" */

#define PRIx0id "0" __PRIx_PREFIXZERO __PRI0id_PREFIX "x"
#define PRIX0id "0" __PRIx_PREFIXZERO __PRI0id_PREFIX "X"
// "PRIx0id"

typedef uintptr_t Datum;

/*
 * We need a #define symbol for sizeof(Datum) for use in some #if tests.
 */
#ifdef __x86_64__
# define SIZEOF_DATUM 8
#else
# define SIZEOF_DATUM 4
#endif

/*
 * To stay backward compatible with older kits.
 * This is used for storing consts in Datum by value
 * for types other than INT8, at least currently.
 *
 */
enum
{
    LegacyCompatibleDatumSize = 4
};

#define SIZEOF_VOID_P SIZEOF_DATUM
#endif /* COMTYPES_H_ */
