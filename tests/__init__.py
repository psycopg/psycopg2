import os
import unittest

dbname = os.environ.get('PSYCOPG2_TESTDB', 'test') 

import test_psycopg2_dbapi20 
import test_transaction 
import types_basic 
import extras_dictcursor 
	 
def test_suite(): 
    suite = unittest.TestSuite() 
    suite.addTest(test_psycopg2_dbapi20.test_suite()) 
    suite.addTest(test_transaction.test_suite()) 
    suite.addTest(types_basic.test_suite()) 
    suite.addTest(extras_dictcursor.test_suite()) 
    return suite 

if __name__ == "__main__":
    unittest.main()

