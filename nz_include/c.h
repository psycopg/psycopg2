/*-------------------------------------------------------------------------
 *
 * c.h
 *      Fundamental C definitions.  This is included by every .c file in
 *      PostgreSQL (via either postgres.h or postgres_fe.h, as appropriate).
 *
 *      Note that the definitions here are not intended to be exposed to
 *      clients of the frontend interface libraries --- so we don't worry
 *      much about polluting the namespace with lots of stuff...
 *
 *
 * Portions Copyright (c) 1996-2001, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *-------------------------------------------------------------------------
 */
/*
 *----------------------------------------------------------------
 *     TABLE OF CONTENTS
 *
 *        When adding stuff to this file, please try to put stuff
 *        into the relevant section, or add new sections as appropriate.
 *
 *      section    description
 *      -------    ------------------------------------------------
 *        0)        config.h and standard system headers
 *        1)        hacks to cope with non-ANSI C compilers
 *        2)        bool, true, false, TRUE, FALSE, NULL
 *        3)        standard system types
 *        4)        IsValid macros for system types
 *        5)        offsetof, lengthof, endof, alignment
 *        6)        widely useful macros
 *        7)        random stuff
 *        8)        system-specific hacks
 *        9)        assertions

 *
 * NOTE: since this file is included by both frontend and backend modules,
 * it's almost certainly wrong to put an "extern" declaration here.
 * typedefs and macros are the kind of thing that might go here.
 *
 *----------------------------------------------------------------
 */
#ifndef C_H
#define C_H

#include "postgres_ext.h"

/* For C++, we want all the ISO C standard macros as well.  */
#ifndef __STDC_LIMIT_MACROS
# define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
# define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_FORMAT_MACROS
# define __STDC_FORMAT_MACROS
#endif

#include "postgres_ext.h"

/* We have to include stdlib.h here because it defines many of these macros
   on some platforms, and we only want our definitions used if stdlib.h doesn't
   have its own.  The same goes for stddef and stdarg if present.
*/

#include "pg_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#ifdef STRING_H_WITH_STRINGS_H
# include <strings.h>
#endif

#include <errno.h>
#ifdef __CYGWIN__
# include <sys/fcntl.h> /* ensure O_BINARY is available */
#endif
#ifdef HAVE_SUPPORTDEFS_H
# include <SupportDefs.h>
#endif

#ifndef WIN32
# include "bitarch.h"
#endif

/* ----------------------------------------------------------------
 *                Section 1: hacks to cope with non-ANSI C compilers
 *
 * type prefixes (const, signed, volatile, inline) are now handled in config.h.
 * ----------------------------------------------------------------
 */

/*
 * CppAsString
 *        Convert the argument to a string, usingclause the C preprocessor.
 * CppConcat
 *        Concatenate two arguments together, usingclause the C preprocessor.
 *
 * Note: the standard Autoconf macro AC_C_STRINGIZE actually only checks
 * whether #identifier works, but if we have that we likely have ## too.
 */
#if defined(HAVE_STRINGIZE)

# define CppAsString(identifier) # identifier
# define CppConcat(x, y)         x##y

#else /* !HAVE_STRINGIZE */

# define CppAsString(identifier) "identifier"

/*
 * CppIdentity -- On Reiser based cpp's this is used to concatenate
 *        two tokens.  That is
 *                CppIdentity(A)B ==> AB
 *        We renamed it to _private_CppIdentity because it should not
 *        be referenced outside this file.  On other cpp's it
 *        produces  A  B.
 */
# define _priv_CppIdentity(x)    x
# define CppConcat(x, y)         _priv_CppIdentity(x) y

#endif /* !HAVE_STRINGIZE */

/*
 * dummyret is used to set return values in macros that use ?: to make
 * assignments.  gcc wants these to be void, other compilers like char
 */
#ifdef __GNUC__ /* GNU cc */
# define dummyret void
#else
# define dummyret char
#endif

/*
 * Append PG_USED_FOR_ASSERTS_ONLY to definitions of variables that are only
 * used in assert-enabled builds, to avoid compiler warnings about unused
 * variables in assert-disabled builds.
 */
#ifdef USE_ASSERT_CHECKING
# define PG_USED_FOR_ASSERTS_ONLY
#else
# define PG_USED_FOR_ASSERTS_ONLY pg_attribute_unused()
#endif

#define PG_PRINTF_ATTRIBUTE printf
/* GCC and XLC support format attributes */
#if defined(__GNUC__) || defined(__IBMC__)
# define pg_attribute_format_arg(a) __attribute__((format_arg(a)))
# define pg_attribute_printf(f, a)  __attribute__((format(PG_PRINTF_ATTRIBUTE, f, a)))
#else
# define pg_attribute_format_arg(a)
# define pg_attribute_printf(f, a)
#endif

/*
 * Hints to the compiler about the likelihood of a branch. Both likely() and
 * unlikely() return the boolean value of the contained expression.
 *
 * These should only be used sparingly, in very hot code paths. It's very easy
 * to mis-estimate likelihoods.
 */
#if __GNUC__ >= 3
# define likely(x)   __builtin_expect((x) != 0, 1)
# define unlikely(x) __builtin_expect((x) != 0, 0)
#else
# define likely(x)   ((x) != 0)
# define unlikely(x) ((x) != 0)
#endif

/* ----------------------------------------------------------------
 *                Section 2:    bool, true, false, TRUE, FALSE, NULL
 * ----------------------------------------------------------------
 */
/*
 * bool
 *        Boolean value, either true or false.
 *
 */

/* BeOS defines bool already, but the compiler chokes on the
 * #ifndef unless we wrap it in this check.
 */
#ifndef __BEOS__

# ifndef __cplusplus
#  ifndef bool_defined
#   define bool_defined
typedef char bool;
#  endif /* ndef bool */

#  ifndef true
#   define true 1
#  endif

#  ifndef false
#   define false 0
#  endif
# endif /* not C++ */

#endif /* __BEOS__ */

typedef bool *BoolPtr;

#ifndef TRUE
# define TRUE 1
#endif

#ifndef FALSE
# define FALSE 0
#endif

/*
 * NULL
 *        Null pointer.
 */
#ifndef NULL
# define NULL ((void *) 0)
#endif

/* ----------------------------------------------------------------
 *                Section 3:    standard system types
 * ----------------------------------------------------------------
 */

/*
 * Pointer
 *        Variable holding address of any memory resident object.
 *
 *        XXX Pointer arithmetic is done with this, so it can't be void *
 *        under "true" ANSI compilers.
 */
typedef char *Pointer;

/*
 * intN
 *        Signed integer, EXACTLY N BITS IN SIZE,
 *        used for numerical computations and the
 *        frontend/backend protocol.
 */
#ifndef __BEOS__ /* this shouldn't be required, but is is! */
# if !AIX
typedef signed char int8;   /* == 8 bits */
typedef signed short int16; /* == 16 bits */
typedef signed int int32;   /* == 32 bits */
# endif
#endif /* __BEOS__ */

/*
 * uintN
 *        Unsigned integer, EXACTLY N BITS IN SIZE,
 *        used for numerical computations and the
 *        frontend/backend protocol.
 */
#ifndef __BEOS__               /* this shouldn't be required, but is is! */
typedef unsigned char uint8;   /* == 8 bits */
typedef unsigned short uint16; /* == 16 bits */
typedef unsigned int uint32;   /* == 32 bits */
#endif                         /* __BEOS__ */

/*
 * boolN
 *        Boolean value, AT LEAST N BITS IN SIZE.
 */
typedef uint8 bool8;   /* >= 8 bits */
typedef uint16 bool16; /* >= 16 bits */
typedef uint32 bool32; /* >= 32 bits */

/*
 * bitsN
 *        Unit of bitwise operation, AT LEAST N BITS IN SIZE.
 */
typedef uint8 bits8;   /* >= 8 bits */
typedef uint16 bits16; /* >= 16 bits */
typedef uint32 bits32; /* >= 32 bits */

/*
 * wordN
 *        Unit of storage, AT LEAST N BITS IN SIZE,
 *        used to fetch/store data.
 */
typedef uint8 word8;   /* >= 8 bits */
typedef uint16 word16; /* >= 16 bits */
typedef uint32 word32; /* >= 32 bits */

/*
 * floatN
 *        Floating point number, AT LEAST N BITS IN SIZE,
 *        used for numerical computations.
 *
 *        Since sizeof(floatN) may be > sizeof(char *), always pass
 *        floatN by reference.
 *
 * XXX: these typedefs are now deprecated in favor of float4 and float8.
 * They will eventually go away.
 */
typedef float float32data;
typedef double float64data;
typedef float *float32;
typedef double *float64;

/*
 * 64-bit integers -- use standard types for these
 */
typedef int64_t int64;
typedef uint64_t uint64;

#define INT64CONST(x)  INT64_C(x)
#define UINT64CONST(x) UINT64_C(x)

/*
 * Size
 *        Size of any memory resident object, as returned by sizeof.
 */
typedef size_t Size;

/*
 * Index
 *        Index into any memory resident array.
 *
 * Note:
 *        Indices are non negative.
 */
typedef unsigned int Index;

/*
 * Offset
 *        Offset into any memory resident array.
 *
 * Note:
 *        This differs from an Index in that an Index is always
 *        non negative, whereas Offset may be negative.
 */
typedef signed int Offset;

/*
 * Common Postgres datatype names (as used in the catalogs)
 */
typedef char int1;
typedef int16 int2;
typedef int32 int4;
typedef float float4;
typedef double float8;

typedef int8 timestamp;
typedef int4 date;
typedef int4 abstime;

/*
 * Oid, RegProcedure, TransactionId, CommandId
 */

/* typedef Oid is in postgres_ext.h */

/* unfortunately, both regproc and RegProcedure are used */
typedef Oid regproc;
typedef Oid RegProcedure;

typedef uint32 TransactionId;

#define InvalidTransactionId 0

typedef uint32 CommandId;

#define FirstCommandId 0

/*
 * Array indexing support
 */
#define MAXDIM 6
typedef struct
{
    int indx[MAXDIM];
} IntArrayMAXDIM; // NZ: blast name conflict

/* ----------------
 * Variable-length datatypes all share the 'struct varlena' header.
 *
 * NOTE: for TOASTable types, this is an oversimplification, since the value
 * may be compressed or moved out-of-line.  However datatype-specific
 * routines are mostly content to deal with de-TOASTed values only, and of
 * course client-side routines should never see a TOASTed value.  See
 * postgres.h for details of the TOASTed form.
 * ----------------
 */
//  The following items must be in sync:
//  class NzVarlena in nde/misc/geninl.h
//  class NzVarlena in nde/expr/pgwrap.h
//  struct varlena in pg/include/c.h
//  struct varattrib in pg/include/postgres.h
//  struct varattrib in udx-source/udx-impls/v2/UDX_Varargs.cpp
struct varlena
{
    int32 vl_len;
    int32 vl_fixedlen;
    union
    {
        char *vl_ptr;
        int64 vl_align; // allow for 64bit pointers
    };
    char vl_dat[1];
};

/*
 * These widely-used datatypes are just a varlena header and the data bytes.
 * There is no terminating null or anything like that --- the data length is
 * always VARSIZE(ptr) - VARHDRSZ.
 */
typedef struct varlena bytea;
typedef struct varlena text;
typedef struct varlena BpChar;  /* blank-padded char, ie SQL char(n) */
typedef struct varlena VarChar; /* var-length char, ie SQL varchar(n) */
typedef struct varlena VarBin;  /* var-length binary, ie SQL varbin(n) */
typedef struct varlena *JsonPtr;
typedef struct _Jsonb *JsonbPtr;
typedef struct _JsonPath *JsonPathPtr;
/*
 * Fixed-length array types (these are not varlena's!)
 */

typedef int2 int2vector[INDEX_MAX_KEYS];
typedef Oid oidvector[INDEX_MAX_KEYS];

/*
 * We want NameData to have length NAMEDATALEN and int alignment,
 * because that's how the data type 'name' is defined in pg_type.
 * Use a union to make sure the compiler agrees.
 */
typedef union nameData
{
    char data[NAMEDATALEN];
    int alignmentDummy;
} NameData;
typedef NameData *Name;

#define NameStr(name) ((name).data)

/*
 * stdint.h limits aren't guaranteed to be present and aren't guaranteed to
 * have compatible types with our fixed width types. So just define our own.
 */
#define PG_INT8_MIN   (-0x7F - 1)
#define PG_INT8_MAX   (0x7F)
#define PG_UINT8_MAX  (0xFF)
#define PG_INT16_MIN  (-0x7FFF - 1)
#define PG_INT16_MAX  (0x7FFF)
#define PG_UINT16_MAX (0xFFFF)
#define PG_INT32_MIN  (-0x7FFFFFFF - 1)
#define PG_INT32_MAX  (0x7FFFFFFF)
#define PG_UINT32_MAX (0xFFFFFFFFU)
#define PG_INT64_MIN  (-INT64CONST(0x7FFFFFFFFFFFFFFF) - 1)
#define PG_INT64_MAX  INT64CONST(0x7FFFFFFFFFFFFFFF)
#define PG_UINT64_MAX UINT64CONST(0xFFFFFFFFFFFFFFFF)

/* ----------------------------------------------------------------
 *                Section 4:    IsValid macros for system types
 * ----------------------------------------------------------------
 */
/*
 * BoolIsValid
 *        True iff bool is valid.
 */
#define BoolIsValid(boolean) ((boolean) == false || (boolean) == true)

/*
 * PointerIsValid
 *        True iff pointer is valid.
 */
#define PointerIsValid(pointer) ((void *) (pointer) != NULL)

/*
 * PointerIsAligned
 *        True iff pointer is properly aligned to point to the given type.
 */
#define PointerIsAligned(pointer, type) (((intptr_t) (pointer) % (sizeof(type))) == 0)

#define OidIsValid(objectId) ((bool) ((objectId) != InvalidOid))

#define RegProcedureIsValid(p) OidIsValid(p)

/* ----------------------------------------------------------------
 *                Section 5:    offsetof, lengthof, endof, alignment
 * ----------------------------------------------------------------
 */
/*
 * offsetof
 *        Offset of a structure/union field within that structure/union.
 *
 *        XXX This is supposed to be part of stddef.h, but isn't on
 *        some systems (like SunOS 4).
 */
#ifndef offsetof
# define offsetof(type, field) ((long) &((type *) 0)->field)
#endif /* offsetof */

/*
 * lengthof
 *        Number of elements in an array.
 */
#ifndef lengthof
# define lengthof(array) (sizeof(array) / sizeof((array)[0]))
#endif

/*
 * endof
 *        Address of the element one past the last in an array.
 */
#ifndef endof
# define endof(array) (&array[lengthof(array)])
#endif

/* ----------------
 * Alignment macros: align a length or address appropriately for a given type.
 *
 * There used to be some incredibly crufty platform-dependent hackery here,
 * but now we rely on the configure script to get the info for us. Much nicer.
 *
 * NOTE: TYPEALIGN will not work if ALIGNVAL is not a power of 2.
 * That case seems extremely unlikely to occur in practice, however.
 * ----------------
 */

#define TYPEALIGN(ALIGNVAL, LEN) \
 (((intptr_t) (LEN) + (ALIGNVAL - 1)) & ~((intptr_t) (ALIGNVAL - 1)))

#define SHORTALIGN(LEN)  TYPEALIGN(ALIGNOF_SHORT, (LEN))
#define INTALIGN(LEN)    TYPEALIGN(ALIGNOF_INT, (LEN))
#define LONGALIGN(LEN)   TYPEALIGN(ALIGNOF_LONG, (LEN))
#define DOUBLEALIGN(LEN) TYPEALIGN(ALIGNOF_DOUBLE, (LEN))
#define MAXALIGN(LEN)    TYPEALIGN(MAXIMUM_ALIGNOF, (LEN))

/* ----------------------------------------------------------------
 *                Section 6:    widely useful macros
 * ----------------------------------------------------------------
 */
/*
 * Max
 *        Return the maximum of two numbers.
 */
#define Max(x, y) ((x) > (y) ? (x) : (y))

/*
 * Min
 *        Return the minimum of two numbers.
 */
#define Min(x, y) ((x) < (y) ? (x) : (y))

/*
 * Abs
 *        Return the absolute value of the argument.
 */
#define Abs(x) ((x) >= 0 ? (x) : -(x))

/* Get a bit mask of the bits set in non-int32 aligned addresses */
#define INT_ALIGN_MASK (sizeof(int32) - 1)

/* Get a bit mask of the bits set in non-long aligned addresses */
#define LONG_ALIGN_MASK (sizeof(long) - 1)

/*
 * MemSet
 *    Exactly the same as standard library function memset(), but considerably
 *    faster for zeroing small word-aligned structures (such as parsetree nodes).
 *    This has to be a macro because the main point is to avoid function-call
 *    overhead.
 *
 *    We got the 64 number by testing this against the stock memset() on
 *    BSD/OS 3.0. Larger values were slower.    bjm 1997/09/11
 *
 *    I think the crossover point could be a good deal higher for
 *    most platforms, actually.  tgl 2000-03-19
 */
#ifdef MemSet
# undef MemSet
#endif
#define MemSet(start, val, len)                                                               \
 do {                                                                                         \
  char *_start = (char *) (start);                                                            \
  int _val = (val);                                                                           \
  Size _len = (len);                                                                          \
                                                                                              \
  if ((((long) _start) & INT_ALIGN_MASK) == 0 && (_len & INT_ALIGN_MASK) == 0 && _val == 0 && \
      _len <= MEMSET_LOOP_LIMIT) {                                                            \
   int32 *_p = (int32 *) _start;                                                              \
   int32 *_stop = (int32 *) (_start + _len);                                                  \
   while (_p < _stop)                                                                         \
    *_p++ = 0;                                                                                \
  } else                                                                                      \
   memset(_start, _val, _len);                                                                \
 } while (0)

/*
 * MemSetAligned is the same as MemSet except it omits the test to see if
 * "start" is word-aligned.  This is okay to use if the caller knows a-priori
 * that the pointer is suitably aligned (typically, because he just got it
 * from palloc(), which always delivers a max-aligned pointer).
 */
#define MemSetAligned(start, val, len)                                           \
 do {                                                                            \
  long *_start = (long *) (start);                                               \
  int _val = (val);                                                              \
  Size _len = (len);                                                             \
                                                                                 \
  if ((_len & LONG_ALIGN_MASK) == 0 && _val == 0 && _len <= MEMSET_LOOP_LIMIT && \
      MEMSET_LOOP_LIMIT != 0) {                                                  \
   long *_stop = (long *) ((char *) _start + _len);                              \
   while (_start < _stop)                                                        \
    *_start++ = 0;                                                               \
  } else                                                                         \
   memset(_start, _val, _len);                                                   \
 } while (0)

#define MEMSET_LOOP_LIMIT 64

/* ----------------------------------------------------------------
 *                Section 7:    random stuff
 * ----------------------------------------------------------------
 */

/* msb for char */
#define CSIGNBIT           (0x80)
#define IS_HIGHBIT_SET(ch) ((unsigned char) (ch) &CSIGNBIT)

#define STATUS_OK           (0)
#define STATUS_ERROR        (-1)
#define STATUS_NOT_FOUND    (-2)
#define STATUS_INVALID      (-3)
#define STATUS_UNCATALOGUED (-4)
#define STATUS_REPLACED     (-5)
#define STATUS_NOT_DONE     (-6)
#define STATUS_BAD_PACKET   (-7)
#define STATUS_TIMEOUT      (-8)
#define STATUS_FOUND        (1)

/*
 * Macro that allows to cast constness and volatile away from an expression, but doesn't
 * allow changing the underlying type.  Enforcement of the latter
 * currently only works for gcc like compilers.
 *
 * Please note IT IS NOT SAFE to cast constness away if the result will ever
 * be modified (it would be undefined behaviour). Doing so anyway can cause
 * compiler misoptimizations or runtime crashes (modifying readonly memory).
 * It is only safe to use when the result will not be modified, but API
 * design or language restrictions prevent you from declaring that
 * (e.g. because a function returns both const and non-const variables).
 *
 * Note that this only works in function scope, not for global variables (it'd
 * be nice, but not trivial, to improve that).
 */
#if defined(HAVE__BUILTIN_TYPES_COMPATIBLE_P)
# define unconstify(underlying_type, expr)                                               \
  (StaticAssertExpr(__builtin_types_compatible_p(__typeof(expr), const underlying_type), \
                    "wrong cast"),                                                       \
   (underlying_type) (expr))
# define unvolatize(underlying_type, expr)                                                  \
  (StaticAssertExpr(__builtin_types_compatible_p(__typeof(expr), volatile underlying_type), \
                    "wrong cast"),                                                          \
   (underlying_type) (expr))
#else
# define unconstify(underlying_type, expr) ((underlying_type) (expr))
# define unvolatize(underlying_type, expr) ((underlying_type) (expr))
#endif

/* ----------------------------------------------------------------
 *                Section 8: system-specific hacks
 *
 *        This should be limited to things that absolutely have to be
 *        included in every source file.  The port-specific header file
 *        is usually a better place for this sort of thing.
 * ----------------------------------------------------------------
 */

#ifdef __CYGWIN__
# define PG_BINARY   O_BINARY
# define PG_BINARY_R "rb"
# define PG_BINARY_W "wb"
#else
# define PG_BINARY   0
# define PG_BINARY_R "r"
# define PG_BINARY_W "w"
#endif

#if defined(linux)
# include <unistd.h>
#endif

#if defined(sun) && defined(__sparc__) && !defined(__SVR4)
# include <unistd.h>
#endif

/* These are for things that are one way on Unix and another on NT */
#define NULL_DEV "/dev/null"
#define SEP_CHAR '/'

/* defines for dynamic linking on Win32 platform */
#ifdef __CYGWIN__
# if __GNUC__ && !defined(__declspec)
#  error You need egcs 1.1 or newer for compiling!
# endif
# ifdef BUILDING_DLL
#  define DLLIMPORT __declspec(dllexport)
# else /* not BUILDING_DLL */
#  define DLLIMPORT __declspec(dllimport)
# endif
#else /* not CYGWIN */
# define DLLIMPORT
#endif

/* Provide prototypes for routines not present in a particular machine's
 * standard C library.    It'd be better to put these in config.h, but
 * in config.h we haven't yet included anything that defines size_t...
 */

#ifndef HAVE_SNPRINTF_DECL
extern int snprintf(char *str, size_t count, const char *fmt, ...);

#endif

#ifndef HAVE_VSNPRINTF_DECL
extern int vsnprintf(char *str, size_t count, const char *fmt, va_list args);

#endif

#if !defined(HAVE_MEMMOVE) && !defined(memmove)
# define memmove(d, s, c) bcopy(s, d, c)
#endif

/* ----------------------------------------------------------------
 *                Section 9:    exception handling definitions
 *                        Assert, Trap, etc macros
 * ----------------------------------------------------------------
 */

typedef char *ExcMessage;

typedef struct Exception
{
    ExcMessage message;
} Exception;

extern Exception FailedAssertion;
extern Exception BadArg;
extern Exception BadState;
extern Exception VarTagError;

extern bool assert_enabled;
extern bool log_mask_all_strings;

extern int ExceptionalCondition(const char *conditionName,
                                Exception *exceptionP,
                                const char *details,
                                const char *fileName,
                                int lineNumber);

/*
 * USE_ASSERT_CHECKING, if defined, turns on all the assertions.
 * - plai  9/5/90
 *
 * It should _NOT_ be defined in releases or in benchmark copies
 */

/*
 * Trap
 *        Generates an exception if the given condition is true.
 *
 */
#define Trap(condition, exception)                                                       \
 do {                                                                                    \
  if ((assert_enabled) && (condition))                                                   \
   ExceptionalCondition(CppAsString(condition), &(exception), NULL, __FILE__, __LINE__); \
 } while (0)

/*
 *    TrapMacro is the same as Trap but it's intended for use in macros:
 *
 *        #define foo(x) (AssertM(x != 0) && bar(x))
 *
 *    Isn't CPP fun?
 */
#define TrapMacro(condition, exception)        \
 ((bool) ((!assert_enabled) || !(condition) || \
          (ExceptionalCondition(CppAsString(condition), &(exception), NULL, __FILE__, __LINE__))))

#ifndef USE_ASSERT_CHECKING
# define Assert(condition)
# define AssertMacro(condition) ((void) true)
# define AssertArg(condition)
# define AssertState(condition)
# define assert_enabled 0

#elif defined(FRONTEND)

# include <assert.h>
# define Assert(p)                         assert(p)
# define AssertMacro(p)                    ((void) assert(p))
# define AssertArg(condition)              assert(condition)
# define AssertState(condition)            assert(condition)
# define AssertPointerAlignment(ptr, bndr) ((void) true)

#else
# define Assert(condition) Trap(!(condition), FailedAssertion)

# define AssertMacro(condition) ((void) TrapMacro(!(condition), FailedAssertion))

# define AssertArg(condition) Trap(!(condition), BadArg)

# define AssertState(condition) Trap(!(condition), BadState)

#endif /* USE_ASSERT_CHECKING */

/*
 * LogTrap
 *        Generates an exception with a message if the given condition is true.
 *
 */
#define LogTrap(condition, exception, printArgs) \
 do {                                            \
  if ((assert_enabled) && (condition))           \
   ExceptionalCondition(CppAsString(condition),  \
                        &(exception),            \
                        vararg_format printArgs, \
                        __FILE__,                \
                        __LINE__);               \
 } while (0)

/*
 *    LogTrapMacro is the same as LogTrap but it's intended for use in macros:
 *
 *        #define foo(x) (LogAssertMacro(x != 0, "yow!") && bar(x))
 */
#define LogTrapMacro(condition, exception, printArgs)    \
 ((bool) ((!assert_enabled) || !(condition) ||           \
          (ExceptionalCondition(CppAsString(condition),  \
                                &(exception),            \
                                vararg_format printArgs, \
                                __FILE__,                \
                                __LINE__))))

extern char *vararg_format(const char *fmt, ...);

#ifndef USE_ASSERT_CHECKING
# define LogAssert(condition, printArgs)
# define LogAssertMacro(condition, printArgs) true
# define LogAssertArg(condition, printArgs)
# define LogAssertState(condition, printArgs)
#else
# define LogAssert(condition, printArgs) LogTrap(!(condition), FailedAssertion, printArgs)

# define LogAssertMacro(condition, printArgs) LogTrapMacro(!(condition), FailedAssertion, printArgs)

# define LogAssertArg(condition, printArgs) LogTrap(!(condition), BadArg, printArgs)

# define LogAssertState(condition, printArgs) LogTrap(!(condition), BadState, printArgs)

# ifdef ASSERT_CHECKING_TEST
extern int assertTest(int val);

# endif

#endif /* USE_ASSERT_CHECKING */

#ifdef __cplusplus /* NZ */
# define CEXTERN extern "C"
#else
# define CEXTERN extern
#endif

/* ----------------
 *        end of c.h
 * ----------------
 */
#endif /* C_H */
