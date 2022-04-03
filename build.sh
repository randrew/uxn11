#!/bin/sh -e

clang-format -i src/uxn11.c

echo "Cleaning.."
rm -f ./bin/*

echo "Building.."
mkdir -p bin

if [ "${1}" = '--install' ]; 
then
	echo "Installing.."
	gcc src/uxn.c src/devices/system.c src/devices/screen.c src/devices/controller.c src/devices/mouse.c src/devices/file.c src/devices/datetime.c src/uxn11.c -D_POSIX_C_SOURCE=199309L -DNDEBUG -Os -g0 -s -o bin/uxn11 -lX11
	gcc src/uxn.c src/devices/system.c src/devices/file.c src/devices/datetime.c src/uxncli.c -D_POSIX_C_SOURCE=199309L -DNDEBUG -Os -g0 -s -o bin/uxncli
	cp bin/uxn11 ~/bin
else
	gcc -std=c89 -D_POSIX_C_SOURCE=199309L -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined src/uxn.c src/devices/system.c src/devices/screen.c src/devices/controller.c src/devices/mouse.c src/devices/file.c src/devices/datetime.c src/uxn11.c -o bin/uxn11 -lX11
	gcc -std=c89 -D_POSIX_C_SOURCE=199309L -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined src/uxn.c src/devices/system.c src/devices/file.c src/devices/datetime.c src/uxncli.c -o bin/uxncli
fi

echo "Done."

# echo "Running.."
# uxnasm etc/repl.tal bin/repl.rom && bin/uxn11 bin/repl.rom
