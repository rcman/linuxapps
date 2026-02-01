
<img width="505" height="494" alt="Screenshot From 2025-12-29 19-38-20" src="https://github.com/user-attachments/assets/2de38f9b-7872-48a9-9a44-9fc364ab6010" />



<BR>


# Wayland Window Monitor

A window monitoring application for GNOME Wayland sessions that displays currently open windows with focus indication. Consists of an SDL2-based monitor application and a GNOME Shell extension that exposes window information via D-Bus.

## Overview

Wayland's security model restricts applications from directly querying window information (unlike X11). This project works around that limitation by using a GNOME Shell extension to expose window data through D-Bus, which the SDL2 application then consumes and displays.

### Components

1. **GNOME Shell Extension** (`extension.js`, `metadata.json`)
   - Runs inside GNOME Shell
   - Exposes window information via D-Bus at `org.focus.WindowIntrospect`
   - Provides `GetWindows` and `GetActiveWindow` methods
   - Emits `WindowsChanged` and `FocusChanged` signals

2. **SDL2 Monitor Application** (`wayland_monitor.cpp`)
   - Standalone C++ application using SDL2 and SDL2_ttf
   - Connects to D-Bus to retrieve window information
   - Displays windows with focus highlighting
   - Updates every second

## GNOME 48/49 Compatibility Fix

### The Problem

After upgrading to **Fedora 43** (which ships with **GNOME 49**), the window monitor stopped displaying focus correctly. This was caused by a breaking API change introduced in GNOME 48.

### Root Cause

In **GNOME 48**, the `global.get_window_actors()` function was moved to `Meta.Compositor.get_window_actors()`. The old API is accessed through `global.compositor.get_window_actors()`.

**Old code (GNOME 45-47):**
```javascript
const windowActors = global.get_window_actors();
```

**New code (GNOME 48+):**
```javascript
const windowActors = global.compositor.get_window_actors();
```

### The Fix

The updated `extension.js` includes a compatibility wrapper that works with both old and new GNOME versions:

```javascript
_getWindowActors() {
    // GNOME 48+ uses global.compositor.get_window_actors()
    if (global.compositor && typeof global.compositor.get_window_actors === 'function') {
        return global.compositor.get_window_actors();
    }
    // GNOME 45-47 uses global.get_window_actors()
    if (typeof global.get_window_actors === 'function') {
        return global.get_window_actors();
    }
    // Fallback: empty array
    console.warn('Window Introspect: Unable to get window actors');
    return [];
}
```

### GNOME Version Reference

| Fedora Version | GNOME Version | API |
|----------------|---------------|-----|
| Fedora 40 | GNOME 46 | `global.get_window_actors()` |
| Fedora 41 | GNOME 47 | `global.get_window_actors()` |
| Fedora 42 | GNOME 48 | `global.compositor.get_window_actors()` |
| Fedora 43 | GNOME 49 | `global.compositor.get_window_actors()` |

## Installation

### Prerequisites

**For the SDL2 application:**
```bash
# Fedora
sudo dnf install SDL2-devel SDL2_ttf-devel glib2-devel gcc-c++

# Ubuntu/Debian
sudo apt install libsdl2-dev libsdl2-ttf-dev libglib2.0-dev g++
```

### Install the GNOME Shell Extension

1. Create the extension directory:
```bash
mkdir -p ~/.local/share/gnome-shell/extensions/window-introspect@focus
```

2. Copy the extension files:
```bash
cp extension.js metadata.json ~/.local/share/gnome-shell/extensions/window-introspect@focus/
```

3. Restart GNOME Shell:
   - On X11: Press `Alt+F2`, type `r`, press Enter
   - On Wayland: Log out and log back in

4. Enable the extension:
```bash
gnome-extensions enable window-introspect@focus
```

Or use the GNOME Extensions app or https://extensions.gnome.org

### Build the SDL2 Application

```bash
g++ -o wayland_monitor wayland_monitor.cpp \
    $(pkg-config --cflags --libs sdl2 SDL2_ttf gio-2.0) \
    -std=c++17
```

### Run

```bash
./wayland_monitor
```

Press `Escape` or close the window to exit.

## D-Bus Interface

The extension exposes the following D-Bus interface at `/org/focus/WindowIntrospect`:

### Methods

**GetWindows**
- Returns: JSON string containing array of window objects
- Each window object contains:
  - `title`: Window title
  - `appId`: Application ID
  - `wmClass`: WM_CLASS property
  - `hasFocus`: Boolean indicating focus state
  - `isHidden`: Boolean indicating minimized state
  - `windowType`: Meta.WindowType enum value
  - `clientType`: "wayland" or "x11"

**GetActiveWindow**
- Returns: JSON string of the currently focused window (or null)

### Signals

- `WindowsChanged`: Emitted when windows are created/destroyed
- `FocusChanged`: Emitted when focus changes, includes window info

### Testing the D-Bus Interface

```bash
# Get all windows
gdbus call --session \
    --dest org.gnome.Shell \
    --object-path /org/focus/WindowIntrospect \
    --method org.focus.WindowIntrospect.GetWindows

# Get active window
gdbus call --session \
    --dest org.gnome.Shell \
    --object-path /org/focus/WindowIntrospect \
    --method org.focus.WindowIntrospect.GetActiveWindow
```

## Troubleshooting

### Extension not loading

Check for errors in the GNOME Shell log:
```bash
journalctl -f -o cat /usr/bin/gnome-shell
```

### No windows detected

1. Verify the extension is enabled:
```bash
gnome-extensions list --enabled | grep window-introspect
```

2. Test the D-Bus interface manually (see above)

3. Check extension logs:
```bash
journalctl --user -f | grep -i "window introspect"
```

### Focus not updating (GNOME 48+)

If you're on GNOME 48 or newer and focus isn't updating, ensure you're using the updated `extension.js` with the `_getWindowActors()` compatibility wrapper.

## Other GNOME 48/49 API Changes

For reference, here are other notable API changes in GNOME 48 that may affect extensions:

| Old API | New API (GNOME 48+) |
|---------|---------------------|
| `global.get_window_actors()` | `global.compositor.get_window_actors()` |
| `Meta.get_window_actors()` | `Meta.Compositor.get_window_actors()` |
| `Meta.disable_unredirect_for_display()` | `Meta.Compositor.disable_unredirect()` |
| `Meta.enable_unredirect_for_display()` | `Meta.Compositor.enable_unredirect()` |
| `Meta.CursorTracker.get_for_display()` | `global.backend.get_cursor_tracker()` |

See the [GNOME Shell 48 porting guide](https://gjs.guide/extensions/upgrading/gnome-shell-48.html) for complete details.

## Files

```
window-introspect/
├── extension.js        # GNOME Shell extension (updated for GNOME 48/49)
├── metadata.json       # Extension metadata (supports GNOME 45-49)
├── wayland_monitor.cpp # SDL2 monitor application
└── README.md           # This file
```

## License

This project is provided as-is for educational and personal use.

## References

- [GNOME Shell Extension Development](https://gjs.guide/extensions/)
- [GNOME Shell 48 Porting Guide](https://gjs.guide/extensions/upgrading/gnome-shell-48.html)
- [GNOME Shell 49 Porting Guide](https://gjs.guide/extensions/upgrading/gnome-shell-49.html)
- [SDL2 Documentation](https://wiki.libsdl.org/SDL2/FrontPage)
- [GLib/GIO D-Bus Documentation](https://docs.gtk.org/gio/class.DBusConnection.html)
