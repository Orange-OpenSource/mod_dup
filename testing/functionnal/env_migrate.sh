#!/bin/sh

echo "IN ENV.SH"

BIN=`cd $1; echo $PWD`
CONF=`cd $2; echo $PWD`
ROOT=`cd $1/..; echo $PWD`
APACHE_DIR=$ROOT/apache2
APACHE_SRC=$APACHE_DIR/src
APACHE_MODS=/usr/lib/apache2/
APT=apt-get
DATA=`cd data; echo $PWD`

echo $ROOT

# Create htdocs
#mkdir -p $APACHE_DIR/htdocs/dup_test
mkdir -p $APACHE_DIR/htdocs/cgi_bin
mkdir -p $APACHE_DIR/logs
touch $APACHE_DIR/mime.types

#echo -n "belge" > $APACHE_DIR/htdocs/dup_test/enrich
#echo -n "DUP" > $APACHE_DIR/htdocs/dup_test/dup
#echo -n "COMPARE" > $APACHE_DIR/htdocs/dup_test/compare
#echo -n "REWRITTEN" > $APACHE_DIR/htdocs/dup_test/rewritten
#echo -n "HEADER_ONLY" > $APACHE_DIR/htdocs/dup_test/header_only
#echo -n "HEADER_AND_BODY" > $APACHE_DIR/htdocs/dup_test/header_and_body
#echo -n "NP1" > $APACHE_DIR/htdocs/service_np1
#cat $DATA/largeresponse > $APACHE_DIR/htdocs/dup_test/largeresponse
#echo -n "MULTI" > $APACHE_DIR/htdocs/multi

echo "BEGINNING OK"

echo "#!/bin/sh

echo \"Content-type: text/html\"
echo \"\"

echo \"Query String: $QUERY_STRING\"
echo \"\"

echo \"Header:\"
env | grep -i http_


echo \"\"
echo \"Data:\"

data=\`cat <&0\`
echo $data

echo \"\"
env | grep -i content_length
echo \"Character count: ${#data}\"

exit 0" > $APACHE_DIR/htdocs/cgi_bin/print_content.cgi
chmod +x $APACHE_DIR/htdocs/cgi_bin/print_content.cgi

echo "WRITING OK"

sed 's|{{ROOT}}|'"$ROOT"'|;s|{{APACHE_MODS}}|'"$APACHE_MODS"'|;s|{{BIN}}|'"$BIN"'|;s|{{CONF}}|'"$CONF"'|;' $PWD/httpd.migrate_conf.templ > $APACHE_DIR/httpd_func_tests.conf

