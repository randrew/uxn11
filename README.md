# Uxn

An assembler and emulator for the [Uxn stack-machine](https://wiki.xxiivv.com/site/uxn.html), written in ANSI C. 

More docs coming soon.

If you wish to build the emulator without graphics mode:

```sh
cc src/devices/datetime.c src/devices/system.c src/devices/file.c src/uxn.c -DNDEBUG -Os -g0 -s src/uxncli.c -o bin/uxncli
```

## Contributing

Submit patches using [`git send-email`](https://git-send-email.io/) to the [~rabbits/public-inbox mailing list](https://lists.sr.ht/~rabbits/public-inbox).
