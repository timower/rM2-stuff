{
  lib,
  pkgs,
  config,
  ...
}:
let
  setup-etc = pkgs.rustPlatform.buildRustPackage {
    pname = "setup-etc";
    version = "0.1.0";
    src = ./setup-etc-rust;
    cargoLock.lockFile = ./setup-etc-rust/Cargo.lock;
  };
in
{
  # The other half of the traditional (non-etc-overlay) activation's perl
  # dependency: user/group management via `update-users-groups.pl`. userborn
  # replaces it with a small Rust tool and doesn't require etc-overlay
  # either (it just writes /etc/passwd &co directly when etc isn't
  # immutable).
  services.userborn.enable = lib.mkDefault true;

  # `system.etc.overlay.enable` would let NixOS set up /etc without perl,
  # but it needs overlayfs (or, for its newer composefs-based mode, EROFS) -
  # neither is enabled in the reMarkable's kernel. Short of that, /etc setup
  # is otherwise hardcoded to shell out to `perl -MFile::Slurp`, pulling in
  # a ~55MB perl closure just for this one script. A POSIX-shell
  # reimplementation avoided the perl closure but forks find/grep/readlink
  # per file, which is slow enough to noticeably delay boot - swap in a
  # small Rust program instead (see setup-etc-rust for the exact semantics
  # it replicates). Adds no new runtime dependency beyond glibc (same as a
  # C rewrite), while getting memory safety from the language instead of
  # relying on hand-audited pointer arithmetic.
  system.build.etcActivationCommands = lib.mkIf (!config.system.etc.overlay.enable) (
    lib.mkForce ''
      echo "setting up /etc..."
      _etc_start=$(date +%s%N)
      ${setup-etc}/bin/setup-etc ${config.system.build.etc}/etc
      _etc_end=$(date +%s%N)
      echo "etc activation took $(( (_etc_end - _etc_start) / 1000000 )) ms"
    ''
  );
}
