#!/bin/bash
set -e
echo " == Label test =="
../../dasm labeltest.dasm /tmp/out.dbin
../../../ddisasm/ddisasm /tmp/out.dbin
diff /tmp/out.dbin correct_out.dbin
echo "ok"
