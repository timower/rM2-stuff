{
  lib,
  pkgs,
  rm2-stuff,
  buildPlatform ? "x86_64-linux",
}:
let
  # For etc-activation's evalMinimalConfig, mirroring
  # nixpkgs' own nixos/tests/all-tests.nix helper.
  nixosLib = import "${pkgs.path}/nixos/lib" { featureFlags.minimalModules = { }; };
  evalMinimalConfig = module: nixosLib.evalModules { modules = [ module ]; };
  etcActivationTest = pkgs.callPackage ./etc-activation.nix { inherit evalMinimalConfig; };

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
      # No setupCommands/snapshot here (unlike vm-xochitl below): QEMU
      # unconditionally disables savevm/migration while a VirtFS 9p export is
      # mounted in the guest ("Migration is disabled when VirtFS export path
      # ... is mounted"), and vm-nixos keeps /nix/store mounted via 9p for its
      # entire runtime - so it always cold-boots instead. The testScript below
      # waits out that boot itself (via in_nixos), rather than relying on
      # setupCommands to pre-warm a snapshot.
      vm-nixos = system.config.system.build.vm-nixos;

      vm-xochitl = system.config.system.build.vm-xochitl.override {
        setupCommands = ''
          while ! ssh -o StrictHostKeyChecking=no -p 2222 root@localhost true; do
            sleep 1
          done
        '';
      };

      vm = if bootNixos then vm-nixos else vm-xochitl;

      fullTestScript =
        lib.optionalString bootNixos ''
          while ! in_nixos systemctl is-active rocket; do
            sleep 1
          done
        ''
        + testScript;

      driver = pkgs.callPackage ./driver.nix {
        inherit vm golden;
        testScript = fullTestScript;
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
{
  # Upstream nixpkgs' own etcActivationCommands correctness test
  # (nixos/modules/system/etc/test.nix), run against our Rust setup-etc.
  etc-activation = etcActivationTest;
}
// setNames rec {
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

  xochitl-latest = xochitl.override {
    modules = [
      ../modules/remarkable.nix
      ../template/config.nix
      { services.rm2fb.variant = "swtcon"; }
      { virtualisation.rmEmuVersion = "3.27.1.0"; }
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

  # Black-box A/B correctness check for the native swtcon reimplementation:
  # for each isolated test case in qsgepaper-test (tools/swtcon-test),
  # runs it once natively and once with SWTCON_LIBIMPL=1 (the real
  # libqsgepaper.so), then diffs the two SWTCON_PAN_CAPTURE sequences with
  # pan-capture-compare - the same technique, and the same environment, as
  # tools/swtcon-test/ab_compare.sh (see its header comment and
  # CLAUDE.md's Building/Testing section), just run automatically instead of
  # by hand against a real device/RemEmu over ssh.
  #
  # Deliberately boots the stock rootfs (bootNixos = false, like `installer`)
  # instead of NixOS: swtcon's waveform lookup
  # (libs/swtcon/init.cpp's find_waveform_path) and the real
  # libqsgepaper.so (/usr/lib/plugins/scenegraph/) only exist on the
  # device's own partitions - under a NixOS boot ("/" is the data partition
  # there) those are only reachable through xochitl-env's chroot, and even
  # that chroot doesn't mount every partition qsgepaper-test's standalone
  # swtcon needs (it mounts what xochitl itself needs). Booting the real
  # rootfs directly sidesteps all of that, and matches how every "confirmed
  # on the emulator" result in doc/swtcon_porting.md was actually obtained.
  swtcon-ab-compare = mkTest {
    modules = [
      ../modules/remarkable.nix
      ../template/config.nix
    ];
    bootNixos = false;
    testScript =
      let
        # Cross-built directly for the target, independent of the NixOS
        # module system above (it isn't what's booted here). Needs the
        # actual device toolchain (same as flake.nix's "dev-rm2-toolchain"
        # and CLAUDE.md's device build instructions), not plain nixpkgs
        # cross (pkgsCross.armv7l-hf-multiplatform) - that links against
        # nixpkgs' own glibc, whose dynamic linker path only exists under a
        # NixOS-managed /nix/store, not on the stock rootfs booted here.
        rm2-toolchain = pkgs.callPackage ../pkgs/rm2-toolchain.nix { };
        rm2-stuff-cross = pkgs.callPackage ../pkgs/rm2-stuff.nix { toolchain_root = "${rm2-toolchain}"; };
        sshOpts = "-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null";
      in
      ''
        REMOTE_DIR=/home/root

        scp -P 2222 ${sshOpts} \
          ${rm2-stuff-cross.tools}/bin/qsgepaper-test \
          ${rm2-stuff-cross.ioctl_dump}/lib/libioctl-dump.so \
          ${rm2-stuff-cross.ioctl_dump}/bin/pan-capture-compare \
          root@localhost:$REMOTE_DIR/

        # /tmp/epd.lock (libs/swtcon/init.cpp's create_pid_file) can still
        # be held right after boot by whatever the stock image auto-starts
        # that also touches the display - it has no real /dev/fb0/epaper
        # here, so it reliably exits (and releases the lock) within the
        # first couple seconds, but a qsgepaper-test invocation landing
        # before that loses the race and exits immediately with no capture.
        # Retry a quick init-only run (case "0" - init then straight to
        # shutdown, no interactive cases) until it succeeds before starting
        # the real loop below.
        for _ in 1 2 3 4 5; do
          if in_vm "cd $REMOTE_DIR && ./qsgepaper-test 0 >/dev/null 2>&1"; then
            break
          fi
          sleep 2
        done

        # Cases 8 ("burst of un-synced overlapping updates") and 9
        # ("in-flight active dependency") deliberately race real-time
        # completion/scheduling (see main.cpp's should_run comment), so
        # they're known-flaky under any A/B comparison and not a real
        # regression signal (see ab_compare.sh's header comment) - skipped
        # here just like ab_compare.sh's default (whole-suite) run. Case 11
        # (the combined 4x4 gray-transition grid) joins them: even split into
        # same-mode Sync batches, its 12 sequential commits still occasionally
        # show a single-frame mismatch (frame count varies slightly run to
        # run, unlike the deterministic cases below). Cases 12-27 cover the
        # same 16 (priming mode, transition mode) combinations individually
        # instead - one region, one mode per Sync commit - and give real
        # per-combination A/B coverage, including the 4 A2-priming
        # combinations (24-27) - see tools/swtcon-test/ab_compare.sh's header
        # comment for the history of why those 4 used to fail reproducibly
        # (an unrelated bordering outline rect forcing a ring-frame-slot
        # collision specific to A2's short LUT) and how dropping that
        # unnecessary border rect from run_transition_grid (main.cpp) fixed
        # it.
        #
        # Cases 12-23 (DU/GL16/GC16 priming) used to mismatch too, but that
        # was never a real content difference - it was capture_display
        # (tools/ioctl-dump/main.cpp) silently dropping a genuine
        # content-change event whenever the display's per-tick idle
        # keep-alive pan (idx==16, always the same static blank/white
        # content) happened to fire moments before a real transition that
        # hashed identically to that static content, purely a question of
        # real-time scheduling luck differing between native and
        # SWTCON_LIBIMPL=1 runs. capture_display now excludes idx==16 pans
        # from its own change-detection state, which fixed these cases
        # without needing any change to the test cases themselves.
        #
        # `yes` (no args - in_vm's own quoting doesn't nest cleanly with an
        # embedded empty-string argument) just as well satisfies
        # qsgepaper-test's getchar() prompts between cases as an
        # empty-string arg would; the byte value doesn't matter, only that
        # input keeps flowing.
        FAILED=""
        for n in 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27; do
          in_vm "cd $REMOTE_DIR && yes | LD_PRELOAD=./libioctl-dump.so SWTCON_PAN_CAPTURE=/tmp/native_$n.txt ./qsgepaper-test $n >/tmp/full_nat_$n.txt"
          in_vm "cd $REMOTE_DIR && yes | SWTCON_LIBIMPL=1 LD_PRELOAD=./libioctl-dump.so SWTCON_PAN_CAPTURE=/tmp/lib_$n.txt ./qsgepaper-test $n >/tmp/full_lib_$n.txt"

          if ! in_vm "cd $REMOTE_DIR && ./pan-capture-compare /tmp/native_$n.txt /tmp/lib_$n.txt"; then
            echo "-- lib $n --"
            in_vm "cat /tmp/lib_$n.txt"
            in_vm "cat /tmp/full_lib_$n.txt"
            echo "-- native $n --"
            in_vm "cat /tmp/native_$n.txt"
            in_vm "cat /tmp/full_nat_$n.txt"
            echo "-- end --"
            FAILED="$FAILED $n"
          fi
        done

        if [ -n "$FAILED" ]; then
          echo "swtcon A/B mismatch for test case(s):$FAILED" >&2
          exit 1
        fi
      '';
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
