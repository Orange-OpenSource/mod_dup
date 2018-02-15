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
import time
import datetime

class RequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    ### self.path self.headers posdup_body => URL HEADERS AND BODY OF MOD_DUP RESPONSE
    def do_ALL(self):
        try:
            length = int(self.headers.getheader('content-length'))
        except:
            length = 0
        posdup_body = self.rfile.read(length)
        if 'SID=DUPSLEEP' in self.path.upper():
            time.sleep(2)
            # we need to rstrip the header lines to remove the trailing newline
            self.server.queue.put((self.path, [line.rstrip() for line in self.headers.headers], posdup_body, self.server.server_port))
            return

        self.send_response(200)
        self.end_headers()
        self.wfile.write(posdup_body)
        self.server.queue.put((self.path, [line.rstrip() for line in self.headers.headers], posdup_body, self.server.server_port))
        return

    def do_GET(self):
        return self.do_ALL()

    def do_POST(self):
        return self.do_ALL()

def http_server(q, host, port):
    server = BaseHTTPServer.HTTPServer((host, port), RequestHandler)
    server.queue = q
    server.serve_forever()

class DupRequest:
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
        self.desc = self.consume(f, "==REQURL==")
        self.path = self.consume(f, "==REQBODY==")
        self.body = self.consume(f, "==DUPURL==")
        self.dup_path = self.consume(f, "==DUPHEADER==")
        self.dup_header = self.consumeIntoList(f, "==DUPBODY==")
        self.dup_body = self.consume(f, "==RESPBODY==")
        self.resp_body = self.consume(f, "==DUPDESTPORT==")
        self.dup_dest = self.consume(f, "==EOF==")
        f.close()
        # response header and body (not from mod_dup but from the web service)
        self.response_headers = list()
        self.response_body = cStringIO.StringIO()

    def __str__(self):
        return '''Request\n
  desc: %s
  path: %s
  body: %s
  dup_path: %s
  dup_body: %s''' % (self.desc, self.path, self.body, self.dup_path, self.dup_body)

    ## RETURN CONCATENATION OF ALL THE LINES CONSUMED
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

    ## RETURN ALL THE LINES CONSUMED AS A LIST
    def consumeIntoList(self, f, key=''):
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
        return lines

    def get_url(self):
        return "http://%s:%d%s" % (self.host, self.port, self.path)

    def header_handler(self, header_line):
        header_line = header_line.decode('iso-8859-1')
        if ':' not in header_line:
            return
        self.response_headers.append(str(header_line).rstrip())

    ## EXECUTE THE CURL REQUEST
    def execute(self, curl, verbose=False):
        if verbose:
            print "Url:", self.get_url()
        curl.setopt(curl.URL, self.get_url())
        curl.setopt(curl.HEADERFUNCTION, self.header_handler) #pycurl executes header_handler on each line of the http header
        curl.setopt(curl.WRITEFUNCTION, self.response_body.write) #pycurl executes write on self.response_body for the body
        curl.setopt(curl.FOLLOWLOCATION, 1);
        if (len(self.body)):
            curl.setopt(curl.POST, 1)
            curl.setopt(curl.HTTPHEADER, ['Content-Type: application/json; charset=utf-8',
                                          'Content-Length: ' + str(len(self.body))] )
            curl.setopt(curl.POSTFIELDSIZE, len(self.body))
            curl.setopt(curl.POSTFIELDS, self.body)
        assert not curl.perform()

    ## MAKE THE ASSERTIONS/COMPARISONS
    def assert_received(self, path, headers, body, server_port):
        # if (self.dup_dest == "MULTI")
        if (len(self.dup_dest) and self.dup_dest != "MULTI"):
            assert server_port == 16555 ,  "########### SHOULD BE ON SECOND LOCATION  ###############"
        elif not len(self.dup_dest):
            assert  server_port != 16555 ,  "########### SHOULD BE ON FIRST LOCATION  ###############"
        assert self.dup_path, '''Unexpected request received
               path: %s
               body: %s''' % (path, body)
        assert self.dup_path and path == self.dup_path ,\
                '''Path did not match:
                   path: %s
                   dup_path: %s''' % (path, self.dup_path)
        assert re.search(self.dup_body, body, re.MULTILINE|re.DOTALL),\
                 '''Body did not match:
                 path: %s
                 body: %s''' % (path, body)
        # check headers
        for headerLineExpected in self.dup_header: # iterate through expected header lines (defined by ==DUPHEADER==)
            contained = False
            for headerLineActual in headers: # iterate through received header lines
                if re.search(headerLineExpected,headerLineActual) != None:
                    contained = True # header line is contained, OK
                    break
            assert contained,'''Header not found: %s\n\nHeaders sent:\n%s''' % (headerLineExpected, '\n'.join(headers))

    def assert_not_received(self):
		#if you want not duplicated to be ok, you need DUPURL and DUPBODY empty in your test req
        assert not self.dup_path and not self.dup_body, 'Request not duplicated where it should!'

def run_tests(request_files, queue, options):

    for r_fname in request_files:
        print "Test:", r_fname
        curl = pycurl.Curl()
        request = DupRequest(r_fname, options.host, int(options.port))
        startReqTime = time.time()
        request.execute(curl, verbose=options.verbose)
        elapsedTimeForOriginalRequest = int((time.time() - startReqTime)*1000) # response time in ms (should be around 1500, according to dup_test.conf)

        if 'SID=SLEEP' in request.path:
            assert elapsedTimeForOriginalRequest < 1550 and elapsedTimeForOriginalRequest > 1499, 'Timeout test failed, original request response did not respect the DupTimeout (1500 in current conf), effective %d' % elapsedTimeForOriginalRequest 

        if (len(request.resp_body)):
            assert request.resp_body == request.response_body.getvalue().rstrip(), '''Response mismatch:
   expected: %s
   received: %s''' % (request.resp_body, request.response_body.getvalue())
        if not options.curl_only:
            try:
                try:
                    path, headers, body, server_port = queue.get(timeout=2)
                    request.assert_received(path, headers, body, server_port)
                    if (request.dup_dest == "MULTI"):
                        # second extraction from the queue
                        print('get second extract from queue')
                        path2, headers2, body2, server_port2 = queue.get(timeout=2)
                        assert server_port != server_port2, "Multi sent on the same location"
                        request.assert_received(path2, headers2, body2, server_port2)

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
        request = DupRequest(r_fname, '', int(8000))
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
    defauldup_path = os.path.join(os.getcwd(), os.path.dirname(__file__), "data/dup/requests")

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
                        help='The folder containing the request files', default=defauldup_path)

    main(*arg_parser.parse_args())
