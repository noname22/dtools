#!/bin/bash

set -e

for t in "dw"; do
../../dasm -v1 $t.dasm /tmp/out.dbin
diff ${t}_correct.dbin /tmp/out.dbin
done

echo "done"
