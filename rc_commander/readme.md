# RC Commander v2.0

A dual-pane file manager inspired by the classic Norton Commander, built in Java Swing.

---

## Quick Start

```bash
javac RCCommander.java
java RCCommander
```

**Requirements:** Java 8 or higher

---

## Complete Feature List

### Function Keys

| Key | Function | Description |
|-----|----------|-------------|
| F1 | Help | Display help screen with keyboard reference |
| F2 | User Menu | Quick-launch menu for frequently used programs |
| F3 | View | Internal file viewer (read-only) |
| F4 | Edit | Internal text editor with save capability |
| F5 | Copy | Copy selected file(s) to opposite panel |
| F6 | Move/Rename | Move or rename selected file(s) |
| F7 | MkDir | Create new directory |
| F8 | Delete | Delete selected file(s) with confirmation |
| F9 | Pull-down Menu | Activate the menu bar |
| F10 | Quit | Exit program with confirmation |

### Navigation

| Key | Function |
|-----|----------|
| TAB | Switch between left and right panels |
| ENTER | Enter directory / Execute file / Open with default app |
| BACKSPACE | Go to parent directory |
| Ctrl+\ | Go to root directory |
| Up/Down Arrows | Navigate file list |
| Home/End | Jump to first/last file |
| Page Up/Down | Scroll file list |

### File Selection

| Key | Function |
|-----|----------|
| INSERT | Select/deselect current file and move to next |
| + (Plus) | Select files by wildcard mask (e.g., `*.txt`, `data*.*`) |
| - (Minus) | Deselect files by wildcard mask |
| * (Multiply) | Invert current selection |

### Panel Operations

| Key | Function |
|-----|----------|
| Ctrl+O | Toggle panels on/off (show command line only) |
| Ctrl+R | Refresh both panels |
| Ctrl+U | Swap left and right panels |
| Alt+F1 | Select drive/root for left panel |
| Alt+F2 | Select drive/root for right panel |

### Search & Find

| Key | Function |
|-----|----------|
| Alt+F7 | Find File dialog (search by name and content) |
| Alt+F8 | Command history |
| Alt+F10 | Directory tree browser |
| Quick Search | Just start typing to jump to matching filename |

### Internal Viewer (F3)

- View text files with syntax highlighting
- Keyboard shortcuts:
  - ESC, F3, F10 - Close viewer
- Scrollable content
- Monospace font display

### Internal Editor (F4)

- Edit text files directly
- Keyboard shortcuts:
  - F2 - Save file
  - ESC, F10 - Close editor
- Full text editing capabilities
- Preserves file encoding

### File Operations

| Operation | Description |
|-----------|-------------|
| Copy (F5) | Copy files/directories to destination with overwrite prompt |
| Move (F6) | Move files or rename single file |
| Delete (F8) | Delete with confirmation, supports directories |
| MkDir (F7) | Create new directory in active panel |
| Attributes | View file properties (size, date, permissions) |

### Directory Comparison

- Compare directories between panels
- Automatically selects files unique to each panel
- Shows count of unique files on each side

### View Modes

| Mode | Description |
|------|-------------|
| Brief | Compact view showing primarily filenames |
| Full | Complete view with name, extension, size, date, attributes |

### Sorting Options

Sort files by:
- Name (alphabetical)
- Extension (file type)
- Size (bytes)
- Date (modification time)

Directories always appear first, sorted separately.

### Color Themes

| Theme | Description |
|-------|-------------|
| Classic DOS | Blue panels, cyan text (authentic NC look) |
| Midnight Commander | Blue/cyan scheme matching MC |
| Modern Dark | Contemporary dark theme |
| Matrix | Green-on-black hacker style |

### Display Features

- Directories shown in UPPERCASE (classic NC style)
- Files shown in lowercase
- Color-coded file types:
  - Directories: Yellow/White
  - Executables (.exe, .com, .bat, .cmd, .sh, .jar): Green
  - Regular files: Cyan/Gray
  - Selected files: Yellow highlight
- File size formatting (B, K, M, G)
- Date format: MM-dd-yy HH:mm

### Command Line

- DOS-style command prompt at bottom of screen
- Execute system commands in current directory
- Press ENTER to execute
- Press ESC to clear and return to file panel

### User Menu (F2)

Customizable quick-launch menu including:
- Text Editor
- Calculator
- Command Prompt
- System Info
- Extensible for additional programs

### Menu Bar

| Menu | Contents |
|------|----------|
| Left | View modes, sorting options for left panel |
| Files | All file operations (F3-F8 functions) |
| Commands | Find, history, tree, swap, compare |
| Options | Color schemes, configuration |
| Right | View modes, sorting options for right panel |

### Supported File Attributes

- Read-only (r)
- Hidden (h)
- Directory (d)
- Executable detection

### Wildcard Support

Pattern matching for file selection:
- `*` - Match any characters
- `?` - Match single character
- Examples: `*.txt`, `data*.csv`, `image?.png`

### Additional Features

- Confirmation dialogs for destructive operations
- Status bar with context-sensitive information
- File count and total size display per panel
- Selection count and size summary
- Window title shows current directory
- Recursive directory copy and delete
- Cross-platform compatibility (Windows, macOS, Linux)

---

## Keyboard Quick Reference

```
┌─────────────────────────────────────────────────────────────┐
│ F1 Help    F2 Menu    F3 View    F4 Edit    F5 Copy        │
│ F6 Move    F7 MkDir   F8 Delete  F9 PullDn  F10 Quit       │
├─────────────────────────────────────────────────────────────┤
│ TAB      - Switch panels      INSERT  - Select file        │
│ ENTER    - Enter/Execute      +       - Select by mask     │
│ BKSP     - Parent directory   -       - Deselect by mask   │
│ Ctrl+\   - Root directory     *       - Invert selection   │
│ Ctrl+O   - Toggle panels      Ctrl+R  - Refresh            │
│ Ctrl+U   - Swap panels        Alt+F7  - Find file          │
└─────────────────────────────────────────────────────────────┘
```

---

## License

Free to use and modify.

---

## Acknowledgments

Inspired by Norton Commander (1986) by John Socha, published by Symantec.
