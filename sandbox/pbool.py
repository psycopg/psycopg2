class B(object):
    def __init__(self, x):
        if x: self._o = True
        else: self._o = False
    def __getattribute__(self, attr):
        print "ga called", attr
        return object.__getattribute__(self, attr)
    def _sqlquote(self):
        if self._o == True:
            return 'It is True'
        else:
            return 'It is False'
        
