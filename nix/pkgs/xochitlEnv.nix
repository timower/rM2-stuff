{
  lib,
  rustPlatform,
  util-linux,
}:
rustPlatform.buildRustPackage {
  pname = "xochitl-env";
  version = "0.1.0";
  src = ./xochitl-env-rust;
  cargoLock.lockFile = ./xochitl-env-rust/Cargo.lock;

  # Baked in as a compile-time constant (see src/main.rs) - $PATH can't be
  # trusted in a setuid context.
  BLKID_PATH = lib.getExe' util-linux "blkid";

  meta.mainProgram = "xochitl-env";
}
