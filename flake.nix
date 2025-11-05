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
          linuxSystem = builtins.replaceStrings [ "darwin" ] [ "linux" ] system;
          pkgs = nixpkgs.legacyPackages."${system}";
          pkgsLinux = nixpkgs.legacyPackages."${linuxSystem}";

          pkgsCross = pkgs.pkgsCross.remarkable2;
          pkgsArmv7 = pkgs.pkgsCross.armv7l-hf-multiplatform;

          nix-installer = pkgsArmv7.callPackage ./nix/pkgs/nix-installer.nix { };

          rm-emu-packages = import ./nix/pkgs/rm-emu/default.nix {
            inherit (nixpkgs) lib;
            inherit pkgs pkgsLinux;
          };
        in
        rec {
          default = pkgs.callPackage ./nix/pkgs/rm2-stuff.nix { };
          rm2-toolchain = pkgs.callPackage ./nix/pkgs/rm2-toolchain.nix { };

          dev-cross = pkgsCross.callPackage ./nix/pkgs/rm2-stuff.nix { };
          dev-rm2-toolchain = pkgs.callPackage ./nix/pkgs/rm2-stuff.nix {
            toolchain_root = "${rm2-toolchain}";
          };

          koreader = pkgsArmv7.callPackage ./nix/pkgs/koreader.nix { };

          inherit nix-installer;
        }
        // rm-emu-packages
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

      nixosModules.default = ./nix/modules/remarkable.nix;

      nixosConfigurations = {
        example = nixpkgs.lib.nixosSystem {
          modules = [
            self.nixosModules.default
            ./nix/example.nix
          ];
        };

        darwin = nixpkgs.lib.nixosSystem {
          modules = [
            self.nixosModules.default
            ./nix/example.nix
            {
              virtualisation.host.pkgs = nixpkgs.legacyPackages."aarch64-darwin";
              nixpkgs.buildPlatform = "aarch64-linux";
            }
          ];

        };
      };

      checks = forAllSystems (
        system:
        import ./nix/tests {
          inherit (nixpkgs) lib;
          pkgs = nixpkgs.legacyPackages."${system}";
          rm2-stuff = self.packages."${system}".default;
        }
      );
    };
}
