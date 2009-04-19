def test():
        import sys, os, thread, psycopg2
        def test2():
                while True:
                        for filename in map(lambda m: getattr(m, "__file__", None), sys.modules.values()):
                                os.stat("/dev/null")
        connection = psycopg2.connect(database="test")
        cursor = connection.cursor()
        thread.start_new_thread(test2, ())
        while True:
                cursor.execute("COMMIT")
test()
