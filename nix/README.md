reMarkable NixOS
================

Booting
-------

The system is launched using a systemd soft-reboot. The `launch` script will
mount the `nixos` directory on `/run/nextroot`. This will cause systemd to use
it as a new root when rebooting. The NixOS stage2 init script at `sbin/init`
will be started, booting the full NixOS system.

Advantages:
 * No changes on reMarkable root partition.
 * Full isolation between NixOS and main system.
 * A simple reboot will restart into xochitl.

Disadvantages:
 * NixOS doesn't manage the linux kernel.

Installing
----------

To install nixos, build the `config.system.build.image`:
```bash
nix build .#nixosConfigurations.example.config.system.build.image
```
This will put a tarball in `result/tarball/rm-nixos.tar.xz`, which can be copied
over and extracted on the remarkable 2:
```bash
scp result/tarball/rm-nixos.tar.xz root@<remarkable>:
ssh root@<remarkable>
> mkdir -p nixos
> tar -C nixos -xJf rm-nixos.tar.xz
```
Don't forget to create a separate directory before extracting, otherwise you'll
pollute the home directory.

Now the system can be launched by starting the launch script, make sure to stop
xochitl / rm2fb first:
```bash
> systemctl stop xochitl rm2fb # Or any other service that uses the screen.
> ./nixos/launch
```
Now a new rm2fb server will be running, with an instance of `yaft_reader` that
shows the kernel log, which is useful for debugging.
To start NixOS, reboot into it. Each of these commands will do the same:
```bash
> systemctl soft-reboot
> systemctl reboot
> reboot
```

To exit NixOS, simply reboot again.

Virtual Machine
---------------

A QEMU based vm is available at `config.system.build.vm`:
```bash
nix build .#nixosConfigurations.example.config.system.build.vm
./result/bin/run_vm
# Wait for boot, login as root
> nixos/launch
# Connect using rm-emu in another terminal.
> systemctl soft-reboot
```

TODO
----

 - [x] Test wpa_supplicant conf copy.
 - [x] Test `nixos-rebuild` from host.
 - [x] Split `./configuration.nix` into separate modules.
 - [x] Create xochitl chroot.
 - [ ] Make vm that starts rm2-emu automatically.
 - [ ] Re-create wrapWithPreload, rm2-stuff program modules.
 - [ ] Write rocket launcher systemd unit.
