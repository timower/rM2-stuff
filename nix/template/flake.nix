{
  description = "Example reMakrable 2 NixOS system";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-unstable";
    rm2-stuff.url = "github:timower/rm2-stuff";
    rm2-stuff.inputs.nixpkgs.follows = "nixpkgs";
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
