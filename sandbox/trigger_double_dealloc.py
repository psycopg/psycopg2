import psycopg2, psycopg2.extensions
import threading
import gc
import time
import sys

# inherit psycopg2 connection class just so that
# garbage collector enters the tp_clear code path
# in delete_garbage()

class my_connection(psycopg2.extensions.connection):
    pass

class db_user(threading.Thread):
    def run(self):
        conn2 = psycopg2.connect(sys.argv[1], connection_factory=my_connection)
        cursor = conn2.cursor()
        cursor.execute("UPDATE test_psycopg2_dealloc SET a = 3", async=1)

        # the conn2 desctructor will block indefinitely
        # on the completion of the query
        # (and it will not be holding the GIL during that time)
        print >> sys.stderr, "begin conn2 del"
        del cursor, conn2
        print >> sys.stderr, "end conn2 del"

def main():
    # lock out a db row
    conn1 = psycopg2.connect(sys.argv[1], connection_factory=my_connection)
    cursor = conn1.cursor()
    cursor.execute("DROP TABLE IF EXISTS test_psycopg2_dealloc")
    cursor.execute("CREATE TABLE test_psycopg2_dealloc (a int)")
    cursor.execute("INSERT INTO test_psycopg2_dealloc VALUES (1)")
    conn1.commit()
    cursor.execute("UPDATE test_psycopg2_dealloc SET a = 2", async=1)

    # concurrent thread trying to access the locked row
    db_user().start()

    # eventually, a gc.collect run will happen
    # while the conn2 is inside conn_close()
    # but this second dealloc won't get blocked
    # as it will avoid conn_close()
    for i in range(10):
        if gc.collect():
            print >> sys.stderr, "garbage collection done"
            break
        time.sleep(1)

    # we now unlock the row by invoking
    # the desctructor of conn1. This will permit the
    # concurrent thread destructor of conn2 to
    # continue and it will end up trying to free
    # self->dsn a second time.
    print >> sys.stderr, "begin conn1 del"
    del cursor, conn1
    print >> sys.stderr, "end conn1 del"


if __name__ == '__main__':
    main()
