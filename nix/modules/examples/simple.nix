{
  pkgs,

  ...
}:
{
  environment.systemPackages = with pkgs; [
    vim
    htop
  ];

  programs.yaft.enable = true;
  programs.tilem.enable = true;
  programs.rocket.enable = true;
}
