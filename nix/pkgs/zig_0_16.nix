# ghostty (hence ghostty-vt) requires Zig 0.16.0; the repo's pinned nixpkgs
# (nixos-25.11) only has 0.15.2. Override in place -- values match what
# nixpkgs' own zig package set uses for 0.16.0 (see
# pkgs/development/compilers/zig/default.nix upstream) -- rather than
# pulling in a second nixpkgs checkout just for this. Shared by flake.nix
# and nix/modules/overlay.nix so the version/hash live in one place.
{
  zig,
  llvmPackages_21,
}:
zig.override {
  version = "0.16.0";
  hash = "sha256-2sTMhaasyrKoBnyH/hQrNCbi0Vh6HekIrpE4XkyQulQ=";
  llvmPackages = llvmPackages_21;
}
