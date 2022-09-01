#!/bin/bash

set -x
set -o trace

kernel=$1
program=$2

flash=0x60000000


./build/qemu-system-arm -M teensy-41 -kernel "$kernel" -device loader,file="$program",addr="$flash",force-raw=on -nographic -s -S
