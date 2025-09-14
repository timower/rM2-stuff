{
  lib,
  pkgs,
  config,
  rm2-stuff,
  ...
}:
let
  vm-ssh-config = pkgs.writeText "vm-ssh-config" ''
    Host RemEmu
      HostName localhost
      User root
      Port 2222
      StrictHostKeyChecking no
      UserKnownHostsFile /dev/null
  '';

  system-installer = rm2-stuff.nix-installer.override {
    extraPaths = [ config.system.build.toplevel ];
  };
  system-installer-tar = "${system-installer}/nix-installer.tar.xz";
in
{
  imports = [ ];

  options = {
  };

  config = {
    system.build.vm = rm2-stuff.rm-emu.override {
      nativeSetupCommands = ''
        in_vm mount /dev/mmcblk2p4 /home

        scp -F ${vm-ssh-config} ${system-installer-tar} RemEmu:/tmp

        # Install nix & system closure
        in_vm mkdir -p /tmp/unpack
        in_vm tar -C /tmp/unpack -xJf /tmp/nix-installer.tar.xz
        in_vm '/tmp/unpack/*/install'
        in_vm rm -rf '/tmp/unpack' /tmp/nix-installer.tar.xz

        # Run activation
        in_vm ${config.system.build.toplevel}/activate
      '';
    };
  };
}
