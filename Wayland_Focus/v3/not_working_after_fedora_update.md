# Fedora D-Bus Updates and Window Focus Monitoring Issue

## Initial Question

**User:** Good morning, do you know of any fedora dbus updates that would affect my c++ /sdl2 code that monitors window focus

## Initial Analysis

Based on my search, I haven’t found specific recent D-Bus updates in Fedora that would directly break window focus monitoring in SDL2/C++ applications. However, there are some important related changes you should be aware of:

### Major SDL-related Changes

- **Fedora 42** (upcoming) proposes replacing SDL2 with sdl2-compat, which transparently uses SDL3 underneath. This means your SDL2 code would actually run on SDL3 via a compatibility layer. While it should be transparent, it’s worth testing when it arrives.

### Wayland-related Changes Affecting Window Focus

- **Fedora 41+** has been pushing Wayland as the default, with Fedora 41’s Workstation edition only including Wayland GNOME packages on install media
- **Fedora 43** (future) will completely remove the GNOME X11 session
- If your SDL2 code monitors window focus using X11-specific mechanisms, this could be problematic on Wayland

### Potential Concerns for Your Code

1. SDL2’s window focus events should still work through both X11 and Wayland backends, but if you’re making direct D-Bus calls or using X11-specific focus tracking, you might hit issues
1. There was an old SDL2 D-Bus bug (from 2017, SDL 2.0.6) related to IBus input method integration that caused D-Bus assertion failures, but that was fixed years ago

### What You Should Check

- Test your application on Wayland to ensure SDL_WINDOWEVENT_FOCUS_GAINED/LOST events still work correctly
- If you’re using any X11-specific window focus APIs alongside SDL2, those won’t work on Wayland
- Monitor for the Fedora 42 sdl2-compat change if you’re planning to support that release

Are you experiencing a specific issue with window focus monitoring, or are you planning ahead for compatibility?

## User Provides Repository Link

**User:** Here’s the link to my software. It no longer gives focus on Fedora 43: https://github.com/rcman/linuxapps/tree/main/Wayland_Focus

## Further Investigation

I can see the GitHub page exists but I’m unable to directly fetch the source code. However, based on the repository name “Wayland_Focus” and your description that it monitors window focus and no longer works on Fedora 43, I can explain what’s likely happening:

### The Problem

**Fedora 43 removed GNOME’s X11 session entirely** - this is the most critical change. According to the release notes I found, Fedora 43 removed the GNOME X11 session completely, forcing all users to Wayland-only sessions.

### Why Your Focus Monitor Likely Broke

If your application uses any of these approaches, they won’t work on Wayland-only Fedora 43:

1. **X11-specific focus tracking** (XGetInputFocus, _NET_ACTIVE_WINDOW property)
1. **Direct D-Bus calls to X11-specific interfaces**
1. **SDL2’s X11 backend** without proper Wayland support

### Solutions for Fedora 43

Since you’re using SDL2, you have a few options:

#### Option 1: Use SDL2’s Native Wayland Backend

Ensure SDL2 is running with its Wayland backend:

```bash
export SDL_VIDEODRIVER=wayland
```

SDL2’s window events (SDL_WINDOWEVENT_FOCUS_GAINED/LOST) should work on Wayland.

#### Option 2: Use org.gnome.Shell D-Bus Interface (Wayland-compatible)

On GNOME Wayland, you can track the focused window via D-Bus:

```bash
gdbus monitor --session --dest org.gnome.Shell --object-path /org/gnome/Shell
```

#### Option 3: Use ext-foreign-toplevel-list Protocol

For Wayland compositors, there’s a protocol specifically for monitoring window lists and focus. This requires using Wayland protocols directly.

Could you share what method your code currently uses to monitor focus? That would help me give you more specific guidance on adapting it for Fedora 43’s Wayland-only environment.

## User Requests Full Repository Check

**User:** Check this repo for the code: https://github.com/rcman/linuxapps

I’m having difficulty directly accessing the source files from your repository. However, based on the repository structure and what I can see, let me provide you with targeted guidance:

### The Core Issue

**Fedora 43 removed GNOME’s X11 session entirely.** If your `Wayland_Focus` application was relying on any X11-specific APIs or D-Bus interfaces for monitoring window focus, it will no longer work.

### Common Broken Approaches on Wayland-Only Systems

1. **X11 Window Properties** - APIs like `XGetInputFocus()`, `_NET_ACTIVE_WINDOW` won’t work
1. **X11-specific D-Bus calls** - Any D-Bus methods tied to X11 window management
1. **SDL2 X11 backend** - If forcing X11 mode, it won’t initialize on Wayland-only systems

### Solutions for Fedora 43 (GNOME Wayland)

#### Option 1: Use GNOME Shell D-Bus Interface

This is the most reliable method for GNOME Wayland - monitor focus changes via D-Bus using the org.gnome.Shell interface.

## User Provides Extension Code

**User:** Here’s part of the code:

```javascript
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';

const DBUS_INTERFACE = `
<node>
  <interface name="org.focus.WindowIntrospect">
    <method name="GetWindows">
      <arg type="s" direction="out" name="windows"/>
    </method>
    <method name="GetActiveWindow">
      <arg type="s" direction="out" name="window"/>
    </method>
    <signal name="WindowsChanged"/>
    <signal name="FocusChanged">
      <arg type="s" name="window"/>
    </signal>
  </interface>
</node>`;

export default class WindowIntrospectExtension {
    _dbusImpl = null;
    _windowTracker = null;
    _focusWindowId = null;
    _focusChangedId = null;
    _windowCreatedId = null;
    _windowDestroyedId = null;

    enable() {
        this._windowTracker = Shell.WindowTracker.get_default();

        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(DBUS_INTERFACE, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/focus/WindowIntrospect');

        // Connect to focus changes
        const display = global.display;
        this._focusChangedId = display.connect('notify::focus-window', () => {
            this._onFocusChanged();
        });

        // Connect to window creation/destruction
        this._windowCreatedId = display.connect('window-created', () => {
            this._dbusImpl.emit_signal('WindowsChanged', null);
        });

        // Track window destruction via workspace
        const workspaceManager = global.workspace_manager;
        this._windowDestroyedId = workspaceManager.connect('workspace-switched', () => {
            this._dbusImpl.emit_signal('WindowsChanged', null);
        });

        console.log('Window Introspect extension enabled');
    }

    disable() {
        if (this._focusChangedId) {
            global.display.disconnect(this._focusChangedId);
            this._focusChangedId = null;
        }

        if (this._windowCreatedId) {
            global.display.disconnect(this._windowCreatedId);
            this._windowCreatedId = null;
        }

        if (this._windowDestroyedId) {
            global.workspace_manager.disconnect(this._windowDestroyedId);
            this._windowDestroyedId = null;
        }

        if (this._dbusImpl) {
            this._dbusImpl.unexport();
            this._dbusImpl = null;
        }

        this._windowTracker = null;
        console.log('Window Introspect extension disabled');
    }

    _onFocusChanged() {
        const focusWindow = global.display.focus_window;
        if (focusWindow) {
            const windowInfo = this._getWindowInfo(focusWindow);
            this._dbusImpl.emit_signal('FocusChanged',
                new GLib.Variant('(s)', [JSON.stringify(windowInfo)]));
        }
    }

    _getWindowInfo(metaWindow) {
        const app = this._windowTracker.get_window_app(metaWindow);
        return {
            title: metaWindow.get_title() || '',
            appId: app ? app.get_id() : '',
            wmClass: metaWindow.get_wm_class() || '',
            hasFocus: metaWindow.has_focus(),
            isHidden: metaWindow.minimized,
            windowType: metaWindow.get_window_type(),
            clientType: metaWindow.get_client_type() === Meta.WindowClientType.WAYLAND ? 'wayland' : 'x11'
        };
    }

    GetWindows() {
        const windows = [];
        const windowActors = global.get_window_actors();

        for (const actor of windowActors) {
            const metaWindow = actor.get_meta_window();
            if (!metaWindow)
                continue;

            // Skip special windows
            const windowType = metaWindow.get_window_type();
            if (windowType === Meta.WindowType.DESKTOP ||
                windowType === Meta.WindowType.DOCK ||
                windowType === Meta.WindowType.SPLASHSCREEN)
                continue;

            // Skip windows that should not appear in taskbar
            if (metaWindow.is_skip_taskbar())
                continue;

            windows.push(this._getWindowInfo(metaWindow));
        }

        return JSON.stringify(windows);
    }

    GetActiveWindow() {
        const focusWindow = global.display.focus_window;
        if (focusWindow) {
            return JSON.stringify(this._getWindowInfo(focusWindow));
        }
        return JSON.stringify(null);
    }
}
```

## Final Analysis and Solution

Ah! Now I can see the issue clearly. You have a **GNOME Shell extension** that exposes window focus information via D-Bus, and your C++/SDL2 application likely connects to this D-Bus service to monitor focus changes.

### The Problem on Fedora 43

The issue isn’t with D-Bus itself, but rather **your GNOME Shell extension may not be loaded or enabled** on Fedora 43. This could be due to:

1. **GNOME version incompatibility** - Fedora 43 ships with GNOME 48, and your extension’s `metadata.json` might not declare support for it
1. **Extension not installed/enabled** after the upgrade
1. **Changes in GNOME 48’s API** that break the extension

### Diagnostic Steps

First, check if your extension is running:

```bash
# Check if the extension is enabled
gnome-extensions list --enabled | grep -i window

# Check extension status
gnome-extensions info window-introspect@focus.org

# Check for errors in the journal
journalctl -f -o cat /usr/bin/gnome-shell
```

Test if the D-Bus service is available:

```bash
# Check if your D-Bus interface is registered
busctl --user list | grep org.focus.WindowIntrospect

# Try calling your D-Bus method directly
busctl --user call org.focus.WindowIntrospect /org/focus/WindowIntrospect org.focus.WindowIntrospect GetActiveWindow
```

### Fix for GNOME 48 (Fedora 43)

Update your `metadata.json` to include GNOME 48:

```json
{
  "name": "Window Introspect",
  "description": "Exposes window focus information via D-Bus",
  "uuid": "window-introspect@focus.org",
  "shell-version": [
    "45",
    "46",
    "47",
    "48"
  ],
  "url": "https://github.com/rcman/linuxapps"
}
```

### Potential Code Issues for GNOME 48

Your extension code looks mostly good, but there’s one issue I spotted:

```javascript
// This connection doesn't actually track window destruction
this._windowDestroyedId = workspaceManager.connect('workspace-switched', () => {
    this._dbusImpl.emit_signal('WindowsChanged', null);
});
```

For proper window destruction tracking on modern GNOME:

```javascript
enable() {
    this._windowTracker = Shell.WindowTracker.get_default();

    this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(DBUS_INTERFACE, this);
    this._dbusImpl.export(Gio.DBus.session, '/org/focus/WindowIntrospect');

    const display = global.display;
    
    // Focus changes
    this._focusChangedId = display.connect('notify::focus-window', () => {
        this._onFocusChanged();
    });

    // Window creation
    this._windowCreatedId = display.connect('window-created', () => {
        this._dbusImpl.emit_signal('WindowsChanged', null);
    });

    // Window destruction - track via window-actors
    this._windowRemovedId = global.window_manager.connect('destroy', () => {
        this._dbusImpl.emit_signal('WindowsChanged', null);
    });

    console.log('Window Introspect extension enabled');
}

disable() {
    if (this._focusChangedId) {
        global.display.disconnect(this._focusChangedId);
        this._focusChangedId = null;
    }

    if (this._windowCreatedId) {
        global.display.disconnect(this._windowCreatedId);
        this._windowCreatedId = null;
    }

    if (this._windowRemovedId) {
        global.window_manager.disconnect(this._windowRemovedId);
        this._windowRemovedId = null;
    }

    if (this._dbusImpl) {
        this._dbusImpl.unexport();
        this._dbusImpl = null;
    }

    this._windowTracker = null;
    console.log('Window Introspect extension disabled');
}
```

### Installation Commands for Fedora 43

```bash
# Install to the correct location
mkdir -p ~/.local/share/gnome-shell/extensions/window-introspect@focus.org
cp extension.js metadata.json ~/.local/share/gnome-shell/extensions/window-introspect@focus.org/

# Enable the extension
gnome-extensions enable window-introspect@focus.org

# Restart GNOME Shell (on Wayland, you need to logout/login or reboot)
# On X11 you could do: killall -3 gnome-shell
```

### Testing

After installing/updating the extension:

```bash
# Verify it's running
busctl --user call org.focus.WindowIntrospect /org/focus/WindowIntrospect org.focus.WindowIntrospect GetActiveWindow

# Monitor focus changes
busctl --user monitor org.focus.WindowIntrospect
```

### Your C++ Client Code

Your C++ application should connect to this D-Bus service. If you can share that part of the code, I can help ensure it’s handling the connection properly and dealing with cases where the extension might not be loaded yet.

## Summary

The key is that on **Fedora 43 with GNOME 48**, you need to:

1. **Update metadata.json** to declare GNOME 48 support
1. **Ensure the extension is installed and enabled**
1. **Potentially fix the window destruction tracking**

The main issue is likely that your GNOME Shell extension isn’t declaring compatibility with GNOME 48, so it’s not being loaded. Once you update the metadata.json file and reinstall/enable the extension, your D-Bus service should be available again and your C++/SDL2 application should be able to connect to it.

## Additional Context from Research

### Fedora 43 Changes

- **GNOME X11 session has been removed** from Fedora Linux
- Users previously using GNOME on X11 are transparently upgraded to the GNOME Wayland session
- X11 applications are still supported on GNOME through XWayland

### SDL2 to SDL3 Migration (Fedora 42)

- Fedora 42 proposes replacing SDL2 with sdl2-compat
- Applications using SDL 2 will transparently use SDL 3 through the compatibility layer
- This should be automatic and users shouldn’t notice any difference

### IBus Integration

- Fedora 42 provides version 1.5.32 of the IBus internationalization library
- Support for Wayland input-method protocol version 2
- IBus now shows candidate popups for non-Wayland applications in Wayland sessions

## Repository Information

- **Repository:** https://github.com/rcman/linuxapps
- **Problematic Directory:** Wayland_Focus
- **Technologies:** C++, SDL2, GNOME Shell Extension, D-Bus
- **Languages in repo:** C++ (35.9%), Makefile (17.6%), C (17.5%), Java (11.9%), Python (8.8%), JavaScript (3.7%)