#!/bin/bash

set -x -e

python3 nuwriter.py -c header-sd.json

python3 nuwriter.py -p pack-sd.json

python3 nuwriter.py -a ddrimg/MA35H04F767C.bin

python3 nuwriter.py -w sd pack/pack.bin
