# Manual test checklist

`make test` covers the logic with no Windows in it — argument parsing, rectangle
parsing and trimming, monitor ordering and the default file name. Everything
below needs a real Windows machine, because it involves the screen, the mouse,
the clipboard or the console.

None of this needs a window manager. Run it from PowerShell alongside Explorer;
the last section is the only part that wants one.

Errors from runs without a console go to `%LOCALAPPDATA%\mcapture\mcapture.log`.

## The CLI

| # | Test | Expected |
|---|------|----------|
| 1 | `mcapture --version` | Prints the version, and nothing else. Exit 0. |
| 2 | `mcapture --help` | Prints the usage text to the console you typed it into. Exit 0. |
| 3 | `mcapture --list` | One line per monitor: index, device, `X,Y,W,H`, and `primary` on one of them, numbered left to right. |
| 4 | `mcapture --bogus` | An error naming `--bogus` and a hint to run `--help`. Exit 2. |
| 5 | `mcapture --screen --window` | An error naming both regions. Exit 2. |
| 6 | `mcapture -o shot.gif` | Refused: only `.png`, `.jpg`, `.bmp`. Exit 2. |
| 7 | `mcapture --rect 99999,0,10,10` | Refused: no area on the screen. Exit 2. |
| 8 | `$p = (mcapture --screen \| Out-String).Trim(); $LASTEXITCODE` | `$p` holds the saved path and the exit code is 0 — the pipe made PowerShell wait. |
| 9 | `$p = mcapture --screen` with no pipe | Returns at once with `$p` empty: a GUI program is not waited for. The file is still written. |

## Capturing

| # | Test | Expected |
|---|------|----------|
| 1 | `mcapture --screen` | A PNG in `Pictures\Screenshots` the size of the whole virtual screen, every monitor in it. Pasting into Paint gives the same image. |
| 2 | `mcapture --monitor 1` | Exactly monitor 1 from `--list`, at its physical resolution even on a scaled display. |
| 3 | `mcapture --monitor` with the cursor on another display | That display. |
| 4 | `mcapture --rect 100,100,640,480 -o r.png` | `r.png` is 640×480, taken from those pixels. The printed path is absolute. |
| 5 | `mcapture --rect -50,-50,200,200` | Trimmed to 150×150. |
| 6 | `mcapture --window --delay 2`, then click a window | That window's visible frame, without the invisible resize border. |
| 7 | `mcapture --screen --cursor` | The pointer is in the image. Without `--cursor` it is not. |
| 8 | `mcapture --screen --no-save` | No file, nothing printed, the image is on the clipboard. |
| 9 | `mcapture --screen --no-clipboard` | A file; the clipboard is unchanged. |
| 10 | `-o shot.jpg`, `-o shot.bmp` | Valid JPEG and BMP files. |
| 11 | Two `mcapture --screen` in the same second | The second file is `...-2.png`; nothing is overwritten. |
| 12 | Open the PNGs in an editor that shows transparency | Fully opaque — no checkerboard anywhere. |

## Selecting

| # | Test | Expected |
|---|------|----------|
| 1 | `mcapture` | The screen dims and freezes across every monitor; the cursor is a crosshair. |
| 2 | Drag a rectangle | It shows undimmed with an accent frame and a `W × H` label; releasing saves exactly that area. Exit 0. |
| 3 | Click on a window without dragging | That window is saved. |
| 4 | Click on an empty desktop | The monitor under the cursor is saved. |
| 5 | Move without clicking, press **Enter** | The highlighted window is saved. |
| 6 | Press **Esc** | The overlay closes, nothing is saved or copied. Exit 3. |
| 7 | Right-click | Same as Esc; no context menu opens in the app underneath. |
| 8 | Start `mcapture` twice | The second one refuses with "a selection is already open". |
| 9 | Open a menu, then `mcapture --delay 2` | The menu is in the frozen screen and in the image. |
| 10 | Drag across two monitors with different scaling | The selection follows the cursor exactly; the image spans both. |

## Under mshell

| # | Test | Expected |
|---|------|----------|
| 1 | Bind `mshell.exec("mcapture.exe")` and press it | No console flashes. The overlay covers everything, including the bar. |
| 2 | While the overlay is open, check `%LOCALAPPDATA%\mshell\mshell.log` | No `Managed:` or `Tracking:` line for the overlay — it is never tiled. |
| 3 | Press **Esc** straight after the hotkey, without touching the mouse | The overlay cancels. |
