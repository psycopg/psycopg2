static long int typecast_NUMBER_types[] = {20, 23, 21, 701, 700, 1700, 0};
static long int typecast_LONGINTEGER_types[] = {20, 0};
static long int typecast_INTEGER_types[] = {23, 21, 0};
static long int typecast_FLOAT_types[] = {701, 700, 0};
static long int typecast_DECIMAL_types[] = {1700, 0};
static long int typecast_UNICODE_types[] = {19, 18, 25, 1042, 1043, 0};
static long int typecast_STRING_types[] = {19, 18, 25, 1042, 1043, 0};
static long int typecast_BOOLEAN_types[] = {16, 0};
static long int typecast_DATETIME_types[] = {1114, 1184, 704, 1186, 0};
static long int typecast_TIME_types[] = {1083, 1266, 0};
static long int typecast_DATE_types[] = {1082, 0};
static long int typecast_INTERVAL_types[] = {704, 1186, 0};
static long int typecast_BINARY_types[] = {17, 0};
static long int typecast_ROWID_types[] = {26, 0};
static long int typecast_LONGINTEGERARRAY_types[] = {1016, 0};
static long int typecast_INTEGERARRAY_types[] = {1005, 1006, 1007, 0};
static long int typecast_STRINGARRAY_types[] = {1002, 1003, 1009, 1014, 1015, 0};


typecastObject_initlist typecast_builtins[] = {
  {"NUMBER", typecast_NUMBER_types, typecast_NUMBER_cast, NULL},
  {"LONGINTEGER", typecast_LONGINTEGER_types, typecast_LONGINTEGER_cast, NULL},
  {"INTEGER", typecast_INTEGER_types, typecast_INTEGER_cast, NULL},
  {"FLOAT", typecast_FLOAT_types, typecast_FLOAT_cast, NULL},
  {"DECIMAL", typecast_DECIMAL_types, typecast_DECIMAL_cast, NULL},
  {"UNICODE", typecast_UNICODE_types, typecast_UNICODE_cast, NULL},
  {"STRING", typecast_STRING_types, typecast_STRING_cast, NULL},
  {"BOOLEAN", typecast_BOOLEAN_types, typecast_BOOLEAN_cast, NULL},
  {"DATETIME", typecast_DATETIME_types, typecast_DATETIME_cast, NULL},
  {"TIME", typecast_TIME_types, typecast_TIME_cast, NULL},
  {"DATE", typecast_DATE_types, typecast_DATE_cast, NULL},
  {"INTERVAL", typecast_INTERVAL_types, typecast_INTERVAL_cast, NULL},
  {"BINARY", typecast_BINARY_types, typecast_BINARY_cast, NULL},
  {"ROWID", typecast_ROWID_types, typecast_ROWID_cast, NULL},
  {"LONGINTEGERARRAY", typecast_LONGINTEGERARRAY_types, typecast_LONGINTEGERARRAY_cast, "LONGINTEGER"},
  {"INTEGERARRAY", typecast_INTEGERARRAY_types, typecast_INTEGERARRAY_cast, "INTEGER"},
  {"STRINGARRAY", typecast_STRINGARRAY_types, typecast_STRINGARRAY_cast, "STRING"},
    {NULL, NULL, NULL, NULL}
};

