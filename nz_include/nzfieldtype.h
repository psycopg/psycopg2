#ifndef NZFIELDTYPE_H
#define NZFIELDTYPE_H

// Field type numbers are persisted in the compressed external
// table header. Extending this list with new types is fine.
// The existing constants up to NzTypeLastEntry cannot be renumbered
// (check with Abhishek Jog for details about external table usage).
// If you change an existing type number, we need to write some backward
// compatibility code.

#define NZ_FIELD_TYPE_LIST                                                                                                                       \
                                                                                                                                                 \
 /*    (ORDINAL, ENUMERATOR,            REP,  ALIGN, FIXED, SCHEMA, ZMACCUM, COMPARE, DESCRIPTION)                                               \
  */                                                                                                                                             \
                                                                                                                                                 \
 _ROW_(0, NzTypeUndefined, void, 0, 0, INVALID, invalid, Bad, "UNUSED 0")                                                                        \
 _SEP_ _ROW_(1,                                                                                                                                  \
             NzTypeRecAddr,                                                                                                                      \
             int64,                                                                                                                              \
             ALN64,                                                                                                                              \
             8,                                                                                                                                  \
             INT,                                                                                                                                \
             invalid,                                                                                                                            \
             Int8,                                                                                                                               \
             "RecAddr (8 bytes)") _SEP_ _ROW_(2,                                                                                                 \
                                              NzTypeDouble,                                                                                      \
                                              double,                                                                                            \
                                              ALN64,                                                                                             \
                                              8,                                                                                                 \
                                              FLOAT,                                                                                             \
                                              double,                                                                                            \
                                              Double,                                                                                            \
                                              "FP double (8 bytes)") _SEP_                                                                       \
     _ROW_(3, NzTypeInt, int32, ALN32, 4, INT, int32, Int4, "Integer (4 bytes)") _SEP_ _ROW_(                                                    \
         4,                                                                                                                                      \
         NzTypeFloat,                                                                                                                            \
         float,                                                                                                                                  \
         ALN32,                                                                                                                                  \
         4,                                                                                                                                      \
         FLOAT,                                                                                                                                  \
         float,                                                                                                                                  \
         Float,                                                                                                                                  \
         "FP single (4 bytes)") _SEP_ _ROW_(5,                                                                                                   \
                                            NzTypeMoney,                                                                                         \
                                            int32,                                                                                               \
                                            ALN32,                                                                                               \
                                            4,                                                                                                   \
                                            INT,                                                                                                 \
                                            int32,                                                                                               \
                                            Int4,                                                                                                \
                                            "Money (4 bytes)") _SEP_                                                                             \
         _ROW_(6, NzTypeDate, int32, ALN32, 4, INT, int32, Int4, "Date (4 bytes)") _SEP_ _ROW_(                                                  \
             7,                                                                                                                                  \
             NzTypeNumeric,                                                                                                                      \
             void,                                                                                                                               \
             ALN128,                                                                                                                             \
             -1,                                                                                                                                 \
             NUMERIC,                                                                                                                            \
             cnum64,                                                                                                                             \
             Bad,                                                                                                                                \
             "Numeric (4, 8 or 16 bytes)") _SEP_ _ROW_(8,                                                                                        \
                                                       NzTypeTime,                                                                               \
                                                       int64,                                                                                    \
                                                       ALN64,                                                                                    \
                                                       8,                                                                                        \
                                                       INT,                                                                                      \
                                                       int64,                                                                                    \
                                                       Int8,                                                                                     \
                                                       "Time (8 bytes)")                                                                         \
             _SEP_ _ROW_(9,                                                                                                                      \
                         NzTypeTimestamp,                                                                                                        \
                         timestamp,                                                                                                              \
                         ALN64,                                                                                                                  \
                         8,                                                                                                                      \
                         INT,                                                                                                                    \
                         int64,                                                                                                                  \
                         Int8,                                                                                                                   \
                         "Timestamp (8 bytes)") _SEP_ _ROW_(10,                                                                                  \
                                                            NzTypeInterval,                                                                      \
                                                            interval,                                                                            \
                                                            ALN128,                                                                              \
                                                            12,                                                                                  \
                                                            TIME,                                                                                \
                                                            interval,                                                                            \
                                                            Interval,                                                                            \
                                                            "Interval (12 bytes)")                                                               \
                 _SEP_ _ROW_(11,                                                                                                                 \
                             NzTypeTimeTz,                                                                                                       \
                             timetz,                                                                                                             \
                             ALN128,                                                                                                             \
                             12,                                                                                                                 \
                             TIME,                                                                                                               \
                             timetz,                                                                                                             \
                             TimeTz,                                                                                                             \
                             "Time and TZ (12 bytes)") _SEP_ _ROW_(12,                                                                           \
                                                                   NzTypeBool,                                                                   \
                                                                   int8,                                                                         \
                                                                   ALNNO,                                                                        \
                                                                   1,                                                                            \
                                                                   INT,                                                                          \
                                                                   int8,                                                                         \
                                                                   Bool,                                                                         \
                                                                   "Boolean (1 byte)")                                                           \
                     _SEP_ _ROW_(                                                                                                                \
                         13,                                                                                                                     \
                         NzTypeInt1,                                                                                                             \
                         int8,                                                                                                                   \
                         ALNNO,                                                                                                                  \
                         1,                                                                                                                      \
                         INT,                                                                                                                    \
                         int8,                                                                                                                   \
                         Int1,                                                                                                                   \
                         "Integer (1 byte)") _SEP_ _ROW_(14,                                                                                     \
                                                         NzTypeBinary,                                                                           \
                                                         void,                                                                                   \
                                                         0,                                                                                      \
                                                         0,                                                                                      \
                                                         INVALID,                                                                                \
                                                         invalid,                                                                                \
                                                         Bad,                                                                                    \
                                                         "UNUSED 14") _SEP_ _ROW_(15,                                                            \
                                                                                  NzTypeChar,                                                    \
                                                                                  void,                                                          \
                                                                                  ALNNO,                                                         \
                                                                                  -2,                                                            \
                                                                                  FIXEDCHAR,                                                     \
                                                                                  str1A,                                                         \
                                                                                  Bad,                                                           \
                                                                                  "Char (fixed, "                                                \
                                                                                  "1-16 bytes)")                                                 \
                         _SEP_ _ROW_(16,                                                                                                         \
                                     NzTypeVarChar,                                                                                              \
                                     varA,                                                                                                       \
                                     FVARY,                                                                                                      \
                                     0,                                                                                                          \
                                     VARCHAR,                                                                                                    \
                                     varA,                                                                                                       \
                                     Bad,                                                                                                        \
                                     "ASCII Char Varying") _SEP_ _ROW_(17,                                                                       \
                                                                       NzDEPR_Text,                                                              \
                                                                       void,                                                                     \
                                                                       FVARY,                                                                    \
                                                                       0,                                                                        \
                                                                       INVALID,                                                                  \
                                                                       invalid,                                                                  \
                                                                       Bad,                                                                      \
                                                                       "UNUSED 17")                                                              \
                             _SEP_ _ROW_(                                                                                                        \
                                 18,                                                                                                             \
                                 NzTypeUnknown,                                                                                                  \
                                 void,                                                                                                           \
                                 FVARY,                                                                                                          \
                                 0,                                                                                                              \
                                 INVALID,                                                                                                        \
                                 invalid,                                                                                                        \
                                 Bad,                                                                                                            \
                                 "UNUSED 18") _SEP_ _ROW_(19,                                                                                    \
                                                          NzTypeInt2,                                                                            \
                                                          int16,                                                                                 \
                                                          ALN16,                                                                                 \
                                                          2,                                                                                     \
                                                          INT,                                                                                   \
                                                          int16,                                                                                 \
                                                          Int2,                                                                                  \
                                                          "Integer (2 "                                                                          \
                                                          "bytes)") _SEP_ _ROW_(20,                                                              \
                                                                                NzTypeInt8,                                                      \
                                                                                int64,                                                           \
                                                                                ALN64,                                                           \
                                                                                8,                                                               \
                                                                                INT,                                                             \
                                                                                int64,                                                           \
                                                                                Int8,                                                            \
                                                                                "Integer (8 "                                                    \
                                                                                "bytes)")                                                        \
                                 _SEP_ _ROW_(21,                                                                                                 \
                                             NzTypeVarFixedChar,                                                                                 \
                                             varA,                                                                                               \
                                             FVARY,                                                                                              \
                                             0,                                                                                                  \
                                             VARCHAR,                                                                                            \
                                             varA,                                                                                               \
                                             Bad,                                                                                                \
                                             "ASCII Char (using "                                                                                \
                                             "varying)") _SEP_ _ROW_(22,                                                                         \
                                                                     NzTypeGeometry,                                                             \
                                                                     varA,                                                                       \
                                                                     FVARY,                                                                      \
                                                                     0,                                                                          \
                                                                     VARCHAR,                                                                    \
                                                                     varA,                                                                       \
                                                                     Bad,                                                                        \
                                                                     "ST_Geometry")                                                              \
                                     _SEP_ _ROW_(23,                                                                                             \
                                                 NzTypeVarBinary,                                                                                \
                                                 varA,                                                                                           \
                                                 FVARY,                                                                                          \
                                                 0,                                                                                              \
                                                 VARCHAR,                                                                                        \
                                                 varA,                                                                                           \
                                                 Bad,                                                                                            \
                                                 "Binary Varying") _SEP_ _ROW_(24,                                                               \
                                                                               NzDEPR_Blob,                                                      \
                                                                               void,                                                             \
                                                                               FVARY,                                                            \
                                                                               0,                                                                \
                                                                               INVALID,                                                          \
                                                                               invalid,                                                          \
                                                                               Bad,                                                              \
                                                                               "UNUSED 24")                                                      \
                                         _SEP_ _ROW_(                                                                                            \
                                             25,                                                                                                 \
                                             NzTypeNChar,                                                                                        \
                                             varA,                                                                                               \
                                             FVARY,                                                                                              \
                                             0,                                                                                                  \
                                             VARCHAR,                                                                                            \
                                             varA,                                                                                               \
                                             Bad,                                                                                                \
                                             "UTF-8 NChar "                                                                                      \
                                             "(using varying)") _SEP_ _ROW_(26,                                                                  \
                                                                            NzTypeNVarChar,                                                      \
                                                                            varA,                                                                \
                                                                            FVARY,                                                               \
                                                                            0,                                                                   \
                                                                            VARCHAR,                                                             \
                                                                            varA,                                                                \
                                                                            Bad,                                                                 \
                                                                            "UTF-8 NChar Varying")                                               \
                                             _SEP_ _ROW_(                                                                                        \
                                                 27,                                                                                             \
                                                 NzDEPR_NText,                                                                                   \
                                                 void,                                                                                           \
                                                 FVARY,                                                                                          \
                                                 0,                                                                                              \
                                                 INVALID,                                                                                        \
                                                 invalid,                                                                                        \
                                                 Bad,                                                                                            \
                                                 "UNUSED 27") _SEP_ _ROW_(28,                                                                    \
                                                                          NzTypeDTIDBitAddr,                                                     \
                                                                          int64,                                                                 \
                                                                          ALN64,                                                                 \
                                                                          8,                                                                     \
                                                                          INT,                                                                   \
                                                                          invalid,                                                               \
                                                                          Int8,                                                                  \
                                                                          "DTIDBitAddr (8 bytes)")                                               \
                                                 _SEP_ _ROW_(29,                                                                                 \
                                                             NzTypeSuperDouble,                                                                  \
                                                             cnum128,                                                                            \
                                                             ALN128,                                                                             \
                                                             16,                                                                                 \
                                                             FLOAT,                                                                              \
                                                             invalid,                                                                            \
                                                             SuperDouble,                                                                        \
                                                             "FP super double (16 bytes)")                                                       \
                                                     _SEP_ _ROW_(30,                                                                             \
                                                                 NzTypeJson,                                                                     \
                                                                 varA,                                                                           \
                                                                 FVARY,                                                                          \
                                                                 0,                                                                              \
                                                                 VARCHAR,                                                                        \
                                                                 varA,                                                                           \
                                                                 Bad,                                                                            \
                                                                 "JSON") _SEP_ _ROW_(31,                                                         \
                                                                                     NzTypeJsonb,                                                \
                                                                                     varA,                                                       \
                                                                                     FVARY,                                                      \
                                                                                     0,                                                          \
                                                                                     VARCHAR,                                                    \
                                                                                     varA,                                                       \
                                                                                     Bad,                                                        \
                                                                                     "JSONB")                                                    \
                                                         _SEP_ _ROW_(32,                                                                         \
                                                                     NzTypeJsonpath,                                                             \
                                                                     varA,                                                                       \
                                                                     FVARY,                                                                      \
                                                                     0,                                                                          \
                                                                     VARCHAR,                                                                    \
                                                                     varA,                                                                       \
                                                                     Bad,                                                                        \
                                                                     "JSONPATH")                                                                 \
                                                             _SEP_ _ROW_(                                                                        \
                                                                 33,                                                                             \
                                                                 NzTypeLastEntry,                                                                \
                                                                 void,                                                                           \
                                                                 0,                                                                              \
                                                                 0,                                                                              \
                                                                 INVALID,                                                                        \
                                                                 invalid,                                                                        \
                                                                 Bad,                                                                            \
                                                                 "UNUSED 33") /* Entries past                                                    \
                                                                                 NzTypeLastEntry                                                 \
                                                                                 are used only                                                   \
                                                                                 in zonemap                                                      \
                                                                                 code, via                                                       \
                                                                                 fieldTypeWithSize(),                                            \
                                                                                 and are not                                                     \
                                                                                 persistent.                                                     \
                                                                               */                                                                \
     _SEP_ _ROW_(                                                                                                                                \
         34,                                                                                                                                     \
         NzTypeChar1A,                                                                                                                           \
         str1A,                                                                                                                                  \
         ALNNO,                                                                                                                                  \
         1,                                                                                                                                      \
         FIXEDCHAR,                                                                                                                              \
         str1A,                                                                                                                                  \
         Bad,                                                                                                                                    \
         "ASCII Char[1] (1 byte)") _SEP_ _ROW_(35,                                                                                               \
                                               NzTypeChar2A,                                                                                     \
                                               str2A,                                                                                            \
                                               ALNNO,                                                                                            \
                                               2,                                                                                                \
                                               FIXEDCHAR,                                                                                        \
                                               str2A,                                                                                            \
                                               Bad,                                                                                              \
                                               "ASCII Char[2] (2 bytes)") _SEP_ _ROW_(36,                                                        \
                                                                                      NzTypeChar3A,                                              \
                                                                                      str3A,                                                     \
                                                                                      ALNNO,                                                     \
                                                                                      3,                                                         \
                                                                                      FIXEDCHAR,                                                 \
                                                                                      str3A,                                                     \
                                                                                      Bad,                                                       \
                                                                                      "ASCII "                                                   \
                                                                                      "Char[3] "                                                 \
                                                                                      "(3 bytes)") _SEP_                                         \
         _ROW_(37, NzTypeChar4A, str4A, ALNNO, 4, FIXEDCHAR, str4A, Bad, "ASCII Char[4] (4 bytes)") _SEP_ _ROW_(                                 \
             38,                                                                                                                                 \
             NzTypeChar5A,                                                                                                                       \
             str5A,                                                                                                                              \
             ALNNO,                                                                                                                              \
             5,                                                                                                                                  \
             FIXEDCHAR,                                                                                                                          \
             str5A,                                                                                                                              \
             Bad,                                                                                                                                \
             "ASCII Char[5] (5 bytes)") _SEP_ _ROW_(39,                                                                                          \
                                                    NzTypeChar6A,                                                                                \
                                                    str6A,                                                                                       \
                                                    ALNNO,                                                                                       \
                                                    6,                                                                                           \
                                                    FIXEDCHAR,                                                                                   \
                                                    str6A,                                                                                       \
                                                    Bad,                                                                                         \
                                                    "ASCII Char[6] (6 bytes)") _SEP_ _ROW_(40,                                                   \
                                                                                           NzTypeChar7A,                                         \
                                                                                           str7A,                                                \
                                                                                           ALNNO,                                                \
                                                                                           7,                                                    \
                                                                                           FIXEDCHAR,                                            \
                                                                                           str7A,                                                \
                                                                                           Bad,                                                  \
                                                                                           "ASCII"                                               \
                                                                                           " Char"                                               \
                                                                                           "[7] "                                                \
                                                                                           "(7 "                                                 \
                                                                                           "bytes"                                               \
                                                                                           ")") _SEP_                                            \
             _ROW_(41, NzTypeChar8A, str8A, ALNNO, 8, FIXEDCHAR, str8, Bad, "ASCII Char[8] (8 bytes)") _SEP_ _ROW_(                              \
                 42,                                                                                                                             \
                 NzTypeChar9A,                                                                                                                   \
                 str9A,                                                                                                                          \
                 ALNNO,                                                                                                                          \
                 9,                                                                                                                              \
                 FIXEDCHAR,                                                                                                                      \
                 str8,                                                                                                                           \
                 Bad,                                                                                                                            \
                 "ASCII Char[9] (9 bytes)") _SEP_                                                                                                \
                 _ROW_(43, NzTypeChar10A, str10A, ALNNO, 10, FIXEDCHAR, str8, Bad, "ASCII Char[10] (10 bytes)") _SEP_ _ROW_(                     \
                     44,                                                                                                                         \
                     NzTypeChar11A,                                                                                                              \
                     str11A,                                                                                                                     \
                     ALNNO,                                                                                                                      \
                     11,                                                                                                                         \
                     FIXEDCHAR,                                                                                                                  \
                     str8,                                                                                                                       \
                     Bad,                                                                                                                        \
                     "ASCII Char[11] (11 bytes)") _SEP_ _ROW_(45,                                                                                \
                                                              NzTypeChar12A,                                                                     \
                                                              str12A,                                                                            \
                                                              ALNNO,                                                                             \
                                                              12,                                                                                \
                                                              FIXEDCHAR,                                                                         \
                                                              str8,                                                                              \
                                                              Bad,                                                                               \
                                                              "ASCII Char[12] (12 bytes)") _SEP_                                                 \
                     _ROW_(46, NzTypeChar13A, str13A, ALNNO, 13, FIXEDCHAR, str8, Bad, "ASCII Char[13] (13 bytes)") _SEP_ _ROW_(                 \
                         47,                                                                                                                     \
                         NzTypeChar14A,                                                                                                          \
                         str14A,                                                                                                                 \
                         ALNNO,                                                                                                                  \
                         14,                                                                                                                     \
                         FIXEDCHAR,                                                                                                              \
                         str8,                                                                                                                   \
                         Bad,                                                                                                                    \
                         "ASCII Char[14] (14 bytes)") _SEP_                                                                                      \
                         _ROW_(48, NzTypeChar15A, str15A, ALNNO, 15, FIXEDCHAR, str8, Bad, "ASCII Char[15] (15 bytes)") _SEP_ _ROW_(             \
                             49,                                                                                                                 \
                             NzTypeChar16A,                                                                                                      \
                             str16A,                                                                                                             \
                             ALNNO,                                                                                                              \
                             16,                                                                                                                 \
                             FIXEDCHAR,                                                                                                          \
                             str8,                                                                                                               \
                             Bad,                                                                                                                \
                             "ASCII Char[16] (16 bytes)") _SEP_                                                                                  \
                             _ROW_(50,                                                                                                           \
                                   NzTypeChar1E,                                                                                                 \
                                   str1E,                                                                                                        \
                                   ALNNO,                                                                                                        \
                                   1,                                                                                                            \
                                   FIXEDCHAR,                                                                                                    \
                                   str1E,                                                                                                        \
                                   Bad,                                                                                                          \
                                   "EBCDIC Char[1] (1 byte)") _SEP_ _ROW_(51,                                                                    \
                                                                          NzTypeChar2E,                                                          \
                                                                          str2E,                                                                 \
                                                                          ALNNO,                                                                 \
                                                                          2,                                                                     \
                                                                          FIXEDCHAR,                                                             \
                                                                          str2E,                                                                 \
                                                                          Bad,                                                                   \
                                                                          "EBCDIC Char[2] (2 "                                                   \
                                                                          "bytes)") _SEP_ _ROW_(52,                                              \
                                                                                                NzTypeChar3E,                                    \
                                                                                                str3E,                                           \
                                                                                                ALNNO,                                           \
                                                                                                3,                                               \
                                                                                                FIXEDCHAR,                                       \
                                                                                                str3E,                                           \
                                                                                                Bad,                                             \
                                                                                                "EBCDIC Char[3] (3 bytes)") _SEP_                \
                                 _ROW_(53, NzTypeChar4E, str4E, ALNNO, 4, FIXEDCHAR, str4E, Bad, "EBCDIC Char[4] (4 bytes)") _SEP_ _ROW_(        \
                                     54,                                                                                                         \
                                     NzTypeChar5E,                                                                                               \
                                     str5E,                                                                                                      \
                                     ALNNO,                                                                                                      \
                                     5,                                                                                                          \
                                     FIXEDCHAR,                                                                                                  \
                                     str5E,                                                                                                      \
                                     Bad,                                                                                                        \
                                     "EBCDIC Char[5] (5 bytes)") _SEP_                                                                           \
                                     _ROW_(55, NzTypeChar6E, str6E, ALNNO, 6, FIXEDCHAR, str6E, Bad, "EBCDIC Char[6] (6 bytes)") _SEP_ _ROW_(    \
                                         56,                                                                                                     \
                                         NzTypeChar7E,                                                                                           \
                                         str7E,                                                                                                  \
                                         ALNNO,                                                                                                  \
                                         7,                                                                                                      \
                                         FIXEDCHAR,                                                                                              \
                                         str7E,                                                                                                  \
                                         Bad,                                                                                                    \
                                         "EBCDIC Char[7] (7 bytes)")                                                                             \
                                         _SEP_ _ROW_(                                                                                            \
                                             57,                                                                                                 \
                                             NzTypeChar8E,                                                                                       \
                                             str8E,                                                                                              \
                                             ALNNO,                                                                                              \
                                             8,                                                                                                  \
                                             FIXEDCHAR,                                                                                          \
                                             str8,                                                                                               \
                                             Bad,                                                                                                \
                                             "EBCDIC Char[8] (8 bytes)")                                                                         \
                                             _SEP_ _ROW_(                                                                                        \
                                                 58,                                                                                             \
                                                 NzTypeChar9E,                                                                                   \
                                                 str9E,                                                                                          \
                                                 ALNNO,                                                                                          \
                                                 9,                                                                                              \
                                                 FIXEDCHAR,                                                                                      \
                                                 str8,                                                                                           \
                                                 Bad,                                                                                            \
                                                 "EBCDIC Char[9] (9 bytes)") _SEP_                                                               \
                                                 _ROW_(59, NzTypeChar10E, str10E, ALNNO, 10, FIXEDCHAR, str8, Bad, "EBCDIC Char[10] (10 bytes)") \
                                                     _SEP_ _ROW_(                                                                                \
                                                         60,                                                                                     \
                                                         NzTypeChar11E,                                                                          \
                                                         str11E,                                                                                 \
                                                         ALNNO,                                                                                  \
                                                         11,                                                                                     \
                                                         FIXEDCHAR,                                                                              \
                                                         str8,                                                                                   \
                                                         Bad,                                                                                    \
                                                         "EBCDIC Char[11] (11 bytes)")                                                           \
                                                         _SEP_ _ROW_(                                                                            \
                                                             61,                                                                                 \
                                                             NzTypeChar12E,                                                                      \
                                                             str12E,                                                                             \
                                                             ALNNO,                                                                              \
                                                             12,                                                                                 \
                                                             FIXEDCHAR,                                                                          \
                                                             str8,                                                                               \
                                                             Bad,                                                                                \
                                                             "EBCDIC Char[12] (12 "                                                              \
                                                             "bytes)")                                                                           \
                                                             _SEP_ _ROW_(                                                                        \
                                                                 62,                                                                             \
                                                                 NzTypeChar13E,                                                                  \
                                                                 str13E,                                                                         \
                                                                 ALNNO,                                                                          \
                                                                 13,                                                                             \
                                                                 FIXEDCHAR,                                                                      \
                                                                 str8,                                                                           \
                                                                 Bad,                                                                            \
                                                                 "EBCDIC Char[13] (13 "                                                          \
                                                                 "bytes)")                                                                       \
                                                                 _SEP_ _ROW_(                                                                    \
                                                                     63,                                                                         \
                                                                     NzTypeChar14E,                                                              \
                                                                     str14E,                                                                     \
                                                                     ALNNO,                                                                      \
                                                                     14,                                                                         \
                                                                     FIXEDCHAR,                                                                  \
                                                                     str8,                                                                       \
                                                                     Bad,                                                                        \
                                                                     "EBCDIC Char[14] "                                                          \
                                                                     "(14 bytes)")                                                               \
                                                                     _SEP_ _ROW_(                                                                \
                                                                         64,                                                                     \
                                                                         NzTypeChar15E,                                                          \
                                                                         str15E,                                                                 \
                                                                         ALNNO,                                                                  \
                                                                         15,                                                                     \
                                                                         FIXEDCHAR,                                                              \
                                                                         str8,                                                                   \
                                                                         Bad,                                                                    \
                                                                         "EBCDIC "                                                               \
                                                                         "Char[15] (15 "                                                         \
                                                                         "bytes)")                                                               \
                                                                         _SEP_ _ROW_(                                                            \
                                                                             65,                                                                 \
                                                                             NzTypeChar16E,                                                      \
                                                                             str16E,                                                             \
                                                                             ALNNO,                                                              \
                                                                             16,                                                                 \
                                                                             FIXEDCHAR,                                                          \
                                                                             str8,                                                               \
                                                                             Bad,                                                                \
                                                                             "EBCDIC "                                                           \
                                                                             "Char[16] "                                                         \
                                                                             "(16 bytes)")                                                       \
                                                                             _SEP_ _ROW_(                                                        \
                                                                                 66,                                                             \
                                                                                 NzTypeVarCharE,                                                 \
                                                                                 varE,                                                           \
                                                                                 FVARY,                                                          \
                                                                                 0,                                                              \
                                                                                 VARCHAR,                                                        \
                                                                                 varE,                                                           \
                                                                                 Bad,                                                            \
                                                                                 "EBCDIC "                                                       \
                                                                                 "Char "                                                         \
                                                                                 "Varyin"                                                        \
                                                                                 "g")                                                            \
                                                                                 _SEP_ _ROW_(                                                    \
                                                                                     67,                                                         \
                                                                                     NzTypeVarFixedCharE,                                        \
                                                                                     varE,                                                       \
                                                                                     FVARY,                                                      \
                                                                                     0,                                                          \
                                                                                     VARCHAR,                                                    \
                                                                                     varE,                                                       \
                                                                                     Bad,                                                        \
                                                                                     "EBC"                                                       \
                                                                                     "DIC"                                                       \
                                                                                     " Ch"                                                       \
                                                                                     "ar "                                                       \
                                                                                     "(us"                                                       \
                                                                                     "ing"                                                       \
                                                                                     " va"                                                       \
                                                                                     "ryi"                                                       \
                                                                                     "ng"                                                        \
                                                                                     ")")                                                        \
                                                                                     _SEP_ _ROW_(                                                \
                                                                                         68,                                                     \
                                                                                         NzTypeNumeric4,                                         \
                                                                                         int32,                                                  \
                                                                                         ALN32,                                                  \
                                                                                         4,                                                      \
                                                                                         NUMERIC,                                                \
                                                                                         int32,                                                  \
                                                                                         Bad,                                                    \
                                                                                         "CNumeri"                                               \
                                                                                         "c32 (4 "                                               \
                                                                                         "bytes)")                                               \
                                                                                         _SEP_ _ROW_(                                            \
                                                                                             69,                                                 \
                                                                                             NzTypeNumeric8,                                     \
                                                                                             cnum64,                                             \
                                                                                             ALN64,                                              \
                                                                                             8,                                                  \
                                                                                             NUMERIC,                                            \
                                                                                             cnum64,                                             \
                                                                                             Bad,                                                \
                                                                                             "CNu"                                               \
                                                                                             "mer"                                               \
                                                                                             "ic6"                                               \
                                                                                             "4 "                                                \
                                                                                             "(8 "                                               \
                                                                                             "byt"                                               \
                                                                                             "es"                                                \
                                                                                             ")") _SEP_                                          \
                                                                                             _ROW_(                                              \
                                                                                                 70,                                             \
                                                                                                 NzTypeNumeric16,                                \
                                                                                                 cnum128,                                        \
                                                                                                 ALN128,                                         \
                                                                                                 16,                                             \
                                                                                                 NUMERIC,                                        \
                                                                                                 cnum128,                                        \
                                                                                                 Bad,                                            \
                                                                                                 "CNumeric128 (16 bytes)")

// Define the EFieldType enumeration
#define _ROW_(ORDINAL, ENUMERATOR, REP, ALIGN, FIXED, SCHEMA, ZMACCUM, COMPARE, DESCRIPTION) \
 ENUMERATOR
#define _SEP_ ,
enum EFieldType
{
    NZ_FIELD_TYPE_LIST
};
#undef _ROW_
#undef _SEP_

#ifdef __cplusplus

EFieldType fieldTypeWithSize(EFieldType ft, unsigned bytes, bool padEbcdic);
EFieldType fieldTypeWithSize(struct field_t const* f);

#endif

#endif // NZFIELDTYPE_H
