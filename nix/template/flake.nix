{
  description = "Example reMakrable 2 NixOS system";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-unstable";
    rm2-stuff = {
      url = "git+https://github.com/timower/rM2-stuff.git?lfs=1";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      rm2-stuff,
    }:
    {
      nixosConfigurations.default = nixpkgs.lib.nixosSystem {
        modules = [
          rm2-stuff.nixosModules.default
          ./config.nix
        ];
      };
    };
}
