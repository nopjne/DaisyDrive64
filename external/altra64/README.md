# Altra64

Alternative Everdrive64 menu

`Altra64` is an open source menu for [Everdrive64](http://krikzz.com/) and ed64+ and is based on a fork of alt64 which was
originally written by saturnu, and released on the
[Everdrive64 forum](http://krikzz.com/forum/index.php?topic=816.0).

## Building

If you want to build the menu, you need an n64 toolchain. This is terrible to build, moparisthebest ended up creating a Dockerfile in the docker folder, instructions included in it.

Or if you trust him, you can use the one he built and pushed to docker hub, [moparisthebest/altra64-dev](https://hub.docker.com/r/moparisthebest/altra64-dev)

### Build `Altra64`

To build the Rom

from the projects root directory, with docker installed

$ docker run --rm -v "$(pwd):/build" moparisthebest/altra64-dev make

If it all worked, you will find `OS64.v64` in the `bin` directory.


### Clean `Altra64`

Finally, we can clean the build objects from the project

from the projects root directory

$ docker run --rm -v "$(pwd):/build" moparisthebest/altra64-dev make clean

Enjoy!
