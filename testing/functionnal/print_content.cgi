#!/bin/sh

echo "Content-type: text/html"
echo ""

echo "Query String: $QUERY_STRING"
echo ""

echo "Header:"
env | grep -i http_


echo ""
echo "Data:"

data=`cat <&0`
echo $data

echo ""
env | grep -i content_length
echo "Character count: ${#data}"

exit 0
