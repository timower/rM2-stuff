{
  lib,
  stdenv,
  fetchurl,
  libarchive,
  python3,
  file,
  which,
}:
stdenv.mkDerivation rec {
  pname = "remarkable2-toolchain";
  version = "5.0.58";

  src = fetchurl {
    url = "https://storage.googleapis.com/remarkable-codex-toolchain/3.20.0.92/rm2/remarkable-production-image-${version}-rm2-public-x86_64-toolchain.sh";
    # url = "https://storage.googleapis.com/remarkable-codex-toolchain/3.15.4.2/remarkable-platform-image-4.1.112-rm2-public-x86_64-toolchain.sh";
    sha256 = "sha256-rqrAxV9yJTyGpAA2ElURTcMb+yNxyVrI9Yi3k3d7K6I=";
    # sha256 = "sha256-dhnMnHhQ4cpDamqPSxN+Owc4009V6v+EzLrlZzbjt78=";
    executable = true;
  };

  nativeBuildInputs = [
    libarchive
    python3
    file
    which
  ];

  dontUnpack = true;
  dontBuild = true;

  installPhase = ''
    mkdir -p $out
    ENVCLEANED=1 $src -y -d $out || true
    rm -f $out/sysroots/cortexa7hf-neon-remarkable-linux-gnueabi/usr/lib/environment.d/99-environment.conf
    rm -f $out/sysroots/cortexa7hf-neon-remarkable-linux-gnueabi/etc/tmpfiles.d/etc.conf
    rm -f $out/sysroots/cortexa7hf-neon-remarkable-linux-gnueabi/etc/tmpfiles.d/home.conf
    rm -f $out/sysroots/cortexa7hf-neon-remarkable-linux-gnueabi/etc/resolv.conf
    rm -f $out/sysroots/cortexa7hf-neon-remarkable-linux-gnueabi/etc/mtab
    rm -f $out/sysroots/cortexa7hf-neon-remarkable-linux-gnueabi/etc/resolv-conf.systemd
    rm -f $out/sysroots/cortexa7hf-neon-remarkable-linux-gnueabi/var/lock
  '';

  meta = with lib; {
    description = "Toolchain for cross-compiling to reMarkable 2 tablets";
    homepage = "https://remarkable.engineering/";
    sourceProvenance = with sourceTypes; [ binaryNativeCode ];
    license = licenses.gpl2Plus;
    maintainers = with maintainers; [ tadfisher ];
    platforms = [ "x86_64-linux" ];
  };
}
