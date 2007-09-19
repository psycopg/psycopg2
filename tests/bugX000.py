import psycopg2
import time

d1 = psycopg2.Date(2002,12,25)
d2 = psycopg2.DateFromTicks(time.mktime((2002,12,25,0,0,0,0,0,0)))
t1 = psycopg2.Time(13,45,30)
t2 = psycopg2.TimeFromTicks(time.mktime((2001,1,1,13,45,30,0,0,0)))
t1 = psycopg2.Timestamp(2002,12,25,13,45,30)
t2 = psycopg2.TimestampFromTicks( time.mktime((2002,12,25,13,45,30,0,0,0)))

