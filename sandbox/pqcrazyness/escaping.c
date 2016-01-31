#include <string.h>
#include <stdlib.h>
#include <libpq-fe.h>

/* Compile me with:
 * 
 *   gcc -p escaping -I$(pg_config --includedir) escaping.c -lpq"
 *
 * then:
 *
 *   ./escaping dbname=test foo\\\\bar
*/

int main(int argc, char** argv)
{
	PGconn* conn;
	int rl, len;
	char* result;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s [dsn] [string]\n", argv[0]);
		return 1;
	}

	len = strlen(argv[1]);
	result = malloc(len * 2 * sizeof(char));

	rl = PQescapeString(result, argv[2], len);
	result[rl] = '\0';
	printf("%s\n", result);

	conn = PQconnectdb(argv[1]);

	rl = PQescapeString(result, argv[2], len);
	result[rl] = '\0';
	printf("%s\n", result);

	return 0;
}

