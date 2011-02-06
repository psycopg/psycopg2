"""Fixer to change b('string') into b'string'."""
# Author: Daniele Varrazzo

import token
from lib2to3 import fixer_base
from lib2to3.pytree import Leaf

class FixB(fixer_base.BaseFix):

    PATTERN = """
              power< wrapper='b' trailer< '(' arg=[any] ')' > rest=any* >
              """

    def transform(self, node, results):
        arg = results['arg']
        wrapper = results["wrapper"]
        if len(arg) == 1 and arg[0].type == token.STRING:
            b = Leaf(token.STRING, 'b' + arg[0].value, prefix=wrapper.prefix)
            node.children = [ b ] + results['rest']

