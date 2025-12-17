# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

Compile the application:
```bash
g++ -o wayland_monitor wayland_monitor.cpp $(pkg-config --cflags --libs gtk4)
```

Run the application:
```bash
./wayland_monitor
```

## Architecture

This is a single-file GTK4 C++ application that monitors open windows on Linux desktops.

**WaylandWindowMonitor class**: Main application class that creates a GTK window displaying a list of open windows. It polls window information every 1000ms using a GLib timeout.

**Window detection strategy** (in priority order):
1. **GNOME Shell D-Bus** (`busctl`): Evaluates JavaScript via `org.gnome.Shell.Eval` to get native Wayland windows
2. **wmctrl fallback**: Detects XWayland applications when GNOME method fails

**Key dependencies**:
- GTK4 for the UI
- External tools: `busctl`, `wmctrl`, `xdotool` (for active window detection)

**Note**: Forces Cairo renderer (`GSK_RENDERER=cairo`) to avoid Vulkan issues.
