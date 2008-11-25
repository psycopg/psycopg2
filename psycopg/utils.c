/* utils.c - miscellaneous utility functions
 *
 */

#include "psycopg/config.h"
#include "psycopg/utils.h"
#include "psycopg/psycopg.h"
#include "psycopg/connection.h"
#include "psycopg/pgtypes.h"
#include "psycopg/pgversion.h"
#include <string.h>
#include <stdlib.h>

char *psycopg_internal_escape_string(connectionObject *conn, const char *string)
{
    char *buffer;
    size_t string_length;
    int equote;         /* buffer offset if E'' quotes are needed */  

    string_length = strlen(string);
    
    buffer = (char *) malloc((string_length * 2 + 4) * sizeof(char));
    if (buffer == NULL) {
        return NULL;
    }

    equote = (conn && (conn->equote)) ? 1 : 0;

    { 
        size_t qstring_length;

        qstring_length = qstring_escape(buffer + equote + 1, string, string_length,
                                        (conn ? conn->pgconn : NULL));

        if (equote)
            buffer[0] = 'E';
        buffer[equote] = '\''; 
        buffer[qstring_length + equote + 1] = '\'';
        buffer[qstring_length + equote + 2] = 0;
    }

    return buffer;
}
