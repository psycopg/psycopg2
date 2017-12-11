import sys

if sys.version_info[0] == 2:
    # Python 2
    string_types = basestring,
    text_type = unicode
else:
    # Python 3
    string_types = str,
    text_type = str
