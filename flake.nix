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
          frida_gum = pkgs.fetchzip {
            url = "https://github.com/frida/frida/releases/download/16.1.4/frida-gum-devkit-16.1.4-linux-x86_64.tar.xz";
            hash = "sha256-WT8RjRj+4SKK3hIhw6Te/GuIy134zk2CXvpLWOqOJcM=";
            stripRoot = false;
          };
          expected = pkgs.fetchzip {
            url = "https://github.com/TartanLlama/expected/archive/refs/tags/v1.1.0.tar.gz";
            hash = "sha256-AuRU8VI5l7Th9fJ5jIc/6mPm0Vqbbt6rY8QCCNDOU50=";
          };
          catch2 = pkgs.fetchzip {
            url = "https://github.com/catchorg/Catch2/archive/refs/tags/v3.4.0.tar.gz";
            hash = "sha256-DqGGfNjKPW9HFJrX9arFHyNYjB61uoL6NabZatTWrr0=";
          };
          utfcpp = pkgs.fetchzip {
            url = "https://github.com/nemtrif/utfcpp/archive/refs/tags/v4.0.0.tar.gz";
            hash = "sha256-IuFbWS5rV3Bdmi+CqUrmgd7zTmWkpQksRpCrhT499fM=";
          };
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "rM2-stuff";
            version = "master";
            src = ./.;

            buildInputs = with pkgs; [
              sdl2-compat.dev
              systemdLibs.dev
              libevdev
            ];

            nativeBuildInputs = with pkgs; [
              clang
              cmake
              ninja
              pkg-config
              xxd
              ncurses
            ];

            cmakeFlags = [
              "-DFETCHCONTENT_SOURCE_DIR_FRIDA-GUM=${frida_gum}"
              "-DFETCHCONTENT_SOURCE_DIR_EXPECTED=${expected}"
              "-DFETCHCONTENT_SOURCE_DIR_CATCH2=${catch2}"
              "-DFETCHCONTENT_SOURCE_DIR_UTFCPP=${utfcpp}"
              "--preset=dev-host"
              "-B."
            ];
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
