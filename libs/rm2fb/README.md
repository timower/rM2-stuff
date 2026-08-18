reMarkable 2 - Framebuffer
==========================

A display manager for the reMarkable 2, extending [ddvk's rm2fb](https://github.com/ddvk/remarkable2-framebuffer).

`rm2fb_server` runs as a systemd service (`rm2fb.service`), preloaded into
`xochitl` via `LD_PRELOAD`. It hooks the framebuffer and evdev input calls so
that exactly one client controls the display and input at a time, and
multiplexes updates from all connected clients through a UNIX socket
(`rm2fb.socket`) instead of `xochitl`'s message queues.

Use Rocket to switch between apps, or `rm2fbctl` on the commandline:
```
rm2fbctl list
rm2fbctl switch <pid>
```
Other launchers relying on the original rm2fb protocol are not supported.

### swtcon mode

Instead of hooking `xochitl`'s internal update functions by address (which
breaks on every xochitl update), the `_swtcon` variants of the server and
client use rm2fb's own reverse engineered software TCON (see
[SWTCON](../swtcon)) to drive the display, so no address-hooking into a
specific xochitl version is needed. Only built for the 32-bit ARM target,
since swtcon itself only builds there.

To run it:
 * Start `rm2fb_server_swtcon` instead of `rm2fb_server`, e.g. as the
   `rm2fb.service` `ExecStart`.
 * `LD_PRELOAD` `librm2fb_client_swtcon.so` (built from the
   `rm2fb_client_swtcon` target) into `xochitl` instead of
   `librm2fb_client.so`. This leaves xochitl's own statically-linked swtcon
   untouched to drive the panel directly; the client only redirects its
   internal buffers so the server can see their content while paused.
