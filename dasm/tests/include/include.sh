#!/bin/bash
echo " == Include test == "
set -e
../../dasm main.dasm /tmp/out.dbin
../../../ddisasm/ddisasm /tmp/out.dbin 
diff /tmp/out.dbin correct_output.dbin
echo "ok"
