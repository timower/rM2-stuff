{
  lib,

  stdenv,
  fetchzip,

  autoPatchelfHook,
  makeWrapper,
  libgcc,

  coreutils,
  dbus,
}:
stdenv.mkDerivation {
  pname = "koreader";
  version = "2025.08";

  src = fetchzip {
    url = "https://github.com/koreader/koreader/releases/download/v2025.08/koreader-remarkable-v2025.08.zip";
    hash = "sha256-QDxTfSSixFwGmW+2pfk9vzefXetNV8i9bxqnvsq2ScA=";
  };

  buildInputs = [ libgcc ];
  nativeBuildInputs = [
    autoPatchelfHook
    makeWrapper
  ];

  dontConfigure = true;
  dontBuild = true;

  installPhase = ''
    mkdir -p $out/bin $out/koreader
    cp -ar * $out/koreader

    # Instead of detecting xochitl, detect nixos in general.
    substituteInPlace $out/koreader/frontend/device.lua \
      --replace-fail /usr/bin/xochitl /etc/NIXOS

    makeWrapper $out/koreader/luajit $out/bin/koreader \
      --chdir $out/koreader \
      --add-flags $out/koreader/reader.lua \
      --set KO_MULTIUSER 1 \
      --set KO_DONT_GRAB_INPUT 1 \
      --set LD_PRELOAD /run/current-system/sw/lib/librm2fb_client.so \
      --set PATH ${
        lib.makeBinPath [
          coreutils
          dbus
        ]
      }
  '';

  meta.mainProgram = "koreader";
}
