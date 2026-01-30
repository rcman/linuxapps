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
    _windowTitleChangedIds = new Map();  // Track title change handlers

    enable() {
        this._windowTracker = Shell.WindowTracker.get_default();

        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(DBUS_INTERFACE, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/focus/WindowIntrospect');

        const display = global.display;
        
        this._focusChangedId = display.connect('notify::focus-window', () => {
            this._onFocusChanged();
        });

        this._windowCreatedId = display.connect('window-created', (d, metaWindow) => {
            this._trackWindowTitle(metaWindow);
            this._dbusImpl.emit_signal('WindowsChanged', null);
        });

        const workspaceManager = global.workspace_manager;
        this._windowDestroyedId = workspaceManager.connect('workspace-switched', () => {
            this._dbusImpl.emit_signal('WindowsChanged', null);
        });

        // Track existing windows
        this._trackAllWindows();

        console.log('Window Introspect extension enabled');
    }

    disable() {
        // Disconnect all title change handlers
        this._untrackAllWindows();

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

    _trackAllWindows() {
        const windowActors = this._getWindowActors();
        for (const actor of windowActors) {
            const metaWindow = actor.get_meta_window();
            if (metaWindow) {
                this._trackWindowTitle(metaWindow);
            }
        }
    }

    _untrackAllWindows() {
        for (const [metaWindow, handlerId] of this._windowTitleChangedIds) {
            try {
                metaWindow.disconnect(handlerId);
            } catch (e) {
                // Window may already be destroyed
            }
        }
        this._windowTitleChangedIds.clear();
    }

    _trackWindowTitle(metaWindow) {
        // Don't double-track
        if (this._windowTitleChangedIds.has(metaWindow))
            return;

        const handlerId = metaWindow.connect('notify::title', () => {
            this._onTitleChanged(metaWindow);
        });

        this._windowTitleChangedIds.set(metaWindow, handlerId);

        // Clean up when window is destroyed
        metaWindow.connect('unmanaging', () => {
            this._untrackWindowTitle(metaWindow);
        });
    }

    _untrackWindowTitle(metaWindow) {
        const handlerId = this._windowTitleChangedIds.get(metaWindow);
        if (handlerId) {
            try {
                metaWindow.disconnect(handlerId);
            } catch (e) {
                // Ignore
            }
            this._windowTitleChangedIds.delete(metaWindow);
        }
    }

    _onTitleChanged(metaWindow) {
        // Emit WindowsChanged so the monitor app refreshes
        this._dbusImpl.emit_signal('WindowsChanged', null);

        // If this is the focused window, also emit FocusChanged
        if (metaWindow.has_focus()) {
            const windowInfo = this._getWindowInfo(metaWindow);
            this._dbusImpl.emit_signal('FocusChanged',
                new GLib.Variant('(s)', [JSON.stringify(windowInfo)]));
        }
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

    _getWindowActors() {
        if (global.compositor && typeof global.compositor.get_window_actors === 'function') {
            return global.compositor.get_window_actors();
        }
        if (typeof global.get_window_actors === 'function') {
            return global.get_window_actors();
        }
        console.warn('Window Introspect: Unable to get window actors');
        return [];
    }

    GetWindows() {
        const windows = [];
        const windowActors = this._getWindowActors();

        for (const actor of windowActors) {
            const metaWindow = actor.get_meta_window();
            if (!metaWindow)
                continue;

            const windowType = metaWindow.get_window_type();
            if (windowType === Meta.WindowType.DESKTOP ||
                windowType === Meta.WindowType.DOCK ||
                windowType === Meta.WindowType.SPLASHSCREEN)
                continue;

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
