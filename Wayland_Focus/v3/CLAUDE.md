# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

Build using Make (recommended):
```bash
make
```

Or compile directly:
```bash
g++ -o wayland_monitor wayland_monitor.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf gio-2.0) -std=c++17
```

Run the application:
```bash
./wayland_monitor
```

Install GNOME Shell extension:
```bash
make install-extension
# Then log out/in and run:
gnome-extensions enable window-introspect@focus
```

## Architecture

This project has two components that work together to display open windows on GNOME Wayland sessions:

**GNOME Shell Extension** (`extension.js`, `metadata.json`):
- Runs inside GNOME Shell to access window information (Wayland restricts direct access)
- Exposes D-Bus interface at `org.focus.WindowIntrospect` with `GetWindows` and `GetActiveWindow` methods
- Emits `WindowsChanged` and `FocusChanged` signals for real-time updates
- Supports GNOME 45-49 with compatibility wrapper for API changes in GNOME 48+

**SDL2 Monitor Application** (`wayland_monitor.cpp`):
- Single-file C++ application using SDL2 and SDL2_ttf for rendering
- `WaylandWindowMonitor` class connects to D-Bus and polls every 1000ms
- Window detection priority: custom extension D-Bus -> GNOME Shell Introspect fallback
- Requires `DejaVuSans.ttf` in the same directory as the executable

## D-Bus Testing

```bash
# Get all windows
gdbus call --session --dest org.gnome.Shell --object-path /org/focus/WindowIntrospect --method org.focus.WindowIntrospect.GetWindows

# Get active window
gdbus call --session --dest org.gnome.Shell --object-path /org/focus/WindowIntrospect --method org.focus.WindowIntrospect.GetActiveWindow
```

## GNOME 48+ Compatibility

The extension uses `_getWindowActors()` wrapper to handle API change:
- GNOME 45-47: `global.get_window_actors()`
- GNOME 48+: `global.compositor.get_window_actors()`
