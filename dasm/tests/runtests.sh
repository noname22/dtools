#!/bin/bash

for t in "allins" "include" "labels" "maximinus-thrax-testsuite" "directives"
do
	cd $t && ./$t.sh && cd -
	if [ $? != 0 ]; then
		echo ""
		echo "test: '$t' failed"
		exit 1
	fi
done

echo ""
echo "all tests passed"
