reMarkable Stuff
================

This repo contains all my rM hacks, it's currently still very much WIP.

SWTCON
------

This lib contains a reverse engineered software TCON. It currently still relies
on some functions from `xochitl`, namely the generator thread routine.
To use these functions it must be launched as an `LD_PRELOAD` library attached to xochitl.
The `swtcon-preload` tool is an example of how it can be currently used.

Tilem
-----

An TI calculator emulator for the remarkable, still WIP.
