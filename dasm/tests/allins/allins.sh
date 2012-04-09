#!/bin/bash
set -e
echo " == All instructions and operand types =="
./genall.py > /tmp/allins.dasm

echo "assembling"
../../dasm /tmp/allins.dasm /tmp/out.dbin

echo "disassembling"
../../../ddisasm/ddisasm /tmp/out.dbin > /tmp/allins_d.dasm

echo "reassembling"
../../dasm /tmp/allins_d.dasm /tmp/out_d.dbin

echo "disassembling"
../../../ddisasm/ddisasm /tmp/out_d.dbin > /tmp/allins_dd.dasm

echo "checking"
diff /tmp/allins_dd.dasm /tmp/allins_d.dasm

echo "done"
