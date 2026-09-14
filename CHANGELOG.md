# Changelog

All notable changes to mcapture are documented here. This project adheres to
[Semantic Versioning](https://semver.org/) (pre-1.0: minor = features + fixes).
Sections after 0.1.0 are generated from commit messages; see *Releases* in the
README.

## 0.1.0

### Added

- Screenshot CLI for Windows: drag-select over a frozen screen (a click takes
  the window under the cursor), the whole virtual screen, one monitor, the
  foreground window, or an explicit `X,Y,W,H` rectangle.
- Saves PNG, JPEG or BMP (by extension) to `Pictures\Screenshots` or `--output`,
  puts the image on the clipboard, and prints the saved path.
- `--delay`, `--cursor`, `--no-save`, `--no-clipboard`, and `--list` for the
  monitors in the same coordinates `--rect` takes.
- Exit status 0 ok, 1 failure, 2 bad usage, 3 cancelled.
- One-line PowerShell installer.
