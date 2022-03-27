# Uxn

An emulator for the [Uxn stack-machine](https://wiki.xxiivv.com/site/uxn.html), written in ANSI C. 

More docs coming soon.

## Graphical

All you need is X11.

```
gcc src/uxn.c src/devices/system.c src/devices/screen.c src/devices/controller.c src/devices/mouse.c src/devices/datetime.c src/uxn11.c -DNDEBUG -Os -g0 -s -o bin/uxn11 -lX11
```

## Terminal

If you wish to build the emulator without graphics mode:

```sh
cc src/devices/datetime.c src/devices/system.c src/devices/file.c src/uxn.c -DNDEBUG -Os -g0 -s src/uxncli.c -o bin/uxncli
```

## Contributing

Submit patches using [`git send-email`](https://git-send-email.io/) to the [~rabbits/public-inbox mailing list](https://lists.sr.ht/~rabbits/public-inbox).
