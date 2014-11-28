import sys

PY3 = sys.version_info[0] == 3


if PY3:
    import _thread

    def iteritems(d, **kw):
        return iter(d.items(**kw))

    thread = _thread
else:
    import thread as _thread

    def iteritems(d, **kw):
        return iter(d.iteritems(**kw))

    thread = _thread
