#!/bin/sh

self="$(dirname "$0")"

mountdir="$HOME/.nix-mount"
dest="/nix"

nix="@nix@"
cacert="@cacert@"

if ! [ -d "$dest" ]; then
  mkdir -m 0755 $dest
  mkdir -p $mountdir
  mount --bind $mountdir $dest
fi

# The auto-chroot code in openFromNonUri() checks for the
# non-existence of /nix/var/nix, so we need to create it here.
mkdir -p "$dest/store" "$dest/var/nix"

printf "copying Nix to %s..." "${dest}/store" >&2
# Insert a newline if no progress is shown.
if [ ! -t 0 ]; then
  echo ""
fi

for i in $(cd "$self/store" >/dev/null && echo ./*); do
  if [ -t 0 ]; then
    printf "." >&2
  fi
  i_tmp="$dest/store/$i.$$"
  if [ -e "$i_tmp" ]; then
    rm -rf "$i_tmp"
  fi
  if ! [ -e "$dest/store/$i" ]; then

    cp -RP -a "$self/store/$i" "$i_tmp"
    chmod -R a-w "$i_tmp"
    chmod +w "$i_tmp"
    mv "$i_tmp" "$dest/store/$i"
    chmod -w "$dest/store/$i"
  fi
done
echo "" >&2

if ! "$nix/bin/nix-store" --load-db <"$self/.reginfo"; then
  echo "$0: unable to register valid paths" >&2
  exit 1
fi

mkdir -p /etc/nix
cat >/etc/nix/nix.conf <<EOF
build-users-group =
filter-syscalls = false
experimental-features = nix-command flakes
system = armv7l-linux
EOF

. "$nix/etc/profile.d/nix.sh"

NIX_LINK="$HOME/.nix-profile"

if ! "$nix/bin/nix-env" -i "$nix"; then
  echo "$0: unable to install Nix into your default profile" >&2
  exit 1
fi

# Install an SSL certificate bundle.
if [ -z "$NIX_SSL_CERT_FILE" ] || ! [ -f "$NIX_SSL_CERT_FILE" ]; then
  "$nix/bin/nix-env" -i "$cacert"
  export NIX_SSL_CERT_FILE="$NIX_LINK/etc/ssl/certs/ca-bundle.crt"
fi

added=
p=
p_sh=$NIX_LINK/etc/profile.d/nix.sh
p_fish=$NIX_LINK/etc/profile.d/nix.fish
# Make the shell source nix.sh during login.
for i in .bash_profile .bash_login .profile; do
  fn="$HOME/$i"
  if [ -w "$fn" ]; then
    if ! grep -q "$p_sh" "$fn"; then
      echo "modifying $fn..." >&2
      printf '\nif [ -e %s ]; then . %s; fi # added by Nix installer\n' "$p_sh" "$p_sh" >>"$fn"
    fi
    added=1
    p=${p_sh}
    break
  fi
done
for i in .zshenv .zshrc; do
  fn="$HOME/$i"
  if [ -w "$fn" ]; then
    if ! grep -q "$p_sh" "$fn"; then
      echo "modifying $fn..." >&2
      printf '\nif [ -e %s ]; then . %s; fi # added by Nix installer\n' "$p_sh" "$p_sh" >>"$fn"
    fi
    added=1
    p=${p_sh}
    break
  fi
done

if [ -d "$HOME/.config/fish" ]; then
  fishdir=$HOME/.config/fish/conf.d
  if [ ! -d "$fishdir" ]; then
    mkdir -p "$fishdir"
  fi

  fn="$fishdir/nix.fish"
  echo "placing $fn..." >&2
  printf '\nif test -e %s; . %s; end # added by Nix installer\n' "$p_fish" "$p_fish" >"$fn"
  added=1
  p=${p_fish}
fi

if [ -z "$added" ]; then
  cat >&2 <<EOF

Installation finished!  To ensure that the necessary environment
variables are set, please add the line

  . $p

to your shell profile (e.g. ~/.profile).
EOF
else
  cat >&2 <<EOF

Installation finished!  To ensure that the necessary environment
variables are set, either log in again, or type

  . $p

in your shell.
EOF
fi
