#!/bin/bash

rm -f outjmeter
jmeter -n -l outjmeter.tmp -t $1

linecount=`cat outjmeter.tmp | grep ',false' | wc -l`

rm -f outjmeter.tmp

# restore backup of migrate.conf
if [ -e /etc/apache2/mods-available/migrate.conf.bak ]
then
	mv -fv /etc/apache2/mods-available/migrate.conf.bak /etc/apache2/mods-available/migrate.conf
else
	echo "" > /etc/apache2/mods-available/migrate.conf
fi

# check that the 6 tests in JMeter succeeded
if [ $linecount -eq 0 ]
then
	echo "\n\nOK : JMeter test passed\n"
	exit 0
else
	echo "\n\nKO : At least of the JMeter test did not succeed\n"
	exit 1
fi

