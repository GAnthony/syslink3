#!/bin/bash
# Run MessageQMulti in parallel processes 
#
if [ $# -ne 2 ]
then
    echo "Usage: multi_process <num_processes> <iterations_per_process>"
    exit
fi

for i in `seq 0 $(( $1 - 1))`
do
	echo "MessageQMulti Test #" $i
	# This calls MessageQMulti with One Thread, a process #, and 
	# number of iterations per thread.
	MessageQMulti 1 $2 $i &
done
