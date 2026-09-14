# mcapture

A **screenshot CLI for Windows**. Drag a rectangle, click a window, name a
monitor or give exact pixels — the image is saved as PNG, JPEG or BMP and put on
the clipboard, and the path is printed so a script can pick it up.

```
mcapture                              drag a rectangle (click = that window)
mcapture --rect 100,100,640,480       exactly those pixels
mcapture --monitor 1 -o shot.jpg      the second monitor from --list, as JPEG
mcapture --window --no-save           the foreground window, clipboard only
```

It needs no window manager and runs fine under Explorer. It is a natural fit
for [mshell](https://github.com/blendonl/mshell) — a tiling WM that replaces
`explorer.exe`, where there is no Snipping Tool hotkey — and pairs with
[mrecord](https://github.com/blendonl/mrecord) for video.

## Usage

```
mcapture [region] [options]

Region (pick one; default --select):
  -s, --select          drag a rectangle over a frozen screen; click to take
                        the window under the cursor; Esc or right-click cancels
  -f, --screen          the whole virtual screen, every monitor
  -m, --monitor [N]     monitor N from --list; without N, the one under the cursor
  -w, --window          the foreground window's visible frame
  -r, --rect X,Y,W,H    an explicit rectangle, in physical screen pixels

Options:
  -o, --output PATH     .png, .jpg or .bmp; default
                        Pictures\Screenshots\mcapture-YYYYMMDD-HHMMSS.png
      --no-save         clipboard only
      --no-clipboard    file only
  -d, --delay SECONDS   wait before capturing (before the overlay for --select)
  -c, --cursor          include the mouse pointer
      --list            print the monitors: index, device, X,Y,W,H, primary
  -h, --help            show this help
  -V, --version         print the version
```

Long options also take their value after `=` (`--rect=0,0,800,600`).

On success the saved path is printed on standard output. The exit status says
what happened:

| Code | Meaning |
|------|---------|
| 0 | captured |
| 1 | something failed (the reason is on standard error) |
| 2 | bad usage, or a region with no area on the screen |
| 3 | the selection was cancelled |

### Selecting

`--select` freezes the screen first, so menus and tooltips stay put while you
aim, and the overlay is never in the picture.

- **Drag** to select a rectangle; its size is shown as you go and releasing the
  button takes it.
- **Click** without dragging to take the window under the cursor, trimmed to the
  screen. A window that covers its whole monitor — a fullscreen game, a desktop
  backdrop — takes that monitor.
- **Enter** takes whatever is highlighted; **Esc** or a **right-click** cancels.

### Coordinates

Everything is in physical pixels on the virtual screen, the same numbers
`--list` prints, so its output can be pasted straight into `--rect`:

```
> mcapture --list
0 \\.\DISPLAY1 0,0,3840,2160 primary
1 \\.\DISPLAY2 3840,0,3840,2160
```

Monitors are numbered left to right, then top to bottom. A rectangle that hangs
off the screen is trimmed to it.

### From a window manager

mcapture is a GUI-subsystem program, so launching it from a hotkey never
flashes a console. When it is run from a terminal it attaches to that console
for its output. Neither shell waits for a GUI program on its own, so a script
that needs the path or the exit status sends the output down a pipe, which
makes the shell wait:

```powershell
$path = (mcapture --rect 0,0,800,600 | Out-String).Trim()
$LASTEXITCODE
```

In `cmd.exe`, use `start /wait mcapture ...` or `for /f` over its output.

With mshell:

```lua
mshell.keys.submap("capture", {
    s = { function() mshell.exec("mcapture.exe") end, desc = "select" },
    w = { function() mshell.exec("mcapture.exe", "--window") end, desc = "window" },
    f = { function() mshell.exec("mcapture.exe", "--screen") end, desc = "screen" },
})
```

Errors from a run with no console are appended to
`%LOCALAPPDATA%\mcapture\mcapture.log`.

## Build

Cross-compiled from Linux with **mingw-w64**:

```sh
sudo apt install gcc-mingw-w64-x86-64
make            # -> mcapture.exe
make test       # host-side unit tests (argument parsing, geometry)
make dist       # -> dist/mcapture-<version>-win64.zip
```

## Releases

Every pull request merged into `main` is a release. While the PR is open, CI
works out the next version from its
[Conventional Commits](https://www.conventionalcommits.org/) and pushes a
`chore(release): vX.Y.Z` commit to the branch that sets `VERSION` in the
Makefile and writes the matching section of `CHANGELOG.md`. When the PR merges,
CI tags that version and publishes the zip with that section as its notes.

Before 1.0, `feat`, `fix` and breaking changes (`feat!:`, or a
`BREAKING CHANGE:` footer) bump the minor version. From 1.0, breaking changes
bump the major, `feat` the minor and `fix` the patch. Anything else bumps the
patch.

## Install

In PowerShell:

```powershell
irm https://raw.githubusercontent.com/blendonl/mcapture/main/install.ps1 | iex
```

That downloads the latest release into `%LOCALAPPDATA%\Programs\mcapture` and
adds the folder to your user `PATH`. Run it again to upgrade. To pin a version,
or install somewhere else:

```powershell
& ([scriptblock]::Create((irm https://raw.githubusercontent.com/blendonl/mcapture/main/install.ps1))) -Version 0.1.0 -InstallDir C:\Tools\mcapture
```

## License

MIT — see [LICENSE](LICENSE).
