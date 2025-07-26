{
  inputs = {
    nixpkgs.url = "nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      # System types to support.
      supportedSystems = [
        "x86_64-linux"
        "x86_64-darwin"
        "aarch64-linux"
        "aarch64-darwin"
      ];

      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages."${system}";
        in
        rec {
          default = pkgs.callPackage ./nix/default.nix { };
          rm2-toolchain = pkgs.callPackage ./nix/rm2-toolchain.nix { };
          dev = pkgs.callPackage ./nix/default.nix {
            preset = "dev";
            toolchain_root = "${rm2-toolchain}";
          };
        }
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages."${system}";
          packages = self.packages."${system}";
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ packages.default ];
            packages = with pkgs; [ clang-tools ];
          };
        }
      );
    };
}
