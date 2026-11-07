#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf -- "$temporary"' EXIT

fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
assert_text() { [[ $(< "$1") == "$2" ]] || fail "Unexpected contents: $1"; }

new_home() {
    export HOME=$temporary/$1/'home with spaces'
    export XDG_CONFIG_HOME=$HOME/'custom config'
    export XDG_STATE_HOME=$HOME/'custom state'
    mkdir -p -- "$HOME"
}

run_installer() { bash "$root/scripts/install.sh" "$1" > /dev/null; }

new_home fresh
printf 'nautilus\n' > "$HOME/.gmrun_history"
run_installer install
[[ -x $HOME/.local/bin/gmrun-minimal ]] || fail 'Launcher is not executable.'
[[ -f $XDG_CONFIG_HOME/gmrun-minimal/gtkrc ]] || fail 'Theme was not installed.'
run_installer uninstall
[[ ! -e $HOME/.local/bin/gmrun-minimal && ! -e $HOME/.gmrunrc ]] || fail 'Installed files remain.'
assert_text "$HOME/.gmrun_history" nautilus
printf 'PASS: install, remove and preserve history\n'

new_home reinstall
printf 'Width = 700\n' > "$HOME/.gmrunrc"
run_installer install
printf 'Width = 900\n' > "$HOME/.gmrunrc"
printf 'gtk-font-name = "Sans 18"\n' > "$XDG_CONFIG_HOME/gmrun-minimal/gtkrc"
run_installer install
assert_text "$HOME/.gmrunrc" 'Width = 900'
assert_text "$XDG_CONFIG_HOME/gmrun-minimal/gtkrc" 'gtk-font-name = "Sans 18"'
run_installer uninstall
assert_text "$HOME/.gmrunrc" 'Width = 700'
printf 'PASS: keep edits on reinstall and restore original settings\n'

new_home symlink
printf 'Width = 640\n' > "$HOME/original-config"
ln -s original-config "$HOME/.gmrunrc"
run_installer install
[[ ! -L $HOME/.gmrunrc ]] || fail 'Installed config is still a symlink.'
assert_text "$HOME/original-config" 'Width = 640'
run_installer uninstall
[[ -L $HOME/.gmrunrc && $(readlink -- "$HOME/.gmrunrc") == original-config ]] || fail 'Symlink was not restored.'
assert_text "$HOME/.gmrunrc" 'Width = 640'
printf 'PASS: restore relative symlinks without changing their targets\n'

new_home broken-link
ln -s missing-config "$HOME/.gmrunrc"
run_installer install
run_installer uninstall
[[ -L $HOME/.gmrunrc && $(readlink -- "$HOME/.gmrunrc") == missing-config ]] || fail 'Broken symlink was not restored.'
printf 'PASS: restore broken symlinks\n'

new_home changed-paths
run_installer install
original_config=$XDG_CONFIG_HOME
export XDG_CONFIG_HOME=$HOME/'another config'
if run_installer install 2> "$temporary/error"; then fail 'Changed install paths were accepted.'; fi
[[ ! -e $XDG_CONFIG_HOME/gmrun-minimal/gtkrc ]] || fail 'Unexpected config was installed.'
run_installer uninstall
[[ ! -e $original_config/gmrun-minimal/gtkrc ]] || fail 'Recorded config path was not removed.'
printf 'PASS: reject changed install paths and uninstall using saved paths\n'

new_home missing-backup
printf 'Width = 600\n' > "$HOME/.gmrunrc"
run_installer install
rm -- "$XDG_STATE_HOME/gmrun-minimal/2/original"
if run_installer uninstall 2> "$temporary/error"; then fail 'Missing backup was accepted.'; fi
[[ -f $HOME/.gmrunrc && -x $HOME/.local/bin/gmrun-minimal ]] || fail 'Files were removed before backup validation.'
printf 'PASS: abort uninstall before changing files if a backup is missing\n'
