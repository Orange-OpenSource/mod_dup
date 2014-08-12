#!/bin/bash

rm -f $1/outjmeter
jmeter -n -l $1/outjmeter.tmp -t $1/TestMigrate.jmx > /dev/null 2>&1

linecount=`cat $1/outjmeter.tmp | grep ',false' | wc -l`

# restore backup of migrate.conf
#if [ -e /etc/apache2/mods-available/migrate.conf.bak ]
#then
#	mv -fv /etc/apache2/mods-available/migrate.conf.bak /etc/apache2/mods-available/migrate.conf
#else
#	echo "" > /etc/apache2/mods-available/migrate.conf
#fi

# check that the 6 tests in JMeter succeeded
if [ $linecount -eq 0 ]
then
	# rm will fail if for any reason jmeter did not actually output to the log file
	rm $1/outjmeter.tmp
	echo -e "\n\nOK : JMeter test passed\n"
	exit 0
else
	echo -e "\n\nKO : At least of the JMeter test did not succeed:\n"
	cat $1/outjmeter.tmp | grep ',false'
    echo -e "\n"
	rm $1/outjmeter.tmp
	exit 1
fi

