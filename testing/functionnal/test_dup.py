#!/usr/bin/env python
#
# Mod Tee functionnal tests
#
# To use this script, see ./run.py -h

import os, glob, time
import cStringIO
import optparse
import pycurl
import BaseHTTPServer
import multiprocessing
import Queue
import re
import sys
import urllib

class RequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):

    def do_GET(self):
        post_body = ''.join(iter(self.rfile.read, ''))
        self.server.queue.put(('POST', self.path, post_body, self.server.server_port))
        # FIXME: why is the pipe broken at this point?
        #self.send_response(200)

    def do_POST(self):
        post_body = ''.join(iter(self.rfile.read, ''))
        self.server.queue.put(('GET', self.path, post_body, self.server.server_port))
        # FIXME: why is the pipe broken at this point?
        #self.send_response(200)


def http_server(q, host, port):
    server = BaseHTTPServer.HTTPServer((host, port), RequestHandler)
    server.queue = q
    server.serve_forever()

class TeeRequest:
    def __init__(self, filename, host, port):
        self.filename = filename
        self.host, self.port = host, port
        self.parse()

    def parse(self):
        """
        Parses the request file
        """
        f = open(self.filename, 'r')
        _ = self.consume(f, '==DESC==')
        self.desc = self.consume(f, "==HEADER==")
        self.path = self.consume(f, "==BODY==")
        self.body = self.consume(f, "==THEADER==")
        self.t_path = self.consume(f, "==TBODY==")
        self.t_body = self.consume(f, "==RBODY==")
        self.r_body = self.consume(f, "==DEST==")
        self.t_dest = self.consume(f, "==EOF==")
        f.close()

    def __str__(self):
        return '''Request\n
  desc: %s
  path: %s
  body: %s
  t_path: %s
  t_body: %s''' % (self.desc, self.path, self.body, self.t_path, self.t_body)

    def consume(self, f, key=''):
        lines = []
        while 1:
            line = f.readline()
            if line == '':
                line = '==EOF=='
                if key != line:
                    raise Exception('EOF while trying to consume %s' % key)
            line = line.strip()
            if line == key:
                break
            lines.append(line)
        return ''.join(lines)

    def get_url(self):
        return "http://%s:%d%s" % (self.host, self.port, self.path)

    def play_with_curl(self, curl, verbose=False):
        buf = cStringIO.StringIO()

        if verbose:
            print "Url:", self.get_url()
        curl.setopt(curl.URL, self.get_url())
        curl.setopt(curl.WRITEFUNCTION, buf.write)
        curl.setopt(curl.FOLLOWLOCATION, 1);
        if (len(self.body)):
            curl.setopt(curl.POST, 1)
            curl.setopt(curl.HTTPHEADER, ['Content-Type: text/xml; charset=utf-8',
                                          'Content-Length: ' + str(len(self.body))] )
            curl.setopt(curl.POSTFIELDSIZE, len(self.body))
            curl.setopt(curl.POSTFIELDS, self.body)
        assert not curl.perform()
        return buf

    def assert_received(self, path, body, server_port):
        # if (self.t_dest == "MULTI")
        if (len(self.t_dest) and self.t_dest != "MULTI"):
            assert server_port == 16555 ,  "########### SHOULD BE ON SECOND LOCATION  ###############"
        elif not len(self.t_dest):
            assert  server_port != 16555 ,  "########### SHOULD BE ON FIRST LOCATION  ###############"
        assert self.t_path, '''Unexpected request received
               path: %s
               body: %s''' % (path, body)
        assert self.t_path and path == self.t_path ,\
                '''Path did not match:
                   path: %s
                   body: %s''' % (path, body)
        assert re.search(self.t_body, body, re.MULTILINE|re.DOTALL),\
                 '''Body did not match:
                 path: %s
                 body: %s''' % (path, body)

    def assert_not_received(self):
        assert not self.t_path and not self.t_body, 'Request not duplicated'

def run_tests(request_files, queue, options):

    for r_fname in request_files:
        print "Test:", r_fname
        curl = pycurl.Curl()
        request = TeeRequest(r_fname, options.host, int(options.port))
        response = request.play_with_curl(curl, verbose=options.verbose)

        if (len(request.r_body)):
            assert request.r_body == response.getvalue().rstrip(), '''Response mismatch:
   expected: %s
   received: %s''' % (request.r_body, response.getvalue())
        if not options.curl_only:
            try:
                try:
                    method, path, body, server_port = queue.get(timeout=3)
                    request.assert_received(path, body, server_port)
                    if (request.t_dest == "MULTI"):
                        # second extraction from the queue
                        method2, path2, body2, server_port2 = queue.get(timeout=3)
                        assert server_port != server_port2, "Multi sent on the same location"
                        request.assert_received(path2, body2, server_port2)

                except Queue.Empty:
                    request.assert_not_received()

            except AssertionError, err:
                print "########### RECEIVED ###############"
                print "Error:", err
                print "************************************"
                print "########### EXPECTED ###############"
                print "Expected:", request
                exit(1)


def display_test_content(request_files):
    for r_fname in request_files:
        request = TeeRequest(r_fname, '', int(8000))
        print ("Test: %s " % r_fname + request.desc)

def main(options, args):
    # Stats the request files
    request_file_pattern = os.path.join(options.path, options.test)
    request_files = glob.glob(request_file_pattern)
    request_files.sort()
    if not request_files:
        sys.stdout.write ("No files matching %s" % request_file_pattern)
    else:

        if options.desc:
            display_test_content(request_files)
            exit(0)
        process = None
        queue = None
        try:
            if not options.curl_only:
                queue = multiprocessing.Queue()
                ## First process
                process = multiprocessing.Process(target=http_server, args=(queue, options.dest_host, options.dest_port))
                process.start()
                ## Second process on port + 1
                process2 = multiprocessing.Process(target=http_server, args=(queue, options.dest_host, 16555))
                process2.start()
                # We need to wait for the server to start up...
                time.sleep(2)
            run_tests(request_files, queue, options)
        except Exception as err_msg:
            print err_msg
            exit(0)
        finally:
            if not options.curl_only:
                process.terminate()
                process2.terminate()
                process.join()
                process2.join()


if __name__ == '__main__':
    # By default, we use the test files in the data folder next to this script
    default_path = os.path.join(os.getcwd(), os.path.dirname(__file__), "data")

    # Option parsing
    arg_parser = optparse.OptionParser()
    arg_parser.add_option('--test', dest='test',
                        help='Pattern of test file(s) to run', default='*.req')
    arg_parser.add_option('--curl', dest='curl_only', action='store_true',
                        help='Only plays the request with curl', default=False)
    arg_parser.add_option('--verbose', dest='verbose', action='store_true',
                        help='More verbose output', default=False)
    arg_parser.add_option('--server', dest='host',
                        help='The server to send the requests to', default="localhost")
    arg_parser.add_option('--port', dest='port',
                        help='The port to send the requests to', default=8042)
    arg_parser.add_option('--desc', dest='desc',
                        help='Display the tests actions and exit', default=False)
    arg_parser.add_option('--dest_server', dest='dest_host',
                        help='The destination server of mod_tee.', default="localhost")
    arg_parser.add_option('--dest_port', dest='dest_port',
                        help='The destination port of mod_tee', default=8043)
    arg_parser.add_option('--path', dest='path',
                        help='The folder containing the request files', default=default_path)

    main(*arg_parser.parse_args())
