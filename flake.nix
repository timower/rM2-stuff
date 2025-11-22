{
  inputs.nixpkgs.url = "nixpkgs/nixos-unstable";

  nixConfig = {
    extra-substituters = [ "https://rm2-stuff.cachix.org" ];
    extra-trusted-public-keys = [
      "rm2-stuff.cachix.org-1:fl0LpfR74xZyeR4s94jljviKfoljLlxC4tdadeNRHcg="
    ];
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
      mkLinux = system: builtins.replaceStrings [ "darwin" ] [ "linux" ] system;
    in
    {
      packages = forAllSystems (
        system:
        let
          linuxSystem = mkLinux system;
          lib = nixpkgs.lib;

          pkgs = nixpkgs.legacyPackages."${system}";
          pkgsLinux = nixpkgs.legacyPackages."${linuxSystem}";
          pkgsArmv7 = pkgs.pkgsCross.armv7l-hf-multiplatform;

          isDarwin = pkgs.stdenv.isDarwin;

          nix-installer = pkgsArmv7.callPackage ./nix/pkgs/nix-installer.nix { };
          rm-emu-packages = import ./nix/pkgs/rm-emu/default.nix {
            inherit (nixpkgs) lib;
            inherit pkgs pkgsLinux;
          };
        in
        {
          default = pkgs.callPackage ./nix/pkgs/rm2-stuff.nix { };
          dev-cross = pkgsArmv7.callPackage ./nix/pkgs/rm2-stuff.nix { };

          koreader = pkgsArmv7.callPackage ./nix/pkgs/koreader.nix { };
          inherit nix-installer;
        }
        // lib.optionalAttrs (!isDarwin) rec {
          rm2-toolchain = pkgsLinux.callPackage ./nix/pkgs/rm2-toolchain.nix { };
          dev-rm2-toolchain = pkgsLinux.callPackage ./nix/pkgs/rm2-stuff.nix {
            toolchain_root = "${rm2-toolchain}";
          };
        }
        // rm-emu-packages
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages."${system}";
          packages = self.packages."${system}";
          isDarwin = pkgs.stdenv.isDarwin;
        in
        {
          default = pkgs.mkShell {
            hardeningDisable = [ "all" ];
            inputsFrom = [ packages.default ];
            packages = with pkgs; [
              clang-tools
              libllvm
              lldb
            ];
          };
        }
        // nixpkgs.lib.optionalAttrs (!isDarwin) {
          rm2-kernel = pkgs.mkShell {
            inputsFrom = [ pkgs.linux ];
            packages = [
              packages.rm2-toolchain
              pkgs.libyaml
            ];
            shellHook = ''
              source "${packages.rm2-toolchain}/environment-setup-cortexa7hf-neon-remarkable-linux-gnueabi"
            '';
          };
        }
      );

      nixosModules.default = ./nix/modules/remarkable.nix;

      nixosConfigurations = {
        example = nixpkgs.lib.nixosSystem {
          modules = [
            self.nixosModules.default
            ./nix/template/config.nix
          ];
        };

        darwin = nixpkgs.lib.nixosSystem {
          modules = [
            self.nixosModules.default
            ./nix/template/config.nix
            {
              virtualisation.host.pkgs = nixpkgs.legacyPackages."aarch64-darwin";
              nixpkgs.buildPlatform = "aarch64-linux";
            }
          ];

        };
      };

      templates.default = {
        path = ./nix/template;
        description = "reMarkable 2 NixOS config flake";
      };

      checks = forAllSystems (
        system:
        import ./nix/tests {
          inherit (nixpkgs) lib;
          pkgs = nixpkgs.legacyPackages."${system}";
          rm2-stuff = self.packages."${system}".default;
          buildPlatform = mkLinux system;
        }
      );
    };

}
