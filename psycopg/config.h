/* config.h - general config and Dprintf macro
 *
 * Copyright (C) 2003 Federico Di Gregorio <fog@debian.org>
 *
 * This file is part of psycopg.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef PSYCOPG_CONFIG_H
#define PSYCOPG_CONFIG_H 1

/* debug printf-like function */
#if defined( __GNUC__) && !defined(__APPLE__)
#ifdef PSYCOPG_DEBUG
#include <sys/types.h>
#include <unistd.h>
#define Dprintf(fmt, args...) \
    fprintf(stderr, "[%d] " fmt "\n", getpid() , ## args)
#else
#define Dprintf(fmt, args...)
#endif
#else /* !__GNUC__ or __APPLE__ */
#ifdef PSYCOPG_DEBUG
#include <stdarg.h>
static void Dprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}
#else
static void Dprintf(const char *fmt, ...) {}
#endif
#endif

/* pthreads work-arounds for mutilated operating systems */
#if defined(_WIN32) || defined(__BEOS__)

#ifdef _WIN32
#include <winsock2.h>
#define pthread_mutex_t HANDLE
#define pthread_condvar_t HANDLE
#define pthread_mutex_lock(object) WaitForSingleObject(object, INFINITE)
#define pthread_mutex_unlock(object) ReleaseMutex(object)
#define pthread_mutex_destroy(ref) (CloseHandle(*(ref)))
/* convert pthread mutex to native mutex */
static int pthread_mutex_init(pthread_mutex_t *mutex, void* fake)
{
  *mutex = CreateMutex(NULL, FALSE, NULL);
  return 0;
}
#endif /* _WIN32 */

#ifdef __BEOS__
#include <OS.h>
#define pthread_mutex_t sem_id
#define pthread_mutex_lock(object) acquire_sem(object)
#define pthread_mutex_unlock(object) release_sem(object)
#define pthread_mutex_destroy(ref) delete_sem(*ref)
static int pthread_mutex_init(pthread_mutex_t *mutex, void* fake)
{
        *mutex = create_sem(1, "psycopg_mutex");
        if (*mutex < B_OK)
                return *mutex;
        return 0;
}
#endif /* __BEOS__ */

#else /* pthread is available */
#include <pthread.h>
#endif

/* to work around the fact that Windows does not have a gmtime_r function, or
   a proper gmtime function */
#ifdef _WIN32
static struct tm *gmtime_r(time_t *t, struct tm *tm)
{
  tm = gmtime(t);
  return tm;
}
static struct tm *localtime_r(time_t *t, struct tm *tm)
{
  tm = localtime(t);
  return tm;
}
/* remove the inline keyword, since it doesn't work unless C++ file */
#define inline
#endif

#if defined(__FreeBSD__) || defined(_WIN32) || defined(__sun__)
/* what's this, we have no round function either? */
static double round(double num)
{
  return (num >= 0) ? floor(num + 0.5) : ceil(num - 0.5);
}
#endif

/* postgresql < 7.4 does not have PQfreemem */
#ifndef HAVE_PQFREEMEM
#define PQfreemem free
#endif

#endif /* !defined(PSYCOPG_CONFIG_H) */
