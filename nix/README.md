reMarkable NixOS
================

Booting
-------

The system is launched using a systemd soft-reboot. The `nixctl` script will
mount the `nixos` directory on `/run/nextroot`. This will cause systemd to use
it as a new root when rebooting. The NixOS stage2 init script at `sbin/init`
will be started, booting the full NixOS system.

Advantages:
 * No changes on reMarkable root partition.
 * Full isolation between NixOS and main system.
 * A simple reboot will restart into xochitl.

Disadvantages:
 * NixOS doesn't manage the linux kernel.

Usage
-----

To use nixos, build the `config.system.build.image`:
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
# Should technically happen automatically as nixos Conflicts with this,
# but doesn't hurt:
> systemctl stop xochitl rm2fb
> ./nixos/nixctl launch
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

Wifi
----

`nixctl` copies your saved wifi networks over from xochitl's config on every
boot, so it works out of the box on NixOS too. It copies both the
`wpa_supplicant`-style `wifi_networks.conf` and any NetworkManager
`*.nmconnection` profiles it finds; only the one actually in use is picked
up, based on which config.nix option you enable, matching your xochitl
version:

 * `networking.wireless.enable = true;` for xochitl < 3.28
   (`wpa_supplicant`).
 * `networking.networkmanager.enable = true;` for xochitl >= 3.28 beta
   (NetworkManager). Add your login user to the `networkmanager` group,
   otherwise it can't talk to NetworkManager over D-Bus.

Installing
----------

The `nixctl install` command will install NixOS so that it automatically starts
when the system is rebooted from xochitl. The `nixos uninstall` command should
remove all traces from the system partition.

Virtual Machine
---------------

A QEMU based vm is available at `config.system.build.vm`:
```bash
nix build .#nixosConfigurations.example.config.system.build.vm
./result
# Wait for boot, login as root
```
This VM boots into NixOS directly.
To boot into xochitl first use `config.system.build.vm-xochitl`.


TODO
----

 - [x] Test wpa_supplicant conf copy.
 - [x] Test `nixos-rebuild` from host.
 - [x] Split `./configuration.nix` into separate modules.
 - [x] Create xochitl chroot.
 - [x] Re-create wrapWithPreload, rm2-stuff program modules.
 - [x] Write rocket launcher systemd unit.
 - [x] Rework launch, allow install.
 - [x] reMarkable document sync.
 - [x] Fix koreader spamming 'WARN  Polling for input events returned an error:'
 - [x] Make vm that starts rm2-emu automatically.
 - [x] NixOS tests.
 - [x] Make xochitl run as a user?
 - [x] Minimize size
  - [x] Drop nixos tools, don't use `-ng` to lose python dep.
 - [ ] Correctly start user session for Rocket.
 - [ ] Usb ethernet.
