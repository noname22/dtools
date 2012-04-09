#!/bin/bash
set -e


for t in "allins" "include" "labels" "maximinus-thrax-testsuite" "directives"
do
	cd $t && ./$t.sh && cd -
done

echo ""
echo "all tests passed"
