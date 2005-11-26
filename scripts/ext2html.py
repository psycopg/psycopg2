#!/usr/bin/env python

# Author: Daniele Varrazzo
# Contact: daniele dot varrazzo at gmail dot com
# Revision: $Revision$
# Date: $Date$
# Copyright: This module has been placed in the public domain.

"""
A minimal front end to the Docutils Publisher, producing HTML.

Output can refer to Epydoc-generated APIs through the iterpreted text role
"api".
"""

import types
import sys

# The url fragment where the api "index.html" resides w.r.t. the generated docs
api_root = "api/"

try:
    import locale
    locale.setlocale(locale.LC_ALL, '')
except:
    pass

from docutils.core import publish_cmdline, default_description
from docutils.parsers.rst.roles import register_canonical_role
from docutils import nodes, utils

# api references are searched for in these modules
api_modules = [
    'psycopg2',
    'psycopg2._psycopg',
    'psycopg2.extensions',
]

# name starting with a dot are looking as those objects attributes.
searched_objects = [
    # module_name, object_name
    ('psycopg2.extensions', 'connection'),
    ('psycopg2.extensions', 'cursor'),
]

# import all the referenced modules
for modname in api_modules:
    __import__(modname)
    
class EpydocTarget:
    """Representation of an element language."""
    def __init__(self, name, element):
        self.name = name
        
        # The python object described
        self.element = element
    
        # The base name of the page
        self.page = None
        
        # The url fragment
        self.fragment = None
        
    def get_url(self):
        # is it a private element?
        components = self.page.split('.')
        if self.fragment: components.append(self.fragment)
            
        for component in components:
            if component.startswith('_'):
                private = True
                break
        else:
            private = False
            
        ref = api_root + (private and "private/" or "public/") \
            + self.page + "-" + self.get_type() + ".html"
        if self.fragment:
            ref = ref + "#" + self.fragment
        
        return ref

    def get_type(self):
        # detect the element type
        if isinstance(self.element, types.TypeType):
            return 'class'
        elif isinstance(self.element, types.ModuleType):
            return 'module'
        else:
            raise ValueError("Can't choose a type for '%s'." % self.name)
            
def filter_par(name):
    """Filter parenthesis away from a name."""
    if name.endswith(")"):
        return name.split("(")[0]
    else:
        return name
        
def get_element_target(name):
    """Return the life, the death, the miracles about a package element."""
    
    name = filter_par(name)
    
    if name.startswith('.'):
        for modname, objname in searched_objects:
            if hasattr(getattr(sys.modules[modname], objname), name[1:]):
                name = objname + name
                break
        
    # is the element a module?
    if name in api_modules:
        out = EpydocTarget(name, sys.modules[name])
        out.page = name
        return out
        
    # look for the element in some module
    for modname in api_modules:
        element = getattr(sys.modules[modname], name, None)
        if element is not None:
            
            # Check if it is a function defined in a module
            if isinstance(element, 
                    (int, types.FunctionType, types.BuiltinFunctionType)):
                out = EpydocTarget(name, sys.modules[modname])
                out.page = modname
                out.fragment = name
            else:
                out = EpydocTarget(name, element)
                out.page = modname + '.' + name
                
            return out
    
    # maybe a qualified name?
    if '.' in name:
        out = get_element_target('.'.join(name.split('.')[:-1]))
        if out is not None:
            out.fragment = filter_par(name.split('.')[-1])
            return out
                
    raise ValueError("Can't find '%s' in any provided module." % name)
    
def api_role(role, rawtext, text, lineno, inliner,
                       options={}, content=[]):
    try:
        target = get_element_target(text)
    except Exception, exc:
        msg = inliner.reporter.error(str(exc), line=lineno)
        prb = inliner.problematic(rawtext, rawtext, msg)
        return [prb], [msg]
        
    ref = target.get_url()
    node2 = nodes.literal(rawtext, utils.unescape(text))
    node = nodes.reference(rawtext, '', node2, refuri=ref,
                           **options)
    return [node], []


register_canonical_role('api', api_role)

# Register the 'api' role as canonical role
from docutils.parsers.rst import roles 
roles.DEFAULT_INTERPRETED_ROLE = 'api'


description = ('Generates (X)HTML documents from standalone reStructuredText '
               'sources with links to Epydoc API.  ' + default_description)


publish_cmdline(writer_name='html', description=description)
