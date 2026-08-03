#!/bin/sh
# Rewrite `#!/usr/bin/env foo [args]` shebangs under $1 (recursively) to an
# absolute interpreter path resolved from $PATH. nix sandboxed builds have
# no /usr/bin/env, so scripts extracted mid-build (outside nixpkgs' own
# patchShebangs machinery) need this done by hand.
set -eu

dir="$1"

find "$dir" -type f -perm -0100 2>/dev/null | while IFS= read -r f; do
    first_line=$(head -c 256 "$f" 2>/dev/null | head -n1) || continue
    case "$first_line" in
        '#!'*/env\ *)
            rest=${first_line#*/env }
            interp=${rest%% *}
            resolved=$(command -v "$interp" 2>/dev/null) || continue
            [ -n "$resolved" ] || continue
            args=${rest#"$interp"}
            sed -i "1s|.*|#!${resolved}${args}|" "$f"
            ;;
    esac
done
