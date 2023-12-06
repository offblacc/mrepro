#!/bin/bash
dirs=$(find . -maxdepth 1 -type d -name "zadatak*" -o -name "lab*" -o -name "mi" -o -name "zi")
for dir in $dirs; do
	cd $dir && make clean && cd ..
done
