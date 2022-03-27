#!/bin/sh -e

clang-format -i uxn11.c

echo "Cleaning.."
rm -f ./bin/*

echo "Building.."
mkdir -p bin
gcc uxn.c devices/ppu.c uxn11.c -o bin/uxn11 -lX11

echo "Running.."
bin/uxn11 etc/screen.rom
