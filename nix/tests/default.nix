{
  lib,
  pkgs,
  rm2-stuff,
  buildPlatform ? "x86_64-linux",
}:
let
  mkTest = lib.makeOverridable (
    {
      modules,
      testScript,
      bootNixos ? true,
      golden ? false,
    }@args:
    let
      system = lib.nixosSystem {
        modules = modules ++ [
          {
            services.openssh.enable = true;

            systemd.services."rocket".environment = {
              ROCKET_WAIT_FOR_INPUT = "1";
            };

            users.users.test = {
              extraGroups = [ "systemd-journal" ];
              isNormalUser = true;
              openssh.authorizedKeys.keyFiles = [ ./id_ed25519.pub ];
            };
          }
          {
            virtualisation.host.pkgs = pkgs;
            nixpkgs.buildPlatform = buildPlatform;
          }
        ];
      };

      rm2fb-test = lib.getExe' rm2-stuff.tools "rm2fb-test";
      vm-nixos = system.config.system.build.vm-nixos.override {
        setupCommands = ''
          while ! ssh -o StrictHostKeyChecking=no -i ${./id_ed25519} \
                      -p 2222 test@localhost systemctl is-active rocket; do
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

      vm = if bootNixos then vm-nixos else vm-xochitl;

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
      ''
  );

  setNames = lib.mapAttrs (
    n: v:
    v.overrideAttrs (o: {
      name = n;
    })
  );
in
setNames rec {
  tilem = mkTest {
    modules = [
      ../modules/remarkable.nix
      ../template/config.nix
    ];
    testScript = ''
      wait_for "startup.png"
      tap_at 644 1064
      wait_for "tilem.png"

      # tap_at 804 952
    '';
  };

  tilem-full = mkTest {
    modules = [
      ../modules/remarkable.nix
      ../template/config.nix
      { programs.tilem.fullscreen = true; }
    ];
    testScript = ''
      wait_for "startup.png"
      tap_at 644 1064
      wait_for "tilem-full.png"

      # tap_at 804 952
    '';
  };

  yaft-nouser = mkTest {
    modules = [
      ../modules/remarkable.nix
      ../template/config.nix
      { programs.rocket.loginUser = lib.mkForce null; }
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

  yaft-user = mkTest {
    modules = [
      ../modules/remarkable.nix
      ../template/config.nix
    ];
    testScript = ''
      wait_for "startup.png"
      tap_at 936 1052
      wait_for "yaft-user.png"
    '';
  };

  # Same scenario as `yaft-user`, but with services.rm2fb.variant = "swtcon"
  # - yaft is a regular (non-xochitl) client, always preloading the plain
  # rm2fb_client regardless of variant (see wrapWithClient.nix/xochitl.nix's
  # variant-only distinction), so this only exercises the server half of
  # the toggle: does rm2fb_server_swtcon start and serve a regular client
  # correctly. Reuses the same testScript/reference screenshot.
  yaft-swtcon = yaft-user.override {
    modules = [
      ../modules/remarkable.nix
      ../template/config.nix
      { services.rm2fb.variant = "swtcon"; }
    ];
  };

  koreader = mkTest {
    modules = [
      ../modules/remarkable.nix
      ../template/config.nix
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
      ../template/config.nix
    ];
    testScript = ''
      wait_for "startup.png"
      tap_at 782 1046
      wait_for "xochitl_3.20.png" 120
    '';
  };

  # Same scenario as `xochitl`, but with services.rm2fb.variant = "swtcon"
  # (nix/modules/rm2-display.nix) - the native swtcon reimplementation
  # server (rm2fb_server_swtcon) paired with xochitl's coexistence client
  # (librm2fb_client_swtcon.so, ClientSwtcon.cpp) instead of the by-address
  # hooking pair. Reuses the same testScript/reference screenshot: both
  # variants are expected to render xochitl identically.
  #
  # Unlike the "hook" pair above, the coexistence client deliberately does
  # NOT redirect xochitl's own /dev/fb0 access (that's the whole point -
  # xochitl drives the real framebuffer itself) - so unlike `xochitl`
  # above, this VM needs a working /dev/fb0 for xochitl's own (real,
  # unmodified) display init to succeed. This emulator doesn't have real
  # e-ink hardware, so mock it the same way this project's own swtcon
  # testing already does on-device (see CLAUDE.md's Building/Testing
  # section) - LD_PRELOAD libioctl-dump.so into xochitl too, after the
  # coexistence client so its ioctl()/open() hooks chain into the mock via
  # RTLD_NEXT instead of a real (nonexistent) kernel driver.
  xochitl-swtcon = xochitl.override {
    modules = [
      ../modules/remarkable.nix
      ../template/config.nix
      { services.rm2fb.variant = "swtcon"; }
      (
        { pkgs, ... }:
        {
          programs.xochitl.extraPreloadLibraries = [
            "${pkgs.rm2-stuff.ioctl_dump}/lib/libioctl-dump.so"
          ];

          # Test-only (the rtprio grant itself now lives in xochitl.nix,
          # applied whenever services.rm2fb.variant is "swtcon" - it's real
          # hardware behavior, not just a VM artifact): raise the core
          # limit so a crash actually produces a coredump instead of
          # "Resource limits disable core dumping", for debugging this test
          # specifically.
          security.pam.loginLimits = [
            {
              domain = "*";
              type = "-";
              item = "core";
              value = "unlimited";
            }
          ];
        }
      )
    ];
  };

  xochitl-nouser = xochitl.override {
    modules = [
      ../modules/remarkable.nix
      ../template/config.nix
      { programs.rocket.loginUser = lib.mkForce null; }
    ];
  };

  installer = mkTest {
    modules = [
      ../modules/remarkable.nix
      ../template/config.nix
    ];
    bootNixos = false;
    testScript = ''
      # "nixctl launch" itself now blocks (via systemd-notify --ready) until
      # yaft_reader has opened /dev/kmsg and is listening, so this echo can't
      # race its lseek(SEEK_END) and get silently dropped.
      in_vm nixos/nixctl launch
      in_vm 'echo test > /dev/kmsg'
      # Tolerant compare: unrelated kernel noise (e.g. "hrtimer: interrupt
      # took ..." warnings under a loaded host) can still land on /dev/kmsg
      # ahead of our echo and push "test" down a line or more, so a strict
      # pixel-perfect check against launch_test.png is non-deterministic.
      # /dev/kmsg writes can't be used to clear the console either - the
      # kernel hex-escapes control bytes written to it (e.g. ESC becomes the
      # literal text "\x1b"), specifically to block this kind of terminal
      # escape-sequence injection.
      wait_for_tolerant "launch_test.png"
      in_vm reboot

      while ! in_nixos true; do
        sleep 5
      done
      wait_for "startup.png"
    '';
  };

  # Same install/reboot scenario as `installer`, but with the resulting
  # NixOS config carrying services.rm2fb.variant = "swtcon" - checks that
  # rm2fb_server_swtcon comes up cleanly as a systemd service after a real
  # install+reboot (reaching the same "startup.png" launcher screen), not
  # just when directly booted via mkTest's default bootNixos = true path
  # like the other *-swtcon checks above.
  installer-swtcon = installer.override {
    modules = [
      ../modules/remarkable.nix
      ../template/config.nix
      { services.rm2fb.variant = "swtcon"; }
    ];
  };
}
