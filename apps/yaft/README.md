Yaft
====

Fast framebuffer terminal emulator, using [libghostty](https://ghostty.org/docs/about#libghostty) for VT parsing.

Usage
-----

To use simply execute `yaft` or `yaft <command..>`.

There's an on-screen keyboard that can be partially hidden by long pressing the escape key.
When the type folio is attached the screen will go into landscape mode and the keyboard will be hidden.
Mouse events are supported using the touchscreen.

Config
------

A config file is created in `.config/yaft/config.toml` on startup.
For syntax see [config.cpp](./config.cpp). The default config is:
```toml
# Layout of the virtual keyboard.
layout = "qwerty"

# Keymap for any physical keyboards. Defaults to the us rM pogo keyboard.
#  * qwerty or rm-qwerty
keymap = "rm-qwerty"

# Auto rotate if keyboard is connected.
auto-rotate = true

# Orientation if no keyboard is connected:
#  * none, clockwise, inverted, counterclockwise
rotation = "none"

# Do a full refresh after 1024 updates.
# Set to 0 to disable auto refresh.
auto-refresh = 1024

# Repeat delay for keyboards.
repeat-delay = 600
# Repeat rate in chars per second.
repeat-rate = 25

```

Credits
-------

Yaft was originally forked from https://uobikiemukot.github.io/yaft/.
Original author: Copyright (c) 2012 haru <uobikiemukot at gmail dot com>
