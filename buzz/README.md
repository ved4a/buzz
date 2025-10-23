# Buzz: A Lightweight Resource Monitor

This folder contains a portable distribution of the Buzz terminal resource monitor.

What you get here:
- `buzz` — the compiled binary
- `run.sh` — launcher that opens a terminal when double-clicked from a file manager
- `buzz.desktop` — desktop launcher that runs in a terminal

Usage
- Double-click `buzz.desktop` to launch in a terminal with default settings.
- Or double-click `run.sh` (choose “Run in Terminal” if prompted).
- Or run from a shell with custom options, for example:
  - `./buzz --sort mem`
  - `./buzz --top 50`

Build and package
- From the repo root, run:
  - `bash dist/package.sh`
- This will build (if needed) and copy the binary into `dist/`.

Notes
- Some file managers require marking `.desktop` files as “Trusted” before double-clicking (right-click → Properties → Permissions → Allow launching).
- The launcher tries common terminal emulators when double-clicked outside a terminal. If none are installed, run from your terminal instead.
