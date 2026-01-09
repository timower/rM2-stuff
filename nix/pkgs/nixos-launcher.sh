#!/bin/sh

set -eu

nixosRoot=$(dirname "$(realpath "$0")")

usage() {
  echo "Nixos launcher, usage:"
  echo " $0 <action>"
  echo "Where action is:"
  echo "  launch     Setup NixOS without installing."
  echo "  install    Install NixOS to start when restarting from xochitl."
  echo "  uninstall  Remove any leftover files from the root partition."
  exit 1
}

uninstall() {
  systemctl disable --now nixos-nextroot.service || true
  systemctl disable --now nixos-server.service || true

  rm -f /usr/bin/nixctl \
    /usr/libexec/nixos-server.sh \
    /etc/systemd/system/nixos-nextroot.service \
    /etc/systemd/system/nixos-server.service

  umount /run/nextroot || true
  rmdir /run/nextroot

  exit 0
}

start_server() {
  # Try to copy wifi networks config, but home might already be unmounted.
  cp /home/root/.config/remarkable/wifi_networks.conf \
    /run/nextroot/etc/wpa_supplicant.conf || true

  # Copy firmware, required for resume after suspend.
  mkdir -p "/run/nextroot/lib/firmware"
  cp -ar /lib/firmware/* "/run/nextroot/lib/firmware"

  # Save active partition
  swupdate -g >/run/active-partition

  # Bind mount /nix so binaries from the nix store work.
  mkdir -p /nix
  mount -o bind,ro /run/nextroot/nix /nix

  rm -f /run/rm2fb.sock /dev/shm/swtfb.01
  LD_LIBRARY_PATH=/usr/lib @rm2fb-server@ &
  while ! test -e /run/rm2fb.sock -a -e /dev/shm/swtfb.01; do
    sleep 1
  done

  LD_PRELOAD="@rm2fb-client@" "@yaft_reader@" /dev/kmsg &

  # The server has started, let systemd know
  systemd-notify --ready

  wait
}

install_services() {

  cat >"$1/nixos-nextroot.service" <<EOF
[Unit]
Description=Bind mount /run/nextroot for NixOS
After=home.mount run.mount

[Service]
Type=oneshot
ExecStart=$nixosRoot/nixctl boot

[Install]
WantedBy=multi-user.target
EOF

  cat >"$1/nixos-server.service" <<EOF
[Unit]
Description=NixOS rm2fb server
SurviveFinalKillSignal=yes
IgnoreOnIsolate=yes
DefaultDependencies=no
After=basic.target rm2fb.service xochitl.service
Conflicts=reboot.target kexec.target poweroff.target halt.target rescue.target emergency.target rm2fb.service rm2fb.socket launcher.service xochitl.service
Before=shutdown.target rescue.target emergency.target soft-reboot.target

[Service]
Type=notify
ExecStart=/run/nextroot/nixctl start-server

[Install]
WantedBy=soft-reboot.target
EOF

}

boot() {
  mkdir -p /run/nextroot
  mount -o bind "$nixosRoot" /run/nextroot

  # Update the services on boot
  if [ -f /etc/systemd/system/nixos-nextroot.service ]; then
    install_services /etc/systemd/system
    systemctl daemon-reload
  fi

  # Copy wifi networks config
  cp /home/root/.config/remarkable/wifi_networks.conf \
    /run/nextroot/etc/wpa_supplicant.conf || true

  exit 0
}

if [ "$#" != "1" ]; then
  usage
fi

systemdTarget=""

case "$1" in
launch)
  systemdTarget="/run/systemd/system"
  ;;
install)
  systemdTarget="/etc/systemd/system"
  ;;
uninstall) uninstall ;;

# Internal commands
boot) boot ;;
start-server) start_server ;;

*) usage ;;
esac

install_services "$systemdTarget"

case "$1" in
launch)
  systemctl start nixos-nextroot.service
  systemctl start nixos-server.service
  ;;
install)
  ln -s $nixosRoot/nixctl /usr/bin/
  systemctl enable nixos-server.service
  systemctl enable --now nixos-nextroot.service
  ;;
esac

echo "Run 'systemctl soft-reboot' to start nixos"
