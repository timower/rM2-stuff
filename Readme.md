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

An TI-84+ calculator emulator for the remarkable.

![Screenshot tilem](doc/tilem.png)

To use simply execute `tilem` in a folder containing a `ti84p.rom` ROM file. On the reMarkable 2 the rm2-fb client shim will need to be available.

[Yaft](apps/yaft)
----

A fast framebuffer terminal emulator.

![Screenshot yaft](doc/yaft.png)

To use simply execute `yaft` or `yaft <command..>`. Again on the reMarkable 2 the rm2-fb needs to be loaded.

Launcher
----
(TODO: pick a name)

A tiny launcher written for my personal needs. Still very much WIP.


![Screenshot launcher](doc/launcher.png)

Currently three gestures are defined:
 * Three finger swipe down: show launcher
 * Three finger swip left/right: switch to next/previous app.

Pressing `[x]` will close an app, `>` indicates the currently running app, and `*` indicates an app paused in the background.
