#!/bin/bash

set -e

int_min=-2147483648
int_max=2147483647
int_size=4
gen_count=10000000
gen_bytes=$(echo "$gen_count * $int_size" | bc)
file=test_filesort.txt
file_orig=test_orig.txt
file_sort=test_sort.txt
cpu_threads=$(grep -c processor /proc/cpuinfo)
buf_size=1

echo "---> Generating test file..."
time od -A n -N ${gen_bytes} -t d${int_size} < /dev/urandom |
	awk '{$1=$1;print}' | tr -s ' ' '\n' > $file
cp $file $file_orig

echo
echo "---> Sorting using sort..."
time LC_ALL=C sort -n --parallel=$cpu_threads -S ${buf_size}M < $file > \
	$file_sort

echo
echo "---> Sorting using filesort..."
time LC_ALL=C ../filesort -b $buf_size -t $cpu_threads $file

echo
set +e
cmp --silent $file_sort $file
res=$?
if [ $res -ne 0 ]; then
	echo "Test failed!"
	exit 1
else
	echo "Test is passed!"
fi
