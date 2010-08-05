# -*- coding: utf-8 -*-
"""
    extension
    ~~~~~~~~~

    A directive to create a box warning that a certain bit of Psycopg is an
    extension to the DBAPI 2.0.

    :copyright: Copyright 2010 by Daniele Varrazzo.
"""

from docutils import nodes

from sphinx.locale import _
from sphinx.util.compat import Directive, make_admonition

class extension_node(nodes.Admonition, nodes.Element): pass


class Extension(Directive):
    """
    An extension entry, displayed as an admonition.
    """

    has_content = True
    required_arguments = 0
    optional_arguments = 0
    final_argument_whitespace = False
    option_spec = {}

    def run(self):
        nodes = make_admonition(extension_node,
                self.name, [_('DB API extension')], self.options,
                self.content, self.lineno, self.content_offset,
                self.block_text, self.state, self.state_machine)
        nodes[0]['classes'].append('dbapi-extension')
        return nodes


def visit_extension_node(self, node):
    self.visit_admonition(node)

def depart_extension_node(self, node):
    self.depart_admonition(node)

def setup(app):
    app.add_node(extension_node,
                 html=(visit_extension_node, depart_extension_node),
                 latex=(visit_extension_node, depart_extension_node),
                 text=(visit_extension_node, depart_extension_node))

    app.add_directive('extension', Extension)

