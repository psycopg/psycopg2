"""
This example/recipe has been contributed by Valentino Volonghi (dialtone)

Mapping arbitrary objects to a PostgreSQL database with psycopg2

- Problem

You need to store arbitrary objects in a PostgreSQL database without being
intrusive for your classes (don't want inheritance from an 'Item' or 
'Persistent' object).

- Solution
"""

from datetime import datetime
 
import psycopg2
from psycopg2.extensions import adapt, register_adapter

try:
    sorted()
except:
    def sorted(seq):
        seq.sort()
        return seq

# Here is the adapter for every object that we may ever need to 
# insert in the database. It receives the original object and does
# its job on that instance

class ObjectMapper(object):
    def __init__(self, orig, curs=None):
        self.orig = orig
        self.tmp = {}
        self.items, self.fields = self._gatherState()
 
    def _gatherState(self):
        adaptee_name = self.orig.__class__.__name__
        fields = sorted([(field, getattr(self.orig, field))
                        for field in persistent_fields[adaptee_name]])
        items = []
        for item, value in fields:
            items.append(item)
        return items, fields
 
    def getTableName(self):
        return self.orig.__class__.__name__
 
    def getMappedValues(self):
        tmp = []
        for i in self.items:
            tmp.append("%%(%s)s"%i)
        return ", ".join(tmp)
 
    def getValuesDict(self):
        return dict(self.fields)
 
    def getFields(self):
        return self.items

    def generateInsert(self):
        qry = "INSERT INTO"
        qry += " " + self.getTableName() + " ("
        qry += ", ".join(self.getFields()) + ") VALUES ("
        qry += self.getMappedValues() + ")"
        return qry, self.getValuesDict()

# Here are the objects
class Album(object):    
    id = 0 
    def __init__(self):
        self.creation_time = datetime.now()
        self.album_id = self.id
        Album.id = Album.id + 1
        self.binary_data = buffer('12312312312121')
 
class Order(object):
     id = 0
     def __init__(self):
        self.items = ['rice','chocolate']
        self.price = 34
        self.order_id = self.id
        Order.id = Order.id + 1

register_adapter(Album, ObjectMapper)
register_adapter(Order, ObjectMapper)
    
# Describe what is needed to save on each object
# This is actually just configuration, you can use xml with a parser if you
# like to have plenty of wasted CPU cycles ;P.

persistent_fields = {'Album': ['album_id', 'creation_time', 'binary_data'],
                              'Order':  ['order_id', 'items', 'price']
                            }
 
print adapt(Album()).generateInsert()
print adapt(Album()).generateInsert()
print adapt(Album()).generateInsert()
print adapt(Order()).generateInsert()
print adapt(Order()).generateInsert()
print adapt(Order()).generateInsert()

"""
- Discussion

Psycopg 2 has a great new feature: adaptation. The big thing about 
adaptation is that it enables the programmer to glue most of the 
code out there without many difficulties.

This recipe tries to focus attention on a way to generate SQL queries to 
insert  completely new objects inside a database. As you can see objects do 
not know anything about the code that is handling them. We specify all the 
fields that we need for each object through the persistent_fields dict.

The most important lines of this recipe are:
    register_adapter(Album, ObjectMapper)
    register_adapter(Order, ObjectMapper)

In these lines we notify the system that when we call adapt with an Album instance 
as an argument we want it to istantiate ObjectMapper passing the Album instance  
as argument (self.orig in the ObjectMapper class).

The output is something like this (for each call to generateInsert):
    
('INSERT INTO Album (album_id, binary_data, creation_time) VALUES 
   (%(album_id)s, %(binary_data)s, %(creation_time)s)', 
      
  {'binary_data': <read-only buffer for 0x402de070, ...>, 
    'creation_time':   datetime.datetime(2004, 9, 10, 20, 48, 29, 633728), 
    'album_id': 1}
)

This is a tuple of {SQL_QUERY, FILLING_DICT}, and all the quoting/converting 
stuff (from python's datetime to postgres s and from python's buffer to 
postgres' blob) is handled with the same adaptation process hunder the hood 
by psycopg2.

At last, just notice that ObjectMapper is working for both Album and Order 
instances without any glitches at all, and both classes could have easily been 
coming from closed source libraries or C coded ones (which are not easily 
modified), whereas a common pattern in todays ORMs or OODBs is to provide 
a basic 'Persistent' object that already knows how to store itself in the 
database.
"""
