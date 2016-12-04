#!/bin/sh
set -x
# WARNING: this script doesn't check for errors, so you have to enhance it in case any of the commands
# below fail.

rmmod sys_mergesort
make clean
make
insmod sys_mergesort.ko

