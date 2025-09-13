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
    rec {
      packages = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages."${system}";

          pkgsCross = pkgs.pkgsCross.remarkable2;
          pkgsArmv7 = pkgs.pkgsCross.armv7l-hf-multiplatform;

          nix-installer = pkgsArmv7.callPackage ./nix/pkgs/nix-installer.nix { };
        in
        rec {
          default = pkgs.callPackage ./nix/pkgs/rm2-stuff.nix { };
          rm2-toolchain = pkgs.callPackage ./nix/pkgs/rm2-toolchain.nix { };

          dev-cross = pkgsCross.callPackage ./nix/pkgs/rm2-stuff.nix { };
          dev-rm2-toolchain = pkgs.callPackage ./nix/pkgs/rm2-stuff.nix {
            toolchain_root = "${rm2-toolchain}";
          };

          inherit nix-installer;
          remarkable-build = pkgs.callPackage ./nix/pkgs/remarkable-build.nix {
            inherit nix-installer;
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
            packages = with pkgs; [
              clang-tools
              libllvm
            ];
          };
        }
      );

      lib.remarkableSystem =
        {
          modules,
          buildSystem ? "x86_64-linux",
        }:
        let
          rm2StuffPkgs = packages."${buildSystem}";

          baseModules = import ./nix/modules/module-list.nix;

          inputsModule =
            { ... }:
            {
              nixpkgs.buildPlatform = buildSystem;
              _module.args = {
                rm2-stuff = rm2StuffPkgs;
                nixpkgs = nixpkgs;
              };
            };
        in
        nixpkgs.lib.evalModules {
          modules = baseModules ++ [ inputsModule ] ++ modules;
          class = "remarkable";
        };

      remarkableConfigurations.example = lib.remarkableSystem {
        modules = [ ./nix/modules/examples/simple.nix ];
      };

    };
}
