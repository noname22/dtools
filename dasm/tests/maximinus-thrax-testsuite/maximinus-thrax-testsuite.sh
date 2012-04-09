#!/bin/bash
set -e

for i in {1..4}; do
echo "test $i"
../../dasm -v1 $i.dasm /tmp/$i.dbin
diff /tmp/$i.dbin ${i}_correct.dbin
done

echo "ok"
