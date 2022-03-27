#!/bin/sh -e

clang-format -i src/uxn11.c

echo "Cleaning.."
rm -f ./bin/*

echo "Building.."
mkdir -p bin
gcc -std=c89 -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined src/uxn.c src/devices/system.c src/devices/screen.c src/uxn11.c -o bin/uxn11 -lX11

echo "Running.."
bin/uxn11 etc/screen.rom
