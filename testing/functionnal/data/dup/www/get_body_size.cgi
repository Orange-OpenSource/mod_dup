#!/usr/bin/env python

import cgi
import cgitb
import sys

cgitb.enable()  # for troubleshooting

print "Content-type: text/html"
print ""

# read the body from stdin
body = sys.stdin.read()
print len(body)
