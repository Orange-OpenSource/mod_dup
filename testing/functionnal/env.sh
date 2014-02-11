#!/bin/sh

BIN=`cd $1; echo $PWD`
CONF=`cd $2; echo $PWD`

ROOT=`cd ../../build; echo $PWD`
APACHE_DIR=$ROOT/apache2
APACHE_SRC=$APACHE_DIR/src

PHP_DIR=$ROOT/env/php5
PHP_SRC=$PHP_DIR/src

APT=apt-get

echo $ROOT

## Apache installation
if test ! -f ${APACHE_DIR}/bin/httpd; then
	printf "Preparing to compile apache2...\n";

	mkdir -p $APACHE_SRC
	cd $APACHE_SRC
	$APT source apache2
	cd apache2*

	# configure
	./configure --prefix=$APACHE_DIR --enable-mods-shared=all --enable-proxy --enable-fcgid --enable-proxy-http --enable-ssl --enable-rewrite --enable-deflate --with-mpm=worker  CFLAGS="-O0 -g -ggdb3 -fno-inline"
	if [ $? -ne 0 ]
	then
	  echo -e "There was an error configuring apache2, check logs\n";
	  exit 1;
	else
	  echo -e "Configuration DONE\n\n\n\n";
	fi

	# compile
	make -j 4 || ( echo -e "There was an error in apache2's compilation. Check logs\n" && exit 1 )
	echo -e "Apache2 compiled successfully\n";

	# install
	make install || ( echo -e "There was an error installing apache2. Check lgos\n" && exit 1 )
	echo -e "Apache2 installed\n";

        touch $APACHE_DIR/mime.types

fi

# Create htdocs
mkdir -p $APACHE_DIR/htdocs/dup_test
mkdir -p $APACHE_DIR/htdocs/cgi_bin
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


sed 's|{{ROOT}}|'"$ROOT"'|;s|{{BIN}}|'"$BIN"'|;s|{{CONF}}|'"$CONF"'|;' $PWD/httpd.conf.templ > $APACHE_DIR/conf/custom_httpd.conf


# restart apache
echo "Stopping apache"
$APACHE_DIR/bin/httpd -f $APACHE_DIR/conf/custom_httpd.conf -k stop
#$APACHE_DIR/bin/apachectl stop || exit 1;

out=`ps auxw | grep $APACHE_DIR/bin/httpd | grep -v grep`
while [ $? -eq 0 ]; do
    echo "Waiting 1s for apache to stop"
    sleep 1
    out=`ps auxw | grep $APACHE_DIR/bin/httpd | grep -v grep`
done

if test $# -eq 3; then
    echo "Starting apache with -X option"
    $APACHE_DIR/bin/httpd -X -f $APACHE_DIR/conf/custom_httpd.conf -k start
else
    echo "Starting apache"
    $APACHE_DIR/bin/httpd -f $APACHE_DIR/conf/custom_httpd.conf -k start &
fi