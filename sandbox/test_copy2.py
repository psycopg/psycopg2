import psycopg2

dbconn = psycopg2.connect(database="test",host="localhost",port="5432")
query = """
        CREATE TEMP TABLE data (
            field01 char,
            field02 varchar,
            field03 varchar,
            field04 varchar,
            field05 varchar,
            field06 varchar,
            field07 varchar,
            field08 varchar,
            field09 numeric,
            field10 integer,
            field11 numeric,
            field12 numeric,
            field13 numeric,
            field14 numeric,
            field15 numeric,
            field16 numeric,
            field17 char,
            field18 char,
            field19 char,
            field20 varchar,
            field21 varchar,
            field22 integer,
            field23 char,
            field24 char
        );
        """
cursor = dbconn.cursor()
cursor.execute(query)

f = open('test_copy2.csv')
cursor.copy_from(f, 'data', sep='|')
f.close()

dbconn.commit()

cursor.close()
dbconn.close()

