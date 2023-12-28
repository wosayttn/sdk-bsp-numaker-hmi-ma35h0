#!/bin/bash

set -x -e

python3 nuwriter.py -a ddrimg/MA35H04F767C.bin

python3 nuwriter.py -o execute -w ddr 0x80800000 ../rtthread.bin

