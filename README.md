# Plugwatch

Check the [Omarchy Plugin Marketplace](https://plugins.omarchy.org/) for new community plugins, browse by official category, and search names, summaries, authors, or tags.

Plugwatch stores a catalog snapshot locally. A daily systemd user timer checks for new listings and keeps discoveries marked until you mark them seen. Set the morning check time in the app or bar widget using 24-hour `HH:MM`; both controls update the same timer. The first successful check establishes a baseline; later checks highlight added listings. Refresh at any time with **Space** or **Ctrl+R**.

| Key | Action |
|---|---|
| Space, Ctrl+R | Check the marketplace |
| / | Focus search |
| ? | Show shortcuts |
| Q | Quit |

Plugin install commands are copied to the clipboard, never run automatically. Listings without an automatic command link to their upstream repository for setup instructions. Community plugins are third-party code; review a plugin's source and instructions before installing.

## Marketplace installation

PlugWatch consists of a shell widget and a compiled Qt desktop companion.
**Installing the marketplace widget alone does not install the companion or
enable its timer.** Without the companion, the widget cannot check the catalog,
open the full browser, or change the schedule.

Install the companion explicitly from this public repository:

```sh
git clone https://github.com/kelmankenberg/PlugWatch.git
cd PlugWatch
bin/build
bin/test
bin/install
```

`bin/install` also enables a daily user timer, performs a first catalog check,
and enables the bar widget. Review the script before running it. It attempts
an Arch package installation when non-interactive sudo is available; otherwise
it installs the application and user units in your home directory. It does not
silently obtain administrator access.

If you prefer the widget to be managed and updated by the Omarchy plugin CLI,
install it from Git **before** running `bin/install`:

```sh
omarchy plugin add https://github.com/kelmankenberg/PlugWatch.git --enable
```

The companion installer preserves an existing Git-managed widget checkout.
The stable plugin ID is `plugwatch.bar-widget`.

## Build and install

Requires an Arch/Omarchy desktop with `qmake6`, a C++17 compiler, `make`,
Qt 6.5 or later (Base/Declarative/Wayland), `makepkg`, and systemd user services. Qt Test
is used by the test runner. These dependencies must already be installed;
the plugin CLI does not install them. Build and test with:

```sh
bin/build
bin/test
```

Install the package and enable its daily check timer with:

```sh
bin/install
```

The launcher entry is **Plugwatch**. To stop the daily background check, run `systemctl --user disable --now plugwatch-check.timer`.

The installer also adds the **Plugwatch** widget to the right side of the Omarchy bar and performs an initial catalog check. Click it to see new listings; choose a listing to open it in the full app, or use **Show all new in Plugwatch**. The widget includes an **Open Plugwatch** button for the full catalog. Run `omarchy plugin disable plugwatch.bar-widget` to remove it from the bar.

## Data and privacy

The app fetches the public `https://plugins.omarchy.org/catalog.json` catalog. It stores the cached listings, the last check time, and known/new plugin IDs in Qt's per-user application data directory. It does not install plugins or send search terms to the marketplace.

On a default installation, that data directory is `~/.local/share/plugwatch/`
(`state.json` and `widget.json`). `XDG_DATA_HOME` can override its parent.
The widget polls the local state file every minute; the network catalog check
is separate and runs when the app opens, when manually requested, and through
the enabled daily timer. The default check is at 08:00 local time. Persistent
systemd timers can catch up a missed check on the next login.

Changing the time writes
`~/.config/systemd/user/plugwatch-check.timer.d/50-plugwatch.conf`, reloads
systemd, and restarts the timer. The widget and app share this setting.
Thumbnail requests go to `https://plugins.omarchy.org/assets/img/plugins/`.
Opening upstream links launches your browser; copying install commands changes
your clipboard but does not execute them. The marketplace receives normal
request information such as your IP address. No account, telemetry, or secrets
are required.

The shell widget runs unsandboxed with your user permissions. The desktop app
is a separate native process, not another Quickshell instance.

## Update and remove

For a Git-managed widget:

```sh
omarchy plugin update plugwatch.bar-widget
```

Update the companion source separately with `git pull --ff-only`, rebuild,
test, and run `bin/install` explicitly. Updating the widget does not rebuild
the native companion.

Disable scheduled network checks before removing the companion:

```sh
systemctl --user disable --now plugwatch-check.timer
systemctl --user stop plugwatch-check.service
omarchy plugin remove plugwatch.bar-widget
```

If installed as an Arch package, remove it with:

```sh
omarchy pkg remove
```

Select `plugwatch` in the package picker.

If installed per-user, remove only these PlugWatch-owned files:

```sh
rm -f ~/.local/bin/plugwatch \
  ~/.local/share/applications/plugwatch.desktop \
  ~/.local/share/icons/hicolor/scalable/apps/plugwatch.svg \
  ~/.config/systemd/user/plugwatch-check.service \
  ~/.config/systemd/user/plugwatch-check.timer
```

For either installation, you may remove the owned schedule override:

```sh
rm -f ~/.config/systemd/user/plugwatch-check.timer.d/50-plugwatch.conf
systemctl --user daemon-reload
```

Adjust paths if using custom XDG directories. Disabling/removing the widget
alone does **not** disable the timer or remove the app. Cached catalog and seen
state are preserved; optionally delete only `state.json` and `widget.json` in
your PlugWatch data directory to reset them.

## Validation and publishing status

```sh
bin/build
bin/test
omarchy plugin validate .
qmllint -I "$OMARCHY_PATH/shell" bar-widget/v3/BarWidget.qml
```

Run manifest validation from a clean Git clone: local `makepkg` debug outputs
can contain symlinks and are deliberately excluded from version control.
GitHub Actions builds the companion and runs its offline tests; Omarchy shell
validation and desktop interaction checks need an actual Omarchy installation.

This repository is being prepared for the
[Omarchy Plugin Marketplace](https://plugins.omarchy.org/publish.html).
No approval is claimed. The companion build, install permissions, timer,
network behavior, and separate removal steps must be disclosed in the listing.

## License

[MIT](LICENSE).
