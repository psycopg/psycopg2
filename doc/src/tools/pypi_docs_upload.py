# -*- coding: utf-8 -*-
"""
    Standalone script to upload a project docs on PyPI

    Hacked together from the following distutils extension, avaliable from
    https://bitbucket.org/jezdez/sphinx-pypi-upload/overview (ver. 0.2.1)

    sphinx_pypi_upload
    ~~~~~~~~~~~~~~~~~~

    setuptools command for uploading Sphinx documentation to PyPI

    :author: Jannis Leidel
    :contact: jannis@leidel.info
    :copyright: Copyright 2009, Jannis Leidel.
    :license: BSD, see LICENSE for details.
"""

import os
import sys
import socket
import zipfile
import httplib
import base64
import urlparse
import tempfile
import cStringIO as StringIO
from ConfigParser import ConfigParser

from distutils import log
from distutils.command.upload import upload
from distutils.errors import DistutilsOptionError

class UploadDoc(object):
    """Distutils command to upload Sphinx documentation."""
    def __init__(self, name, upload_dir, repository=None):
        self.name = name
        self.upload_dir = upload_dir

        p = ConfigParser()
        p.read(os.path.expanduser('~/.pypirc'))
        self.username = p.get('pypi', 'username')
        self.password = p.get('pypi', 'password')

        self.show_response = False
        self.repository = repository or upload.DEFAULT_REPOSITORY

    def create_zipfile(self):
        # name = self.distribution.metadata.get_name()
        name = self.name
        tmp_dir = tempfile.mkdtemp()
        tmp_file = os.path.join(tmp_dir, "%s.zip" % name)
        zip_file = zipfile.ZipFile(tmp_file, "w")
        for root, dirs, files in os.walk(self.upload_dir):
            if not files:
                raise DistutilsOptionError, \
                      "no files found in upload directory '%s'" % self.upload_dir
            for name in files:
                full = os.path.join(root, name)
                relative = root[len(self.upload_dir):].lstrip(os.path.sep)
                dest = os.path.join(relative, name)
                zip_file.write(full, dest)
        zip_file.close()
        return tmp_file

    def upload_file(self, filename):
        content = open(filename,'rb').read()
        # meta = self.distribution.metadata
        data = {
            ':action': 'doc_upload',
            'name': self.name, # meta.get_name(),
            'content': (os.path.basename(filename),content),
        }
        # set up the authentication
        auth = "Basic " + base64.encodestring(self.username + ":" + self.password).strip()

        # Build up the MIME payload for the POST data
        boundary = '--------------GHSKFJDLGDS7543FJKLFHRE75642756743254'
        sep_boundary = '\n--' + boundary
        end_boundary = sep_boundary + '--'
        body = StringIO.StringIO()
        for key, value in data.items():
            # handle multiple entries for the same name
            if type(value) != type([]):
                value = [value]
            for value in value:
                if type(value) is tuple:
                    fn = ';filename="%s"' % value[0]
                    value = value[1]
                else:
                    fn = ""
                value = str(value)
                body.write(sep_boundary)
                body.write('\nContent-Disposition: form-data; name="%s"'%key)
                body.write(fn)
                body.write("\n\n")
                body.write(value)
                if value and value[-1] == '\r':
                    body.write('\n')  # write an extra newline (lurve Macs)
        body.write(end_boundary)
        body.write("\n")
        body = body.getvalue()

        self.announce("Submitting documentation to %s" % (self.repository), log.INFO)

        # build the Request
        # We can't use urllib2 since we need to send the Basic
        # auth right with the first request
        schema, netloc, url, params, query, fragments = \
            urlparse.urlparse(self.repository)
        assert not params and not query and not fragments
        if schema == 'http':
            http = httplib.HTTPConnection(netloc)
        elif schema == 'https':
            http = httplib.HTTPSConnection(netloc)
        else:
            raise AssertionError, "unsupported schema "+schema

        data = ''
        loglevel = log.INFO
        try:
            http.connect()
            http.putrequest("POST", url)
            http.putheader('Content-type',
                           'multipart/form-data; boundary=%s'%boundary)
            http.putheader('Content-length', str(len(body)))
            http.putheader('Authorization', auth)
            http.endheaders()
            http.send(body)
        except socket.error, e:
            self.announce(str(e), log.ERROR)
            return

        response = http.getresponse()
        if response.status == 200:
            self.announce('Server response (%s): %s' % (response.status, response.reason),
                          log.INFO)
        elif response.status == 301:
            location = response.getheader('Location')
            if location is None:
                location = 'http://packages.python.org/%s/' % self.name # meta.get_name()
            self.announce('Upload successful. Visit %s' % location,
                          log.INFO)
        else:
            self.announce('Upload failed (%s): %s' % (response.status, response.reason),
                          log.ERROR)
        if self.show_response:
            print '-'*75, response.read(), '-'*75

    def run(self):
        zip_file = self.create_zipfile()
        self.upload_file(zip_file)
        os.remove(zip_file)

    def announce(self, msg, *args, **kwargs):
        print msg

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print >>sys.stderr, "usage: %s PROJECT UPLOAD_DIR" % sys.argv[0]
        sys.exit(2)

    project, upload_dir = sys.argv[1:]
    up = UploadDoc(project, upload_dir=upload_dir)
    up.run()

