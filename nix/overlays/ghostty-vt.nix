# Adds zig_0_16/ghostty-vt to any pkgs set (via `.extend` or
# `nixpkgs.overlays`), auto-targeting that set's own stdenv.hostPlatform.
final: prev: {
  zig_0_16 = final.callPackage ../pkgs/zig_0_16.nix { };
  ghostty-vt = final.callPackage ../pkgs/ghostty-vt.nix {
    zig = final.buildPackages.zig_0_16;
  };
}
