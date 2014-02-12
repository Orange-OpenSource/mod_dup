#!/bin/sh

BIN=`cd $1; echo $PWD`
CONF=`cd $2; echo $PWD`
ROOT=`cd $1/..; echo $PWD`
APACHE_DIR=$ROOT/apache2
APACHE_SRC=$APACHE_DIR/src
APACHE_MODS=/usr/lib/apache2/
APT=apt-get

echo $ROOT

# Create htdocs
mkdir -p $APACHE_DIR/htdocs/dup_test
mkdir -p $APACHE_DIR/htdocs/cgi_bin
mkdir -p $APACHE_DIR/logs
touch $APACHE_DIR/mime.types

echo -n "belge" > $APACHE_DIR/htdocs/dup_test/enrich
echo -n "DUP" > $APACHE_DIR/htdocs/dup_test/dup
echo -n "COMPARE" > $APACHE_DIR/htdocs/dup_test/compare
echo -n "REWRITTEN" > $APACHE_DIR/htdocs/dup_test/rewritten
echo -n "HEADER_ONLY" > $APACHE_DIR/htdocs/dup_test/header_only
echo -n "HEADER_AND_BODY" > $APACHE_DIR/htdocs/dup_test/header_and_body

echo "#!/usr/bin/env python

import cgi
import cgitb
import sys

cgitb.enable()  # for troubleshooting

print \"Content-type: text/html\"
print \"\"

# read the body from stdin
body = sys.stdin.read()
print len(body)
" > $APACHE_DIR/htdocs/cgi_bin/get_body_size.cgi
chmod +x $APACHE_DIR/htdocs/cgi_bin/get_body_size.cgi


sed 's|{{ROOT}}|'"$ROOT"'|;s|{{APACHE_MODS}}|'"$APACHE_MODS"'|;s|{{BIN}}|'"$BIN"'|;s|{{CONF}}|'"$CONF"'|;' $PWD/httpd.conf.templ > $APACHE_DIR/httpd_func_tests.conf

