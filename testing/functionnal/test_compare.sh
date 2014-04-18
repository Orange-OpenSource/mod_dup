#!/bin/bash
RESULT_DIR=`cd $1; echo $PWD`
outjtl=$RESULT_DIR/results.jtl
echo ecco il file $outjtl
if [ -e $outjtl ]
then
   rm $outjtl
fi

jmeter -n -l $outjtl -t $RESULT_DIR/compare.jmx >/dev/null 2>&1
    grep -q ',false,' $outjtl
    if [ $? -eq 0 ]; then
        echo -n "`date` - compare.jmx - "
	echo "$(tput setaf 1)FAILURE$(tput sgr0)"
	grep ',false,' $outjtl
    else
        rm $outjtl
	echo -n "`date` - compare.jmx - "
        echo "$(tput setaf 2)SUCCESS$(tput sgr0)"
    fi

