#!/bin/bash
set -x
if [ ! -z "$1" ]
then
   numsecs=$1
else
   numsecs=10
fi
echo "Piping" $numsecs "seconds of data to DSP at 96Khz..."
arecord -d $numsecs -r 96000 -f S32_LE -D default:CARD=EVM -t raw | nano_test
