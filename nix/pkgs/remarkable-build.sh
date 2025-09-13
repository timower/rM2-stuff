#!@runtimeShell@

set -eu
set -o pipefail

export PATH=@path@
nix_installer='@nix_installer@/nix-installer.tar.xz'

FLAKE="$1"
TARGET="$2"

if ! ssh $TARGET test -d /nix; then
  echo "Nix not found, installing..."
  scp $nix_installer $TARGET:/tmp

  ssh $TARGET mkdir -p /tmp/unpack
  ssh $TARGET tar -C /tmp/unpack -xJf /tmp/nix-installer.tar.xz
  ssh $TARGET '/tmp/unpack/*/install'
  ssh $TARGET rm -rf '/tmp/unpack' /tmp/nix-installer.tar.xz
fi

echo "Building system..."
systemPath=$(nix build --json -- "$FLAKE.config.system.build.toplevel" | jq -r '.[0].outputs.out')

echo "Copying system..."
nix copy --to ssh://$TARGET?remote-program=/home/root/.nix-profile/bin/nix-store "$systemPath"

echo "Running activation..."
ssh $TARGET "$systemPath/activate"

echo "Done!"
