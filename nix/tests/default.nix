{
  lib,
  pkgs,
  rm2-stuff,
}:
let
  mkTest =
    {
      modules,
      testScript,
      vmFast ? true,
      golden ? false,
    }@args:
    let
      system = lib.nixosSystem {
        modules = modules ++ [
          {
            services.openssh.enable = true;
            users.users.test = {
              isNormalUser = true;
              openssh.authorizedKeys.keyFiles = [ ./id_ed25519.pub ];
            };
          }
        ];
      };

      vm-fast = system.config.system.build.vm-fast.override {
        setupCommands = ''
          while ! ssh -o StrictHostKeyChecking=no -i ${./id_ed25519} -p 2222 test@localhost true; do
            sleep 1
          done
        '';
      };

      vm-xochitl = system.config.system.build.vm-xochitl.override {
        setupCommands = ''
          while ! ssh -o StrictHostKeyChecking=no -p 2222 root@localhost true; do
            sleep 1
          done
        '';
      };

      vm = if vmFast then vm-fast else vm-xochitl;

      driver = pkgs.callPackage ./driver.nix {
        inherit vm testScript golden;
        inherit (rm2-stuff) tools;
      };
    in
    pkgs.runCommand "rm-nix-test"
      {
        passthru = lib.optionalAttrs (!golden) {
          inherit vm driver;
          golden = mkTest (args // { golden = true; });
        };
      }
      ''
        ${driver}
      '';
in
{
  tilem = mkTest {
    modules = [
      ../modules/remarkable.nix
      ../example.nix
    ];
    testScript = ''
      wait_for "startup.png"
      tap_at 644 1064
      wait_for "tilem.png"

      # tap_at 804 952
    '';
  };

  yaft = mkTest {
    modules = [
      ../modules/remarkable.nix
      ../example.nix
    ];
    testScript = ''

      wait_for "startup.png"
      tap_at 936 1052
      wait_for "yaft.png"

      # tap_at 56 1836
      # sleep 1
      # tap_at 308 1704
      # sleep 1
    '';
  };

  koreader = mkTest {
    modules = [
      ../modules/remarkable.nix
      ../example.nix
    ];
    testScript = ''
      wait_for "startup.png"
      tap_at 476 1038
      wait_for "koreader.png" 30

      # tap_at 920 122
      # sleep 3
      # tap_at 1340 76
      # sleep 2
      # tap_at 266 1108
      # sleep 2
      # tap_at 172 652
    '';
  };

  xochitl = mkTest {
    modules = [
      ../modules/remarkable.nix
      ../example.nix
    ];
    testScript = ''
      wait_for "startup.png"
      tap_at 782 1046
      wait_for "xochitl_3.20.png" 120
    '';
  };

  installer = mkTest {
    modules = [
      ../modules/remarkable.nix
      ../example.nix
    ];
    vmFast = false;
    testScript = ''
      in_vm nixos/nixctl launch
      sleep 3
      in_vm 'echo test > /dev/kmsg'
      wait_for "launch_test.png"
      in_vm reboot

      while ! in_nixos true; do
        sleep 5
      done
      wait_for "startup.png"
    '';
  };
}
