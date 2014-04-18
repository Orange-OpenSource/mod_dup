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

echo -n "COMPARE" > $APACHE_DIR/htdocs/dup_test/compare
cat data/largeresponse > $APACHE_DIR/htdocs/dup_test/largeresponse
echo -n "BODY" > $APACHE_DIR/htdocs/dup_test/comp_test1
echo -n "BODY2" > $APACHE_DIR/htdocs/dup_test/comp_test2
echo -n "The log has been truncated" > $APACHE_DIR/htdocs/dup_test/comp_truncate
echo -n "NP1" > $APACHE_DIR/htdocs/service_np1

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

sed 's|{{APACHE_DIR}}|'"$APACHE_DIR"'|;' $CONF/compare.conf.tpl > $CONF/compare.conf
sed 's|{{ROOT}}|'"$ROOT"'|;s|{{APACHE_MODS}}|'"$APACHE_MODS"'|;s|{{BIN}}|'"$BIN"'|;s|{{CONF}}|'"$CONF"'|;' $PWD/httpd.compare_conf.templ > $APACHE_DIR/httpd_func_tests.conf

