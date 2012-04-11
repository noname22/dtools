#!/bin/bash

echo "assembling"
../../../dasm/dasm reg_ref_overflow.dasm /tmp/out.dbin

echo "executing"

../../dinterpret /tmp/out.dbin
ret=$?

if [ "$ret" != "123" ]; then
	echo "reg_ref_overflow returned $ret instead of 123"
	exit 1
fi

echo "ok"
