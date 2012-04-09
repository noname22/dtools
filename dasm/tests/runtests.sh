#!/bin/bash
set -e
cd allins && ./allins.sh && cd -
cd include && ./include.sh && cd -
cd labels && ./labels.sh && cd -
cd maximinus-thrax-testsuite && ./maximinus-thrax-testsuite.sh && cd -

echo ""
echo "all tests passed"
