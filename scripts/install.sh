#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
config=${XDG_CONFIG_HOME:-$HOME/.config}
state=${XDG_STATE_HOME:-$HOME/.local/state}/gmrun-minimal
sources=(build/appearance.so bin/gmrun-minimal config/gmrunrc config/gtkrc)
targets=("$HOME/.local/lib/gmrun-minimal/appearance.so"
         "$HOME/.local/bin/gmrun-minimal" "$HOME/.gmrunrc"
         "$config/gmrun-minimal/gtkrc")

fail() {
    printf 'gmrun-minimal: %s\n' "$*" >&2
    exit 1
}

exists() { [[ -e $1 || -L $1 ]]; }

check_record() {
    local entry=$1
    [[ -f $entry/target ]] || fail "Missing installation record: $entry"
    IFS= read -r -d '' recorded_target < "$entry/target" || fail "Invalid record: $entry"
    [[ $recorded_target == /* ]] || fail "Expected an absolute path in $entry"
    exists "$entry/original" || [[ -f $entry/absent ]] || fail "Missing backup: $entry"
}

install_files() {
    local i entry target saved
    for i in "${!sources[@]}"; do
        [[ -f $root/${sources[i]} ]] || fail "Missing ${sources[i]}. Run make first."
        target=${targets[i]}
        [[ $target == /* ]] || fail 'HOME and XDG paths must be absolute.'
        [[ ! -d $target || -L $target ]] || fail "Expected a file: $target"
        if [[ -d $state/$i ]]; then
            check_record "$state/$i"
            [[ $recorded_target == "$target" ]] || fail 'Installation paths changed. Uninstall first.'
        fi
    done
    (umask 077; mkdir -p -- "$state")
    printf '1\n' > "$state/format"
    for i in "${!sources[@]}"; do
        entry=$state/$i
        target=${targets[i]}
        # Keep local config edits when updating an existing installation.
        if [[ -d $entry && $i -ge 2 ]] && exists "$target"; then
            continue
        fi
        mkdir -p -- "$(dirname -- "$target")"
        if [[ ! -d $entry ]]; then
            saved=$(mktemp -d "$state/.backup.XXXXXX")
            if exists "$target"; then
                cp -a -- "$target" "$saved/original"
            else
                touch -- "$saved/absent"
            fi
            # A NUL terminator preserves spaces and newlines in paths.
            printf '%s\0' "$target" > "$saved/target"
            mv -- "$saved" "$entry"
        fi
        cp --remove-destination -- "$root/${sources[i]}" "$target"
        if [[ $i == 1 ]]; then chmod 755 -- "$target"; fi
    done
    printf 'Installed: %s\nOriginal files: %s\n' "${targets[1]}" "$state"
}

uninstall_files() {
    local i entry
    [[ -f $state/format && $(< "$state/format") == 1 ]] || fail 'No installation record found for this user.'
    # Check every backup before removing any installed files.
    for i in "${!sources[@]}"; do
        if [[ -d $state/$i ]]; then check_record "$state/$i"; fi
    done
    for ((i=${#sources[@]}-1; i>=0; i--)); do
        entry=$state/$i
        [[ -d $entry ]] || continue
        check_record "$entry"
        [[ ! -d $recorded_target || -L $recorded_target ]] || fail "Expected a file: $recorded_target"
    done
    for ((i=${#sources[@]}-1; i>=0; i--)); do
        entry=$state/$i
        [[ -d $entry ]] || continue
        check_record "$entry"
        rm -f -- "$recorded_target"
        if exists "$entry/original"; then
            mkdir -p -- "$(dirname -- "$recorded_target")"
            cp -a -- "$entry/original" "$recorded_target"
        fi
    done
    rm -rf -- "$state"
    printf 'Removed installed files and restored original settings.\n'
}

[[ $# == 1 ]] || fail 'Usage: bash scripts/install.sh install|uninstall'
case $1 in
    install|uninstall) ;;
    *) fail 'Usage: bash scripts/install.sh install|uninstall' ;;
esac
[[ $EUID != 0 ]] || fail 'Run as your desktop user, without sudo.'
[[ ! -e $state/installed.json ]] || fail 'Uninstall the previous installer version before switching to this one.'
if [[ -f $state/format ]]; then
    [[ $(< "$state/format") == 1 ]] || fail 'Unsupported installation record format.'
fi
case $1 in
    install) install_files ;;
    uninstall) uninstall_files ;;
esac
