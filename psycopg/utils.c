/* utils.c - miscellaneous utility functions
 *
 */

#include <Python.h>
#include <string.h>

#include "psycopg/config.h"
#include "psycopg/psycopg.h"
#include "psycopg/connection.h"
#include "psycopg/pgtypes.h"
#include "psycopg/pgversion.h"
#include <stdlib.h>

char *
psycopg_escape_string(PyObject *obj, const char *from, Py_ssize_t len,
                       char *to, Py_ssize_t *tolen)
{
    Py_ssize_t ql;
    connectionObject *conn = (connectionObject*)obj;
    int eq = (conn && (conn->equote)) ? 1 : 0;   

    if (len == 0)
        len = strlen(from);
    
    if (to == NULL) {
        to = (char *)PyMem_Malloc((len * 2 + 4) * sizeof(char));
        if (to == NULL)
            return NULL;
    }

    #ifndef PSYCOPG_OWN_QUOTING
    {
        #if PG_VERSION_HEX >= 0x080104
            int err;
            if (conn && conn->pgconn)
                ql = PQescapeStringConn(conn->pgconn, to+eq+1, from, len, &err);
            else
        #endif
                ql = PQescapeString(to+eq+1, from, len);
    }
    #else
    {
        int i, j;
    
        for (i=0, j=eq+1; i<len; i++) {
            switch(from[i]) {
    
            case '\'':
                to[j++] = '\'';
                to[j++] = '\'';
                break;
    
            case '\\':
                to[j++] = '\\';
                to[j++] = '\\';
                break;
    
            case '\0':
                /* do nothing, embedded \0 are discarded */
                break;
    
            default:
                to[j++] = from[i];
            }
        }
        to[j] = '\0';
    
        Dprintf("qstring_quote: to = %s", to);
        ql = strlen(to);
    }
    #endif

    if (eq)
        to[0] = 'E';
    to[eq] = '\''; 
    to[ql+eq+1] = '\'';
    to[ql+eq+2] = '\0';

    if (tolen)
        *tolen = ql+eq+2;
        
    return to;
}
