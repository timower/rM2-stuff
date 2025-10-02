#!/bin/sh

set -eu

nixosRoot="$(dirname "$0")"

# Copy wifi networks config
cp ~/.config/remarkable/wifi_networks.conf "$nixosRoot/etc/wpa_supplicant.conf"

# Copy firmware, required for resume after suspend.
mkdir -p "$nixosRoot/lib/firmware"
cp -ar /lib/firmware/* "$nixosRoot/lib/firmware"

# Bind mount /nix so binaries from the nix store work.
mkdir -p /nix
mount -o bind,ro $nixosRoot/nix /nix

# Create launch service script.
cat >/run/nixos-rm2fb.sh <<EOF
#!/bin/sh
rm -f /run/rm2fb.sock /dev/shm/swtfb.01
@rm2fb-server@ &
while ! test -e /run/rm2fb.sock -a -e /dev/shm/swtfb.01 ; do
  sleep 1
done
LD_PRELOAD=@rm2fb-client@ @yaft_reader@ /dev/kmsg
EOF
chmod +x /run/nixos-rm2fb.sh

# Create and start launch service.
cat >/run/systemd/system/nixos-rm2fb.service <<EOF
[Unit]
Description=Nixos rm2fb server
SurviveFinalKillSignal=yes
IgnoreOnIsolate=yes
DefaultDependencies=no
After=basic.target rm2fb.service
Conflicts=reboot.target kexec.target poweroff.target halt.target rescue.target emergency.target rm2fb.service rm2fb.socket launcher.service
Before=shutdown.target rescue.target emergency.target

[Service]
Type=simple
ExecStart=/run/nixos-rm2fb.sh
EOF
systemctl start nixos-rm2fb

# Bind mount nextroot, so reboot will do soft-reboot into nixos.
mkdir -p /run/nextroot
mount -o bind $nixosRoot /run/nextroot

echo "Run 'systemctl soft-reboot' to start nixos"
