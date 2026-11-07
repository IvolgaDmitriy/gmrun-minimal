# Installation and configuration

## Requirements

Use the GTK2 version of gmrun in an X11 session. This setup has been tested with
Ubuntu 24.04's gmrun 0.9.2 and GNOME. GTK3 builds and native Wayland are untested.

[gmrun's repository](https://github.com/wdlkmpx/gmrun) contains newer releases.
The [original project](https://sourceforge.net/projects/gmrun/) is on SourceForge.
This module uses GTK2 widgets, including the older completion list.

On Ubuntu:

```sh
sudo apt install gmrun gnome-terminal gtk2-engines-murrine build-essential pkg-config libgtk2.0-dev python3
make
make install
```

GTK2 development headers and pkg-config are needed to build `appearance.so`.
The Murrine engine draws the input field. GNOME Terminal runs commands that need
a terminal. The [Dracula palette](https://draculatheme.com/contribute#color-palette)
is included; installing the Dracula theme is optional.

`make install` calls `install.py` to copy files and save backups. `make uninstall`
uses the same script to restore them. Run these commands as your regular user.

Installation writes:

| File | Purpose |
| --- | --- |
| `~/.local/bin/gmrun-minimal` | Launcher |
| `~/.local/lib/gmrun-minimal/appearance.so` | GTK2 layout module |
| `~/.config/gmrun-minimal/gtkrc` | Font, colors and input padding |
| `~/.gmrunrc` | Command and history settings |

Configuration follows `XDG_CONFIG_HOME` when set. Backups and the installation
record go in `$XDG_STATE_HOME/gmrun-minimal`, or `~/.local/state/gmrun-minimal`.
Reinstalling updates the launcher and module and keeps edits to installed configs.
gmrun's own history stays in `~/.gmrun_history`.

## Alt+F2 in GNOME

1. Open Settings, Keyboard, then Custom shortcuts (inside View and customize shortcuts).
2. Edit the existing Alt+F2 shortcut, or add one named `gmrun-minimal`.
3. Set its command to the full path printed by `make install`, such as
   `/home/alex/.local/bin/gmrun-minimal`, and assign Alt+F2.

Use an absolute path in the shortcut; GNOME does not expand `~` like a shell.
If GNOME's built-in run dialog still takes the key, disable its binding:

```sh
gsettings get org.gnome.desktop.wm.keybindings panel-run-dialog
gsettings set org.gnome.desktop.wm.keybindings panel-run-dialog '[]'
```

Keep the first command's output if you want to restore that binding later.
The installer does not change desktop shortcuts.

## Commands

| Key | Action |
| --- | --- |
| Enter | Run the command |
| Ctrl+Enter | Run it in a terminal |
| Tab | Complete a command or path |
| Up / Down | Browse gmrun history |
| Ctrl+R | Search backward through history |
| Escape | Close the completion list or launcher |

`AlwaysInTerm` in `~/.gmrunrc` lists commands that open a terminal on Enter.
For example, typing `codex` and pressing Enter opens GNOME Terminal with Codex.
Codex is optional and is not installed or started by this project.

`Terminal` is the command for an empty Ctrl+Enter. `TermExec` receives the entered
command as one string. The supplied configuration uses:

```ini
Terminal = gnome-terminal
TermExec = ${Terminal} -- /bin/sh -c
```

For another terminal, use its command execution option with `sh -c` and check a
command with arguments. Shell aliases and functions from an interactive shell
are not loaded by this command. The launcher adds `~/.local/bin` and `~/bin` to PATH
and starts in your home directory.

## Appearance

Edit `~/.gmrunrc` to change `Width` (pixels), `History` (saved commands),
`AlwaysInTerm`, or whether hidden files appear in completion (`ShowDotFiles`).

Edit `~/.config/gmrun-minimal/gtkrc` to change the font and colors. Keep both
`gtk-font-name` and `font_name` in sync. `GtkEntry::inner-border` sets padding in
left, right, top, bottom order. Increase the last two values for a taller field.

In `gmrun-entry`, `base[SELECTED]` is the selection background and `text[SELECTED]`
is its text color. Use contrasting colors. `base[NORMAL]` and `base[ACTIVE]` set
the input background. Fonts fall back to an installed family if Ubuntu Sans is
unavailable.

The module centers the bar on the monitor containing the pointer when it opens.
It hides the status label along with the header, so completion status and errors
normally shown there are also hidden. The entry's context menu remains available.
The module is passed on gmrun's command line; launched applications do not inherit it.

Completion menu height and row height are `COMPLETION_HEIGHT` and `ROW_HEIGHT` in
`src/appearance.c`. After editing those constants, run `make install` to rebuild.
Config edits take effect the next time you open the launcher.

## Remove

```sh
make uninstall
```

This restores the files saved before the first installation. Edits made to the
installed configs are replaced by those backups. Remove or reassign the custom
GNOME shortcut separately. To restore GNOME's default run-dialog binding:

```sh
gsettings reset org.gnome.desktop.wm.keybindings panel-run-dialog
```

If you had a different binding, restore the value you saved instead.
