{
  config,
  lib,
  pkgs,
  ...
}:
let

  inherit (config.security) wrapperDir;

  wrappers = lib.filterAttrs (name: value: value.enable) config.security.wrappers;

  parentWrapperDir = dirOf wrapperDir;

  # This is security-sensitive code, and glibc vulns happen from time to time.
  # musl is security-focused and generally more minimal, so it's a better choice here.
  # The dynamic linker is still a fairly complex piece of code, and the wrappers are
  # quite small, so linking it statically is more appropriate.
  securityWrapper =
    sourceProg:
    pkgs.pkgsStatic.callPackage ./wrapper.nix {
      inherit sourceProg;

      # glibc definitions of insecure environment variables
      #
      # We extract the single header file we need into its own derivation,
      # so that we don't have to pull full glibc sources to build wrappers.
      #
      # They're taken from pkgs.glibc so that we don't have to keep as close
      # an eye on glibc changes. Not every relevant variable is in this header,
      # so we maintain a slightly stricter list in wrapper.c itself as well.
      unsecvars = lib.overrideDerivation (pkgs.srcOnly pkgs.glibc) (
        { name, ... }:
        {
          name = "${name}-unsecvars";
          installPhase = ''
            mkdir $out
            cp sysdeps/generic/unsecvars.h $out
          '';
        }
      );
    };

  ###### Activation script for the setcap wrappers
  mkSetcapProgram =
    {
      program,
      capabilities,
      source,
      owner,
      group,
      permissions,
      ...
    }:
    ''
      cp ${securityWrapper source}/bin/security-wrapper "$wrapperDir/${program}"

      # Prevent races
      chmod 0000 "$wrapperDir/${program}"
      chown ${owner}:${group} "$wrapperDir/${program}"

      # Set desired capabilities on the file plus cap_setpcap so
      # the wrapper program can elevate the capabilities set on
      # its file into the Ambient set.
      ${pkgs.libcap.out}/bin/setcap "cap_setpcap,${capabilities}" "$wrapperDir/${program}"

      # Set the executable bit
      chmod ${permissions} "$wrapperDir/${program}"
    '';

  ###### Activation script for the setuid wrappers
  mkSetuidProgram =
    {
      program,
      source,
      owner,
      group,
      setuid,
      setgid,
      permissions,
      ...
    }:
    ''
      cp ${securityWrapper source}/bin/security-wrapper "$wrapperDir/${program}"

      # Prevent races
      chmod 0000 "$wrapperDir/${program}"
      chown ${owner}:${group} "$wrapperDir/${program}"

      chmod "u${if setuid then "+" else "-"}s,g${if setgid then "+" else "-"}s,${permissions}" "$wrapperDir/${program}"
    '';

  mkWrappedPrograms = builtins.map (
    opts: if opts.capabilities != "" then mkSetcapProgram opts else mkSetuidProgram opts
  ) (lib.attrValues wrappers);
in
{
  ###### interface

  options = {
    security.enableWrappersNoCaps = lib.mkEnableOption "SUID/SGID wrappers" // {
      default = true;
    };
  };

  ###### implementation
  config = lib.mkIf config.security.enableWrappersNoCaps {

    # Disable upstream wrappers.
    security.enableWrappers = lib.mkForce false;

    assertions = lib.mapAttrsToList (name: opts: {
      assertion = opts.setuid || opts.setgid -> opts.capabilities == "";
      message = ''
        The security.wrappers.${name} wrapper is not valid:
            setuid/setgid and capabilities are mutually exclusive.
      '';
    }) wrappers;

    security.wrappers =
      let
        mkSetuidRoot = source: {
          setuid = true;
          owner = "root";
          group = "root";
          inherit source;
        };
      in
      {
        # These are mount related wrappers that require the +s permission.
        fusermount = mkSetuidRoot "${lib.getBin pkgs.fuse}/bin/fusermount";
        fusermount3 = mkSetuidRoot "${lib.getBin pkgs.fuse3}/bin/fusermount3";
        mount = mkSetuidRoot "${lib.getBin pkgs.util-linux}/bin/mount";
        umount = mkSetuidRoot "${lib.getBin pkgs.util-linux}/bin/umount";
      };

    # Make sure our wrapperDir exports to the PATH env variable when
    # initializing the shell
    environment.extraInit = ''
      # Wrappers override other bin directories.
      export PATH="${wrapperDir}:$PATH"
    '';

    security.apparmor.includes = lib.mapAttrs' (
      wrapName: wrap:
      lib.nameValuePair "nixos/security.wrappers/${wrapName}" ''
        include "${
          pkgs.apparmorRulesFromClosure { name = "security.wrappers.${wrapName}"; } [
            (securityWrapper wrap.source)
          ]
        }"
        mrpx ${wrap.source},
      ''
    ) wrappers;

    systemd.mounts = [
      {
        where = parentWrapperDir;
        what = "tmpfs";
        type = "tmpfs";
        options = lib.concatStringsSep "," ([
          "nodev"
          "mode=755"
          "size=${config.security.wrapperDirSize}"
        ]);
      }
    ];

    systemd.services.suid-sgid-wrappers = {
      description = "Create SUID/SGID Wrappers";
      wantedBy = [ "sysinit.target" ];
      before = [
        "sysinit.target"
        "shutdown.target"
      ];
      conflicts = [ "shutdown.target" ];
      after = [ "systemd-sysusers.service" ];
      unitConfig.DefaultDependencies = false;
      unitConfig.RequiresMountsFor = [
        "/nix/store"
        "/run/wrappers"
      ];
      serviceConfig.Type = "oneshot";
      script = ''
        chmod 755 "${parentWrapperDir}"

        # We want to place the tmpdirs for the wrappers to the parent dir.
        wrapperDir=$(mktemp --directory --tmpdir="${parentWrapperDir}" wrappers.XXXXXXXXXX)
        chmod a+rx "$wrapperDir"

        ${lib.concatStringsSep "\n" mkWrappedPrograms}

        if [ -L ${wrapperDir} ]; then
          # Atomically replace the symlink
          # See https://axialcorps.com/2013/07/03/atomically-replacing-files-and-directories/
          old=$(readlink -f ${wrapperDir})
          if [ -e "${wrapperDir}-tmp" ]; then
            rm --force --recursive "${wrapperDir}-tmp"
          fi
          ln --symbolic --force --no-dereference "$wrapperDir" "${wrapperDir}-tmp"
          mv --no-target-directory "${wrapperDir}-tmp" "${wrapperDir}"
          rm --force --recursive "$old"
        else
          # For initial setup
          ln --symbolic "$wrapperDir" "${wrapperDir}"
        fi
      '';
    };

    ###### wrappers consistency checks
    system.checks = lib.singleton (
      pkgs.runCommand "ensure-all-wrappers-paths-exist"
        {
          preferLocalBuild = true;
        }
        ''
          # make sure we produce output
          mkdir -p $out

          echo -n "Checking that Nix store paths of all wrapped programs exist... "

          declare -A wrappers
          ${lib.concatStringsSep "\n" (lib.mapAttrsToList (n: v: "wrappers['${n}']='${v.source}'") wrappers)}

          for name in "''${!wrappers[@]}"; do
            path="''${wrappers[$name]}"
            if [[ "$path" =~ /nix/store ]] && [ ! -e "$path" ]; then
              test -t 1 && echo -ne '\033[1;31m'
              echo "FAIL"
              echo "The path $path does not exist!"
              echo 'Please, check the value of `security.wrappers."'$name'".source`.'
              test -t 1 && echo -ne '\033[0m'
              exit 1
            fi
          done

          echo "OK"
        ''
    );
  };
}
