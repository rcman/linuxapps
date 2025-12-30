import javax.swing.*;
import javax.swing.border.Border;
import javax.swing.border.TitledBorder;
import javax.swing.table.DefaultTableCellRenderer;
import javax.swing.table.DefaultTableModel;
import javax.swing.text.DefaultCaret;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.nio.file.*;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.List;
import java.util.regex.Pattern;

/**
 * Norton Commander Clone - Authentic DOS-era dual-pane file manager
 * 
 * Function Keys (matching original Norton Commander):
 * F1  - Help
 * F2  - User Menu
 * F3  - View file (internal viewer)
 * F4  - Edit file (internal editor)
 * F5  - Copy
 * F6  - Move/Rename
 * F7  - Make Directory
 * F8  - Delete
 * F9  - Pull-down Menu
 * F10 - Quit
 * 
 * Additional shortcuts:
 * TAB        - Switch panels
 * INSERT     - Select/deselect file
 * Ctrl+O     - Toggle panels visibility
 * Alt+F1/F2  - Drive selection
 * Ctrl+R     - Refresh
 * Ctrl+\     - Go to root
 * Backspace  - Parent directory
 * Enter      - Enter directory / Execute / View
 * +          - Select by mask
 * -          - Deselect by mask
 * *          - Invert selection
 */
public class RCCommander extends JFrame {
    private static final String[] COLUMN_NAMES = {"Name", "Ext", "Size", "Date", "Attr"};
    private static final String VERSION = "RC Commander v2.0";
    
    // DOS-authentic color themes
    private static final Map<String, Theme> THEMES = new HashMap<>();
    static {
        // Classic NC blue panels with cyan text
        THEMES.put("Classic DOS", new Theme(
            new Color(0, 0, 170),      // Panel background (DOS blue)
            new Color(0, 255, 255),    // Normal text (cyan)
            new Color(0, 0, 128),      // Header background
            new Color(255, 255, 255),  // Header text (white)
            new Color(0, 170, 170),    // Selection background (dark cyan)
            new Color(255, 255, 0),    // Directory color (yellow)
            new Color(0, 170, 0),      // Executable color (green)
            new Color(170, 170, 170),  // Hidden files (gray)
            new Color(255, 255, 255),  // Selected text
            new Color(0, 0, 0),        // Status bar background
            new Color(0, 255, 255)     // Status bar text
        ));
        
        THEMES.put("Modern Dark", new Theme(
            new Color(30, 30, 40),
            new Color(220, 220, 220),
            new Color(45, 45, 55),
            new Color(255, 255, 255),
            new Color(60, 80, 120),
            new Color(100, 180, 255),
            new Color(100, 220, 100),
            new Color(128, 128, 128),
            new Color(255, 255, 255),
            new Color(20, 20, 25),
            new Color(200, 200, 200)
        ));
        
        THEMES.put("Midnight Commander", new Theme(
            new Color(0, 0, 128),
            new Color(192, 192, 192),
            new Color(0, 128, 128),
            new Color(0, 0, 0),
            new Color(128, 128, 128),
            new Color(255, 255, 255),
            new Color(0, 255, 0),
            new Color(100, 100, 100),
            new Color(0, 0, 0),
            new Color(0, 128, 128),
            new Color(0, 0, 0)
        ));
        
        THEMES.put("Matrix", new Theme(
            new Color(0, 10, 0),
            new Color(0, 255, 0),
            new Color(0, 30, 0),
            new Color(0, 200, 0),
            new Color(0, 60, 0),
            new Color(100, 255, 100),
            new Color(0, 200, 0),
            new Color(0, 100, 0),
            new Color(200, 255, 200),
            new Color(0, 20, 0),
            new Color(0, 180, 0)
        ));
    }
    
    private Theme currentTheme = THEMES.get("Classic DOS");
    
    // UI Components
    private JTable leftTable, rightTable;
    private DefaultTableModel leftModel, rightModel;
    private JLabel leftPathLabel, rightPathLabel;
    private JLabel leftInfoLabel, rightInfoLabel;
    private JLabel statusLabel;
    private JTextField commandLine;
    private JPanel leftPanel, rightPanel;
    private JPanel mainPanel;
    private JPanel functionKeyPanel;
    private JMenuBar menuBar;
    
    // State
    private JTable activeTable;
    private File leftCurrentDir, rightCurrentDir;
    private Set<String> leftSelected = new HashSet<>();
    private Set<String> rightSelected = new HashSet<>();
    private boolean panelsVisible = true;
    private StringBuilder quickSearchBuffer = new StringBuilder();
    private long lastKeyTime = 0;
    
    // User menu items
    private List<UserMenuItem> userMenu = new ArrayList<>();
    
    static class Theme {
        Color background, foreground, headerBg, headerFg;
        Color selection, directory, executable, hidden;
        Color selectedText, statusBg, statusFg;
        
        Theme(Color bg, Color fg, Color hBg, Color hFg, Color sel, Color dir, 
              Color exec, Color hid, Color selTxt, Color stBg, Color stFg) {
            this.background = bg;
            this.foreground = fg;
            this.headerBg = hBg;
            this.headerFg = hFg;
            this.selection = sel;
            this.directory = dir;
            this.executable = exec;
            this.hidden = hid;
            this.selectedText = selTxt;
            this.statusBg = stBg;
            this.statusFg = stFg;
        }
    }
    
    static class UserMenuItem {
        String name;
        String command;
        char hotkey;
        
        UserMenuItem(String name, String command, char hotkey) {
            this.name = name;
            this.command = command;
            this.hotkey = hotkey;
        }
    }
    
    static class FileEntry {
        String name;
        String ext;
        long size;
        long modified;
        boolean isDirectory;
        boolean isHidden;
        boolean isExecutable;
        
        FileEntry(File file) {
            String fullName = file.getName();
            int dotIndex = fullName.lastIndexOf('.');
            if (dotIndex > 0 && !file.isDirectory()) {
                this.name = fullName.substring(0, dotIndex);
                this.ext = fullName.substring(dotIndex + 1);
            } else {
                this.name = fullName;
                this.ext = "";
            }
            this.size = file.length();
            this.modified = file.lastModified();
            this.isDirectory = file.isDirectory();
            this.isHidden = file.isHidden();
            this.isExecutable = file.canExecute() && !file.isDirectory();
        }
    }
    
    public RCCommander() {
        initializeUserMenu();
        initializeUI();
        setupKeyBindings();
        
        // Initialize with home directory
        String home = System.getProperty("user.home");
        leftCurrentDir = new File(home);
        rightCurrentDir = new File(home);
        
        loadDirectory(leftCurrentDir, leftModel, leftSelected);
        loadDirectory(rightCurrentDir, rightModel, rightSelected);
        updatePathLabels();
        updateInfoLabels();
        
        activeTable = leftTable;
        updateActivePanel();
        applyTheme();
        
        // Select first item
        if (leftTable.getRowCount() > 0) {
            leftTable.setRowSelectionInterval(0, 0);
        }
    }
    
    private void initializeUserMenu() {
        // Default user menu items (like NC's F2 menu)
        userMenu.add(new UserMenuItem("File Manager", "", 'F'));
        userMenu.add(new UserMenuItem("Text Editor", "notepad", 'E'));
        userMenu.add(new UserMenuItem("Calculator", "calc", 'C'));
        userMenu.add(new UserMenuItem("Command Prompt", "cmd", 'P'));
        userMenu.add(new UserMenuItem("System Info", "systeminfo", 'S'));
    }
    
    private void initializeUI() {
        setTitle(VERSION);
        setDefaultCloseOperation(JFrame.DO_NOTHING_ON_CLOSE);
        addWindowListener(new WindowAdapter() {
            @Override
            public void windowClosing(WindowEvent e) {
                confirmExit();
            }
        });
        
        setLayout(new BorderLayout(0, 0));
        
        // Create menu bar
        createMenuBar();
        
        // Main panel with two file panels
        mainPanel = new JPanel(new GridLayout(1, 2, 2, 0));
        mainPanel.setBackground(Color.BLACK);
        
        // Left panel
        leftPanel = createFilePanel();
        leftModel = createTableModel();
        leftTable = createFileTable(leftModel, true);
        leftPathLabel = new JLabel();
        leftInfoLabel = new JLabel();
        setupFilePanel(leftPanel, leftTable, leftPathLabel, leftInfoLabel);
        
        // Right panel  
        rightPanel = createFilePanel();
        rightModel = createTableModel();
        rightTable = createFileTable(rightModel, false);
        rightPathLabel = new JLabel();
        rightInfoLabel = new JLabel();
        setupFilePanel(rightPanel, rightTable, rightPathLabel, rightInfoLabel);
        
        mainPanel.add(leftPanel);
        mainPanel.add(rightPanel);
        
        // Command line (DOS prompt style)
        JPanel commandPanel = new JPanel(new BorderLayout());
        JLabel promptLabel = new JLabel(">");
        promptLabel.setFont(new Font(Font.MONOSPACED, Font.BOLD, 12));
        commandLine = new JTextField();
        commandLine.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 12));
        commandLine.addActionListener(e -> executeCommand());
        commandPanel.add(promptLabel, BorderLayout.WEST);
        commandPanel.add(commandLine, BorderLayout.CENTER);
        
        // Status bar
        statusLabel = new JLabel(" " + VERSION + " - Press F1 for Help");
        statusLabel.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 11));
        
        // Function key panel (authentic NC style)
        functionKeyPanel = createFunctionKeyPanel();
        
        // Bottom panel combining command line, status, and function keys
        JPanel bottomPanel = new JPanel(new BorderLayout(0, 0));
        bottomPanel.add(commandPanel, BorderLayout.NORTH);
        bottomPanel.add(functionKeyPanel, BorderLayout.CENTER);
        bottomPanel.add(statusLabel, BorderLayout.SOUTH);
        
        add(mainPanel, BorderLayout.CENTER);
        add(bottomPanel, BorderLayout.SOUTH);
        
        setSize(1024, 768);
        setLocationRelativeTo(null);
        
        // Use monospace font throughout for authentic look
        setUIFont(new Font(Font.MONOSPACED, Font.PLAIN, 12));
    }
    
    private void setUIFont(Font font) {
        UIManager.put("Table.font", font);
        UIManager.put("TableHeader.font", font);
        UIManager.put("Label.font", font);
        UIManager.put("TextField.font", font);
        UIManager.put("Button.font", new Font(Font.MONOSPACED, Font.PLAIN, 10));
    }
    
    private void createMenuBar() {
        menuBar = new JMenuBar();
        
        // Left menu
        JMenu leftMenu = new JMenu("Left");
        leftMenu.setMnemonic('L');
        leftMenu.add(createMenuItem("Brief", KeyEvent.VK_B, e -> setBriefView(leftTable)));
        leftMenu.add(createMenuItem("Full", KeyEvent.VK_F, e -> setFullView(leftTable)));
        leftMenu.addSeparator();
        leftMenu.add(createMenuItem("Sort by Name", 0, e -> sortBy(leftModel, 0)));
        leftMenu.add(createMenuItem("Sort by Extension", 0, e -> sortBy(leftModel, 1)));
        leftMenu.add(createMenuItem("Sort by Size", 0, e -> sortBy(leftModel, 2)));
        leftMenu.add(createMenuItem("Sort by Date", 0, e -> sortBy(leftModel, 3)));
        leftMenu.addSeparator();
        leftMenu.add(createMenuItem("Refresh (Ctrl+R)", 0, e -> refresh()));
        
        // Files menu
        JMenu filesMenu = new JMenu("Files");
        filesMenu.setMnemonic('F');
        filesMenu.add(createMenuItem("View (F3)", KeyEvent.VK_F3, e -> viewFile()));
        filesMenu.add(createMenuItem("Edit (F4)", KeyEvent.VK_F4, e -> editFile()));
        filesMenu.add(createMenuItem("Copy (F5)", KeyEvent.VK_F5, e -> copyFiles()));
        filesMenu.add(createMenuItem("Move/Rename (F6)", KeyEvent.VK_F6, e -> moveFiles()));
        filesMenu.add(createMenuItem("Make Directory (F7)", KeyEvent.VK_F7, e -> createNewFolder()));
        filesMenu.add(createMenuItem("Delete (F8)", KeyEvent.VK_F8, e -> deleteFiles()));
        filesMenu.addSeparator();
        filesMenu.add(createMenuItem("File Attributes", 0, e -> showAttributes()));
        filesMenu.add(createMenuItem("Select Group (+)", 0, e -> selectByMask()));
        filesMenu.add(createMenuItem("Deselect Group (-)", 0, e -> deselectByMask()));
        filesMenu.add(createMenuItem("Invert Selection (*)", 0, e -> invertSelection()));
        
        // Commands menu
        JMenu commandsMenu = new JMenu("Commands");
        commandsMenu.setMnemonic('C');
        commandsMenu.add(createMenuItem("Find File (Alt+F7)", 0, e -> findFile()));
        commandsMenu.add(createMenuItem("History (Alt+F8)", 0, e -> showHistory()));
        commandsMenu.add(createMenuItem("Directory Tree (Alt+F10)", 0, e -> showDirectoryTree()));
        commandsMenu.addSeparator();
        commandsMenu.add(createMenuItem("Swap Panels (Ctrl+U)", 0, e -> swapPanels()));
        commandsMenu.add(createMenuItem("Panels On/Off (Ctrl+O)", 0, e -> togglePanels()));
        commandsMenu.add(createMenuItem("Compare Directories", 0, e -> compareDirectories()));
        
        // Options menu
        JMenu optionsMenu = new JMenu("Options");
        optionsMenu.setMnemonic('O');
        JMenu themeMenu = new JMenu("Color Scheme");
        for (String themeName : THEMES.keySet()) {
            JMenuItem item = new JMenuItem(themeName);
            item.addActionListener(e -> switchTheme(themeName));
            themeMenu.add(item);
        }
        optionsMenu.add(themeMenu);
        optionsMenu.add(createMenuItem("Configuration...", 0, e -> showConfiguration()));
        
        // Right menu
        JMenu rightMenu = new JMenu("Right");
        rightMenu.setMnemonic('R');
        rightMenu.add(createMenuItem("Brief", KeyEvent.VK_B, e -> setBriefView(rightTable)));
        rightMenu.add(createMenuItem("Full", KeyEvent.VK_F, e -> setFullView(rightTable)));
        rightMenu.addSeparator();
        rightMenu.add(createMenuItem("Sort by Name", 0, e -> sortBy(rightModel, 0)));
        rightMenu.add(createMenuItem("Sort by Extension", 0, e -> sortBy(rightModel, 1)));
        rightMenu.add(createMenuItem("Sort by Size", 0, e -> sortBy(rightModel, 2)));
        rightMenu.add(createMenuItem("Sort by Date", 0, e -> sortBy(rightModel, 3)));
        
        menuBar.add(leftMenu);
        menuBar.add(filesMenu);
        menuBar.add(commandsMenu);
        menuBar.add(optionsMenu);
        menuBar.add(rightMenu);
        
        setJMenuBar(menuBar);
    }
    
    private JMenuItem createMenuItem(String text, int keyCode, ActionListener action) {
        JMenuItem item = new JMenuItem(text);
        if (keyCode != 0) {
            item.setAccelerator(KeyStroke.getKeyStroke(keyCode, 0));
        }
        item.addActionListener(action);
        return item;
    }
    
    private DefaultTableModel createTableModel() {
        return new DefaultTableModel(COLUMN_NAMES, 0) {
            @Override
            public boolean isCellEditable(int row, int column) {
                return false;
            }
        };
    }
    
    private JPanel createFilePanel() {
        JPanel panel = new JPanel(new BorderLayout(0, 0));
        return panel;
    }
    
    private JTable createFileTable(DefaultTableModel model, boolean isLeft) {
        JTable table = new JTable(model);
        table.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
        table.setRowSelectionAllowed(true);
        table.setColumnSelectionAllowed(false);
        table.getTableHeader().setReorderingAllowed(false);
        table.setShowGrid(false);
        table.setIntercellSpacing(new Dimension(0, 0));
        table.setRowHeight(16);
        table.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 12));
        
        // Column widths (NC style: name dominant)
        table.getColumnModel().getColumn(0).setPreferredWidth(150); // Name
        table.getColumnModel().getColumn(1).setPreferredWidth(40);  // Ext
        table.getColumnModel().getColumn(2).setPreferredWidth(70);  // Size
        table.getColumnModel().getColumn(3).setPreferredWidth(80);  // Date
        table.getColumnModel().getColumn(4).setPreferredWidth(40);  // Attr
        
        // Custom renderer
        table.setDefaultRenderer(Object.class, new NCTableCellRenderer(isLeft));
        
        // Double-click handler
        table.addMouseListener(new MouseAdapter() {
            @Override
            public void mouseClicked(MouseEvent e) {
                if (e.getClickCount() == 2) {
                    enterDirectory();
                }
            }
        });
        
        // Focus tracking
        table.addFocusListener(new FocusAdapter() {
            @Override
            public void focusGained(FocusEvent e) {
                activeTable = table;
                updateActivePanel();
            }
        });
        
        // Key listener for quick search
        table.addKeyListener(new KeyAdapter() {
            @Override
            public void keyTyped(KeyEvent e) {
                char c = e.getKeyChar();
                if (Character.isLetterOrDigit(c) || c == '.' || c == '_' || c == '-') {
                    handleQuickSearch(c);
                }
            }
        });
        
        return table;
    }
    
    private void setupFilePanel(JPanel panel, JTable table, JLabel pathLabel, JLabel infoLabel) {
        // Path label at top
        pathLabel.setOpaque(true);
        pathLabel.setHorizontalAlignment(SwingConstants.CENTER);
        pathLabel.setBorder(BorderFactory.createEmptyBorder(2, 5, 2, 5));
        
        // Scroll pane for table
        JScrollPane scrollPane = new JScrollPane(table);
        scrollPane.setVerticalScrollBarPolicy(JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED);
        scrollPane.setHorizontalScrollBarPolicy(JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
        scrollPane.setBorder(null);
        
        // Info label at bottom (shows selection info)
        infoLabel.setOpaque(true);
        infoLabel.setHorizontalAlignment(SwingConstants.CENTER);
        infoLabel.setBorder(BorderFactory.createEmptyBorder(2, 5, 2, 5));
        
        panel.add(pathLabel, BorderLayout.NORTH);
        panel.add(scrollPane, BorderLayout.CENTER);
        panel.add(infoLabel, BorderLayout.SOUTH);
    }
    
    private JPanel createFunctionKeyPanel() {
        JPanel panel = new JPanel(new GridLayout(1, 10, 1, 0));
        panel.setBorder(BorderFactory.createEmptyBorder(2, 0, 2, 0));
        
        // Authentic NC function key labels
        String[] labels = {
            "1Help", "2Menu", "3View", "4Edit", "5Copy",
            "6RenMov", "7MkDir", "8Delete", "9PullDn", "10Quit"
        };
        
        ActionListener[] actions = {
            e -> showHelp(),
            e -> showUserMenu(),
            e -> viewFile(),
            e -> editFile(),
            e -> copyFiles(),
            e -> moveFiles(),
            e -> createNewFolder(),
            e -> deleteFiles(),
            e -> showPulldownMenu(),
            e -> confirmExit()
        };
        
        for (int i = 0; i < 10; i++) {
            JButton btn = new JButton(labels[i]);
            btn.setFont(new Font(Font.MONOSPACED, Font.BOLD, 11));
            btn.setMargin(new Insets(1, 2, 1, 2));
            btn.setFocusable(false);
            btn.addActionListener(actions[i]);
            panel.add(btn);
        }
        
        return panel;
    }
    
    private void setupKeyBindings() {
        JRootPane root = getRootPane();
        InputMap im = root.getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW);
        ActionMap am = root.getActionMap();
        
        // Function keys F1-F10
        bindKey(im, am, KeyEvent.VK_F1, 0, "help", e -> showHelp());
        bindKey(im, am, KeyEvent.VK_F2, 0, "usermenu", e -> showUserMenu());
        bindKey(im, am, KeyEvent.VK_F3, 0, "view", e -> viewFile());
        bindKey(im, am, KeyEvent.VK_F4, 0, "edit", e -> editFile());
        bindKey(im, am, KeyEvent.VK_F5, 0, "copy", e -> copyFiles());
        bindKey(im, am, KeyEvent.VK_F6, 0, "move", e -> moveFiles());
        bindKey(im, am, KeyEvent.VK_F7, 0, "mkdir", e -> createNewFolder());
        bindKey(im, am, KeyEvent.VK_F8, 0, "delete", e -> deleteFiles());
        bindKey(im, am, KeyEvent.VK_F9, 0, "pulldown", e -> showPulldownMenu());
        bindKey(im, am, KeyEvent.VK_F10, 0, "quit", e -> confirmExit());
        
        // Navigation
        bindKey(im, am, KeyEvent.VK_TAB, 0, "switch", e -> switchPanel());
        bindKey(im, am, KeyEvent.VK_ENTER, 0, "enter", e -> enterDirectory());
        bindKey(im, am, KeyEvent.VK_BACK_SPACE, 0, "parent", e -> goToParent());
        
        // Selection
        bindKey(im, am, KeyEvent.VK_INSERT, 0, "select", e -> toggleSelection());
        bindKey(im, am, KeyEvent.VK_ADD, 0, "selectmask", e -> selectByMask());
        bindKey(im, am, KeyEvent.VK_SUBTRACT, 0, "deselectmask", e -> deselectByMask());
        bindKey(im, am, KeyEvent.VK_MULTIPLY, 0, "invert", e -> invertSelection());
        
        // Ctrl combinations
        bindKey(im, am, KeyEvent.VK_O, InputEvent.CTRL_DOWN_MASK, "toggle", e -> togglePanels());
        bindKey(im, am, KeyEvent.VK_R, InputEvent.CTRL_DOWN_MASK, "refresh", e -> refresh());
        bindKey(im, am, KeyEvent.VK_U, InputEvent.CTRL_DOWN_MASK, "swap", e -> swapPanels());
        bindKey(im, am, KeyEvent.VK_BACK_SLASH, InputEvent.CTRL_DOWN_MASK, "root", e -> goToRoot());
        
        // Alt+F combinations
        bindKey(im, am, KeyEvent.VK_F1, InputEvent.ALT_DOWN_MASK, "leftdrive", e -> selectDrive(true));
        bindKey(im, am, KeyEvent.VK_F2, InputEvent.ALT_DOWN_MASK, "rightdrive", e -> selectDrive(false));
        bindKey(im, am, KeyEvent.VK_F7, InputEvent.ALT_DOWN_MASK, "find", e -> findFile());
        bindKey(im, am, KeyEvent.VK_F8, InputEvent.ALT_DOWN_MASK, "history", e -> showHistory());
        bindKey(im, am, KeyEvent.VK_F10, InputEvent.ALT_DOWN_MASK, "tree", e -> showDirectoryTree());
        
        // Escape to clear command line
        bindKey(im, am, KeyEvent.VK_ESCAPE, 0, "escape", e -> {
            commandLine.setText("");
            activeTable.requestFocus();
        });
    }
    
    private void bindKey(InputMap im, ActionMap am, int keyCode, int modifiers, 
                        String name, ActionListener action) {
        KeyStroke ks = KeyStroke.getKeyStroke(keyCode, modifiers);
        im.put(ks, name);
        am.put(name, new AbstractAction() {
            @Override
            public void actionPerformed(ActionEvent e) {
                action.actionPerformed(e);
            }
        });
    }
    
    // ==================== File Operations ====================
    
    private void loadDirectory(File directory, DefaultTableModel model, Set<String> selected) {
        model.setRowCount(0);
        selected.clear();
        
        if (directory == null || !directory.exists() || !directory.isDirectory()) {
            return;
        }
        
        // Add parent directory entry
        if (directory.getParentFile() != null) {
            model.addRow(new Object[]{"..", "", "<UP-DIR>", "", ""});
        }
        
        File[] files = directory.listFiles();
        if (files == null) return;
        
        // Sort: directories first, then by name
        Arrays.sort(files, (a, b) -> {
            if (a.isDirectory() && !b.isDirectory()) return -1;
            if (!a.isDirectory() && b.isDirectory()) return 1;
            return a.getName().compareToIgnoreCase(b.getName());
        });
        
        SimpleDateFormat sdf = new SimpleDateFormat("MM-dd-yy HH:mm");
        
        for (File file : files) {
            FileEntry entry = new FileEntry(file);
            
            String displayName = entry.isDirectory ? 
                entry.name.toUpperCase() : entry.name.toLowerCase();
            String size = entry.isDirectory ? "<DIR>" : formatFileSize(entry.size);
            String date = sdf.format(new Date(entry.modified));
            String attr = getFileAttributes(file);
            
            model.addRow(new Object[]{
                displayName, 
                entry.ext.toLowerCase(), 
                size, 
                date, 
                attr
            });
        }
    }
    
    private String formatFileSize(long size) {
        if (size < 1024) return String.valueOf(size);
        if (size < 1024 * 1024) return (size / 1024) + "K";
        if (size < 1024L * 1024 * 1024) return (size / (1024 * 1024)) + "M";
        return (size / (1024L * 1024 * 1024)) + "G";
    }
    
    private String getFileAttributes(File file) {
        StringBuilder attr = new StringBuilder();
        if (!file.canWrite()) attr.append("r");
        if (file.isHidden()) attr.append("h");
        if (file.isDirectory()) attr.append("d");
        return attr.toString();
    }
    
    private void updatePathLabels() {
        leftPathLabel.setText(truncatePath(leftCurrentDir.getAbsolutePath(), 40));
        rightPathLabel.setText(truncatePath(rightCurrentDir.getAbsolutePath(), 40));
        
        // Update command line prompt
        File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        // Update window title with current directory
        setTitle(VERSION + " - " + currentDir.getAbsolutePath());
    }
    
    private String truncatePath(String path, int maxLen) {
        if (path.length() <= maxLen) return path;
        return "..." + path.substring(path.length() - maxLen + 3);
    }
    
    private void updateInfoLabels() {
        updateInfoLabel(leftInfoLabel, leftModel, leftSelected, leftCurrentDir);
        updateInfoLabel(rightInfoLabel, rightModel, rightSelected, rightCurrentDir);
    }
    
    private void updateInfoLabel(JLabel label, DefaultTableModel model, Set<String> selected, File dir) {
        int fileCount = 0;
        int dirCount = 0;
        long totalSize = 0;
        long selectedSize = 0;
        int selectedCount = selected.size();
        
        for (int i = 0; i < model.getRowCount(); i++) {
            String name = (String) model.getValueAt(i, 0);
            String size = (String) model.getValueAt(i, 2);
            
            if ("..".equals(name)) continue;
            
            if ("<DIR>".equals(size)) {
                dirCount++;
            } else {
                fileCount++;
                try {
                    File f = new File(dir, name);
                    totalSize += f.length();
                    if (selected.contains(name)) {
                        selectedSize += f.length();
                    }
                } catch (Exception e) {}
            }
        }
        
        if (selectedCount > 0) {
            label.setText(String.format("%d of %d files, %s of %s", 
                selectedCount, fileCount, formatFileSize(selectedSize), formatFileSize(totalSize)));
        } else {
            label.setText(String.format("%d files, %d dirs, %s bytes", 
                fileCount, dirCount, formatFileSize(totalSize)));
        }
    }
    
    private void updateActivePanel() {
        Border activeBorder = BorderFactory.createLineBorder(currentTheme.headerFg, 2);
        Border inactiveBorder = BorderFactory.createLineBorder(currentTheme.headerBg, 1);
        
        leftPanel.setBorder(activeTable == leftTable ? activeBorder : inactiveBorder);
        rightPanel.setBorder(activeTable == rightTable ? activeBorder : inactiveBorder);
        
        // Update path labels to show active state
        leftPathLabel.setBackground(activeTable == leftTable ? currentTheme.headerBg : currentTheme.background);
        rightPathLabel.setBackground(activeTable == rightTable ? currentTheme.headerBg : currentTheme.background);
    }
    
    private void switchPanel() {
        if (activeTable == leftTable) {
            rightTable.requestFocus();
            activeTable = rightTable;
        } else {
            leftTable.requestFocus();
            activeTable = leftTable;
        }
        updateActivePanel();
        updatePathLabels();
    }
    
    private void enterDirectory() {
        int row = activeTable.getSelectedRow();
        if (row == -1) return;
        
        String name = (String) activeTable.getValueAt(row, 0);
        File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        
        if ("..".equals(name)) {
            goToParent();
            return;
        }
        
        // Handle directories in uppercase
        File target = findFileIgnoreCase(currentDir, name);
        if (target == null) return;
        
        if (target.isDirectory()) {
            changeDirectory(target);
        } else {
            // Try to open/execute file
            String ext = getExtension(target.getName()).toLowerCase();
            if (isExecutable(ext)) {
                executeFile(target);
            } else {
                viewFile();
            }
        }
    }
    
    private File findFileIgnoreCase(File dir, String name) {
        File[] files = dir.listFiles();
        if (files == null) return null;
        
        for (File f : files) {
            if (f.getName().equalsIgnoreCase(name)) {
                return f;
            }
        }
        return new File(dir, name);
    }
    
    private String getExtension(String filename) {
        int dot = filename.lastIndexOf('.');
        return (dot > 0) ? filename.substring(dot + 1) : "";
    }
    
    private boolean isExecutable(String ext) {
        return ext.equals("exe") || ext.equals("com") || ext.equals("bat") || 
               ext.equals("cmd") || ext.equals("sh") || ext.equals("jar");
    }
    
    private void changeDirectory(File newDir) {
        if (activeTable == leftTable) {
            leftCurrentDir = newDir;
            loadDirectory(newDir, leftModel, leftSelected);
        } else {
            rightCurrentDir = newDir;
            loadDirectory(newDir, rightModel, rightSelected);
        }
        updatePathLabels();
        updateInfoLabels();
        
        if (activeTable.getRowCount() > 0) {
            activeTable.setRowSelectionInterval(0, 0);
        }
        
        statusLabel.setText(" " + newDir.getAbsolutePath());
    }
    
    private void goToParent() {
        File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        File parent = currentDir.getParentFile();
        if (parent != null) {
            changeDirectory(parent);
        }
    }
    
    private void goToRoot() {
        File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        File root = currentDir;
        while (root.getParentFile() != null) {
            root = root.getParentFile();
        }
        changeDirectory(root);
    }
    
    // ==================== Selection Operations ====================
    
    private void toggleSelection() {
        int row = activeTable.getSelectedRow();
        if (row == -1) return;
        
        String name = (String) activeTable.getValueAt(row, 0);
        if ("..".equals(name)) {
            // Move to next row
            if (row + 1 < activeTable.getRowCount()) {
                activeTable.setRowSelectionInterval(row + 1, row + 1);
            }
            return;
        }
        
        Set<String> selected = (activeTable == leftTable) ? leftSelected : rightSelected;
        
        if (selected.contains(name)) {
            selected.remove(name);
        } else {
            selected.add(name);
        }
        
        // Move to next row
        if (row + 1 < activeTable.getRowCount()) {
            activeTable.setRowSelectionInterval(row + 1, row + 1);
        }
        
        activeTable.repaint();
        updateInfoLabels();
    }
    
    private void selectByMask() {
        String mask = JOptionPane.showInputDialog(this, "Select files matching:", "*.*");
        if (mask == null || mask.isEmpty()) return;
        
        selectByPattern(mask, true);
    }
    
    private void deselectByMask() {
        String mask = JOptionPane.showInputDialog(this, "Deselect files matching:", "*.*");
        if (mask == null || mask.isEmpty()) return;
        
        selectByPattern(mask, false);
    }
    
    private void selectByPattern(String mask, boolean select) {
        // Convert DOS wildcard to regex
        String regex = mask.replace(".", "\\.").replace("*", ".*").replace("?", ".");
        Pattern pattern = Pattern.compile(regex, Pattern.CASE_INSENSITIVE);
        
        DefaultTableModel model = (activeTable == leftTable) ? leftModel : rightModel;
        Set<String> selected = (activeTable == leftTable) ? leftSelected : rightSelected;
        
        for (int i = 0; i < model.getRowCount(); i++) {
            String name = (String) model.getValueAt(i, 0);
            String ext = (String) model.getValueAt(i, 1);
            String fullName = ext.isEmpty() ? name : name + "." + ext;
            
            if ("..".equals(name)) continue;
            if ("<DIR>".equals(model.getValueAt(i, 2))) continue;
            
            if (pattern.matcher(fullName).matches()) {
                if (select) {
                    selected.add(name);
                } else {
                    selected.remove(name);
                }
            }
        }
        
        activeTable.repaint();
        updateInfoLabels();
    }
    
    private void invertSelection() {
        DefaultTableModel model = (activeTable == leftTable) ? leftModel : rightModel;
        Set<String> selected = (activeTable == leftTable) ? leftSelected : rightSelected;
        
        for (int i = 0; i < model.getRowCount(); i++) {
            String name = (String) model.getValueAt(i, 0);
            if ("..".equals(name)) continue;
            if ("<DIR>".equals(model.getValueAt(i, 2))) continue;
            
            if (selected.contains(name)) {
                selected.remove(name);
            } else {
                selected.add(name);
            }
        }
        
        activeTable.repaint();
        updateInfoLabels();
    }
    
    private List<File> getSelectedFiles() {
        List<File> files = new ArrayList<>();
        File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        Set<String> selected = (activeTable == leftTable) ? leftSelected : rightSelected;
        DefaultTableModel model = (activeTable == leftTable) ? leftModel : rightModel;
        
        if (selected.isEmpty()) {
            // If nothing selected, use current row
            int row = activeTable.getSelectedRow();
            if (row >= 0) {
                String name = (String) model.getValueAt(row, 0);
                if (!"..".equals(name)) {
                    File f = findFileIgnoreCase(currentDir, name);
                    if (f != null) files.add(f);
                }
            }
        } else {
            for (String name : selected) {
                File f = findFileIgnoreCase(currentDir, name);
                if (f != null) files.add(f);
            }
        }
        
        return files;
    }
    
    // ==================== File Management Functions ====================
    
    private void copyFiles() {
        List<File> sources = getSelectedFiles();
        if (sources.isEmpty()) {
            statusLabel.setText(" No files selected");
            return;
        }
        
        File targetDir = (activeTable == leftTable) ? rightCurrentDir : leftCurrentDir;
        
        String msg = sources.size() == 1 ? 
            "Copy \"" + sources.get(0).getName() + "\" to:" :
            "Copy " + sources.size() + " files to:";
        
        String target = (String) JOptionPane.showInputDialog(this, msg, "Copy",
            JOptionPane.PLAIN_MESSAGE, null, null, targetDir.getAbsolutePath());
        
        if (target == null) return;
        
        File destDir = new File(target);
        if (!destDir.exists() || !destDir.isDirectory()) {
            statusLabel.setText(" Invalid destination directory");
            return;
        }
        
        int copied = 0;
        for (File source : sources) {
            try {
                File dest = new File(destDir, source.getName());
                if (source.isDirectory()) {
                    copyDirectory(source, dest);
                } else {
                    Files.copy(source.toPath(), dest.toPath(), StandardCopyOption.REPLACE_EXISTING);
                }
                copied++;
            } catch (IOException e) {
                statusLabel.setText(" Error copying: " + e.getMessage());
            }
        }
        
        clearSelection();
        refresh();
        statusLabel.setText(" Copied " + copied + " file(s)");
    }
    
    private void copyDirectory(File source, File dest) throws IOException {
        if (!dest.exists()) dest.mkdirs();
        
        File[] files = source.listFiles();
        if (files == null) return;
        
        for (File file : files) {
            File newDest = new File(dest, file.getName());
            if (file.isDirectory()) {
                copyDirectory(file, newDest);
            } else {
                Files.copy(file.toPath(), newDest.toPath(), StandardCopyOption.REPLACE_EXISTING);
            }
        }
    }
    
    private void moveFiles() {
        List<File> sources = getSelectedFiles();
        if (sources.isEmpty()) {
            statusLabel.setText(" No files selected");
            return;
        }
        
        File targetDir = (activeTable == leftTable) ? rightCurrentDir : leftCurrentDir;
        
        // For single file, also offer rename
        if (sources.size() == 1) {
            String msg = "Move/Rename \"" + sources.get(0).getName() + "\" to:";
            String target = (String) JOptionPane.showInputDialog(this, msg, "Move/Rename",
                JOptionPane.PLAIN_MESSAGE, null, null, 
                new File(targetDir, sources.get(0).getName()).getAbsolutePath());
            
            if (target == null) return;
            
            try {
                File dest = new File(target);
                if (dest.getParentFile() != null && !dest.getParentFile().exists()) {
                    dest.getParentFile().mkdirs();
                }
                Files.move(sources.get(0).toPath(), dest.toPath(), StandardCopyOption.REPLACE_EXISTING);
                clearSelection();
                refresh();
                statusLabel.setText(" Moved: " + sources.get(0).getName());
            } catch (IOException e) {
                statusLabel.setText(" Error: " + e.getMessage());
            }
            return;
        }
        
        // Multiple files
        String target = (String) JOptionPane.showInputDialog(this, 
            "Move " + sources.size() + " files to:", "Move",
            JOptionPane.PLAIN_MESSAGE, null, null, targetDir.getAbsolutePath());
        
        if (target == null) return;
        
        File destDir = new File(target);
        if (!destDir.exists()) destDir.mkdirs();
        
        int moved = 0;
        for (File source : sources) {
            try {
                Files.move(source.toPath(), new File(destDir, source.getName()).toPath(), 
                          StandardCopyOption.REPLACE_EXISTING);
                moved++;
            } catch (IOException e) {
                statusLabel.setText(" Error moving: " + e.getMessage());
            }
        }
        
        clearSelection();
        refresh();
        statusLabel.setText(" Moved " + moved + " file(s)");
    }
    
    private void deleteFiles() {
        List<File> files = getSelectedFiles();
        if (files.isEmpty()) {
            statusLabel.setText(" No files selected");
            return;
        }
        
        String msg = files.size() == 1 ?
            "Delete \"" + files.get(0).getName() + "\"?" :
            "Delete " + files.size() + " file(s)?";
        
        int result = JOptionPane.showConfirmDialog(this, msg, "Delete", 
            JOptionPane.YES_NO_OPTION, JOptionPane.WARNING_MESSAGE);
        
        if (result != JOptionPane.YES_OPTION) return;
        
        int deleted = 0;
        for (File file : files) {
            if (deleteRecursive(file)) {
                deleted++;
            }
        }
        
        clearSelection();
        refresh();
        statusLabel.setText(" Deleted " + deleted + " file(s)");
    }
    
    private boolean deleteRecursive(File file) {
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteRecursive(child);
                }
            }
        }
        return file.delete();
    }
    
    private void createNewFolder() {
        String name = JOptionPane.showInputDialog(this, "Create directory:", "MkDir", 
            JOptionPane.PLAIN_MESSAGE);
        
        if (name == null || name.trim().isEmpty()) return;
        
        File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        File newDir = new File(currentDir, name.trim());
        
        if (newDir.mkdir()) {
            refresh();
            statusLabel.setText(" Created: " + name);
        } else {
            statusLabel.setText(" Failed to create directory");
        }
    }
    
    private void clearSelection() {
        if (activeTable == leftTable) {
            leftSelected.clear();
        } else {
            rightSelected.clear();
        }
    }
    
    private void refresh() {
        loadDirectory(leftCurrentDir, leftModel, leftSelected);
        loadDirectory(rightCurrentDir, rightModel, rightSelected);
        updatePathLabels();
        updateInfoLabels();
        statusLabel.setText(" Refreshed");
    }
    
    // ==================== View and Edit ====================
    
    private void viewFile() {
        int row = activeTable.getSelectedRow();
        if (row == -1) return;
        
        String name = (String) activeTable.getValueAt(row, 0);
        if ("..".equals(name)) return;
        
        File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        File file = findFileIgnoreCase(currentDir, name);
        
        if (file == null || !file.exists() || file.isDirectory()) return;
        
        // Show internal viewer
        showViewer(file, false);
    }
    
    private void editFile() {
        int row = activeTable.getSelectedRow();
        if (row == -1) return;
        
        String name = (String) activeTable.getValueAt(row, 0);
        if ("..".equals(name)) return;
        
        File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        File file = findFileIgnoreCase(currentDir, name);
        
        if (file == null || file.isDirectory()) return;
        
        // Show internal editor
        showViewer(file, true);
    }
    
    private void showViewer(File file, boolean editable) {
        JDialog dialog = new JDialog(this, (editable ? "Edit: " : "View: ") + file.getName(), true);
        dialog.setSize(800, 600);
        dialog.setLocationRelativeTo(this);
        
        JTextArea textArea = new JTextArea();
        textArea.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 12));
        textArea.setBackground(currentTheme.background);
        textArea.setForeground(currentTheme.foreground);
        textArea.setCaretColor(currentTheme.foreground);
        textArea.setEditable(editable);
        
        // Load file content
        try {
            StringBuilder content = new StringBuilder();
            try (BufferedReader reader = new BufferedReader(new FileReader(file))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    content.append(line).append("\n");
                }
            }
            textArea.setText(content.toString());
            textArea.setCaretPosition(0);
        } catch (IOException e) {
            textArea.setText("Error reading file: " + e.getMessage());
        }
        
        JScrollPane scrollPane = new JScrollPane(textArea);
        
        // Bottom panel with key hints
        JPanel bottomPanel = new JPanel(new FlowLayout(FlowLayout.LEFT));
        bottomPanel.setBackground(currentTheme.statusBg);
        JLabel hints = new JLabel(editable ? "F2-Save  F10/Esc-Quit" : "F3/F10/Esc-Quit");
        hints.setForeground(currentTheme.statusFg);
        bottomPanel.add(hints);
        
        dialog.setLayout(new BorderLayout());
        dialog.add(scrollPane, BorderLayout.CENTER);
        dialog.add(bottomPanel, BorderLayout.SOUTH);
        
        // Key bindings
        textArea.getInputMap().put(KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0), "close");
        textArea.getInputMap().put(KeyStroke.getKeyStroke(KeyEvent.VK_F10, 0), "close");
        textArea.getInputMap().put(KeyStroke.getKeyStroke(KeyEvent.VK_F3, 0), "close");
        textArea.getActionMap().put("close", new AbstractAction() {
            @Override
            public void actionPerformed(ActionEvent e) {
                dialog.dispose();
            }
        });
        
        if (editable) {
            textArea.getInputMap().put(KeyStroke.getKeyStroke(KeyEvent.VK_F2, 0), "save");
            textArea.getActionMap().put("save", new AbstractAction() {
                @Override
                public void actionPerformed(ActionEvent e) {
                    try (FileWriter writer = new FileWriter(file)) {
                        writer.write(textArea.getText());
                        statusLabel.setText(" Saved: " + file.getName());
                    } catch (IOException ex) {
                        JOptionPane.showMessageDialog(dialog, "Error saving: " + ex.getMessage());
                    }
                }
            });
        }
        
        dialog.setVisible(true);
    }
    
    private void executeFile(File file) {
        try {
            if (Desktop.isDesktopSupported()) {
                Desktop.getDesktop().open(file);
            }
        } catch (IOException e) {
            statusLabel.setText(" Error executing: " + e.getMessage());
        }
    }
    
    private void showAttributes() {
        int row = activeTable.getSelectedRow();
        if (row == -1) return;
        
        String name = (String) activeTable.getValueAt(row, 0);
        if ("..".equals(name)) return;
        
        File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        File file = findFileIgnoreCase(currentDir, name);
        
        if (file == null) return;
        
        StringBuilder info = new StringBuilder();
        info.append("Name: ").append(file.getName()).append("\n");
        info.append("Path: ").append(file.getAbsolutePath()).append("\n");
        info.append("Size: ").append(file.length()).append(" bytes\n");
        info.append("Modified: ").append(new SimpleDateFormat("yyyy-MM-dd HH:mm:ss")
            .format(new Date(file.lastModified()))).append("\n");
        info.append("\nAttributes:\n");
        info.append("  Readable: ").append(file.canRead()).append("\n");
        info.append("  Writable: ").append(file.canWrite()).append("\n");
        info.append("  Executable: ").append(file.canExecute()).append("\n");
        info.append("  Hidden: ").append(file.isHidden()).append("\n");
        info.append("  Directory: ").append(file.isDirectory()).append("\n");
        
        JOptionPane.showMessageDialog(this, info.toString(), "File Attributes", 
            JOptionPane.INFORMATION_MESSAGE);
    }
    
    // ==================== Commands and Dialogs ====================
    
    private void showHelp() {
        String help = """
            ╔══════════════════════════════════════════════════════════════╗
            ║                    RC COMMANDER - HELP                        ║
            ╠══════════════════════════════════════════════════════════════╣
            ║  FUNCTION KEYS:                                              ║
            ║  F1  - This help screen                                      ║
            ║  F2  - User menu                                             ║
            ║  F3  - View file (internal viewer)                           ║
            ║  F4  - Edit file (internal editor)                           ║
            ║  F5  - Copy file(s) to other panel                           ║
            ║  F6  - Move/Rename file(s)                                   ║
            ║  F7  - Create new directory                                  ║
            ║  F8  - Delete file(s)                                        ║
            ║  F9  - Activate pull-down menu                               ║
            ║  F10 - Quit program                                          ║
            ╠══════════════════════════════════════════════════════════════╣
            ║  NAVIGATION:                                                 ║
            ║  TAB       - Switch between panels                           ║
            ║  ENTER     - Enter directory / Execute file                  ║
            ║  BACKSPACE - Go to parent directory                          ║
            ║  Ctrl+\\   - Go to root directory                            ║
            ╠══════════════════════════════════════════════════════════════╣
            ║  SELECTION:                                                  ║
            ║  INSERT    - Select/deselect current file                    ║
            ║  +         - Select files by mask                            ║
            ║  -         - Deselect files by mask                          ║
            ║  *         - Invert selection                                ║
            ╠══════════════════════════════════════════════════════════════╣
            ║  OTHER:                                                      ║
            ║  Ctrl+O    - Toggle panels on/off                            ║
            ║  Ctrl+R    - Refresh panels                                  ║
            ║  Ctrl+U    - Swap panels                                     ║
            ║  Alt+F1/F2 - Select drive for left/right panel               ║
            ║  Alt+F7    - Find file                                       ║
            ║  Alt+F10   - Directory tree                                  ║
            ╠══════════════════════════════════════════════════════════════╣
            ║  QUICK SEARCH: Just start typing to find files               ║
            ╚══════════════════════════════════════════════════════════════╝
            """;
        
        JDialog dialog = new JDialog(this, "Help - F1", true);
        JTextArea text = new JTextArea(help);
        text.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 12));
        text.setEditable(false);
        text.setBackground(currentTheme.background);
        text.setForeground(currentTheme.foreground);
        
        JScrollPane scroll = new JScrollPane(text);
        dialog.add(scroll);
        dialog.setSize(550, 500);
        dialog.setLocationRelativeTo(this);
        
        text.addKeyListener(new KeyAdapter() {
            @Override
            public void keyPressed(KeyEvent e) {
                if (e.getKeyCode() == KeyEvent.VK_ESCAPE || 
                    e.getKeyCode() == KeyEvent.VK_F1 ||
                    e.getKeyCode() == KeyEvent.VK_ENTER) {
                    dialog.dispose();
                }
            }
        });
        
        dialog.setVisible(true);
    }
    
    private void showUserMenu() {
        JPopupMenu menu = new JPopupMenu("User Menu");
        
        for (UserMenuItem item : userMenu) {
            JMenuItem mi = new JMenuItem(item.hotkey + " " + item.name);
            mi.setMnemonic(item.hotkey);
            mi.addActionListener(e -> {
                if (!item.command.isEmpty()) {
                    commandLine.setText(item.command);
                    executeCommand();
                }
            });
            menu.add(mi);
        }
        
        menu.addSeparator();
        JMenuItem edit = new JMenuItem("Edit menu...");
        edit.addActionListener(e -> editUserMenu());
        menu.add(edit);
        
        // Show at center of active panel
        Point loc = activeTable.getLocationOnScreen();
        menu.show(activeTable, activeTable.getWidth() / 2, activeTable.getHeight() / 2);
    }
    
    private void editUserMenu() {
        JOptionPane.showMessageDialog(this, 
            "User menu editing would be implemented here.\n" +
            "In the original NC, this was configured via a text file.",
            "User Menu Editor", JOptionPane.INFORMATION_MESSAGE);
    }
    
    private void showPulldownMenu() {
        // Activate the menu bar (like F9 in NC)
        menuBar.getMenu(0).doClick();
    }
    
    private void findFile() {
        JDialog dialog = new JDialog(this, "Find File", true);
        dialog.setLayout(new BorderLayout(10, 10));
        
        JPanel inputPanel = new JPanel(new GridLayout(3, 2, 5, 5));
        inputPanel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));
        
        inputPanel.add(new JLabel("File mask:"));
        JTextField maskField = new JTextField("*.*");
        inputPanel.add(maskField);
        
        inputPanel.add(new JLabel("Containing text:"));
        JTextField textField = new JTextField();
        inputPanel.add(textField);
        
        inputPanel.add(new JLabel("Start from:"));
        File startDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        JTextField dirField = new JTextField(startDir.getAbsolutePath());
        inputPanel.add(dirField);
        
        JPanel buttonPanel = new JPanel(new FlowLayout());
        JButton searchBtn = new JButton("Search");
        JButton cancelBtn = new JButton("Cancel");
        buttonPanel.add(searchBtn);
        buttonPanel.add(cancelBtn);
        
        DefaultListModel<String> resultModel = new DefaultListModel<>();
        JList<String> resultList = new JList<>(resultModel);
        JScrollPane resultScroll = new JScrollPane(resultList);
        resultScroll.setPreferredSize(new Dimension(400, 200));
        
        searchBtn.addActionListener(e -> {
            resultModel.clear();
            String mask = maskField.getText();
            String text = textField.getText();
            File start = new File(dirField.getText());
            
            // Simple recursive search
            searchFiles(start, mask, text, resultModel, 0);
            statusLabel.setText(" Found " + resultModel.size() + " file(s)");
        });
        
        cancelBtn.addActionListener(e -> dialog.dispose());
        
        resultList.addMouseListener(new MouseAdapter() {
            @Override
            public void mouseClicked(MouseEvent e) {
                if (e.getClickCount() == 2) {
                    String path = resultList.getSelectedValue();
                    if (path != null) {
                        File file = new File(path);
                        if (file.exists()) {
                            changeDirectory(file.getParentFile());
                            dialog.dispose();
                        }
                    }
                }
            }
        });
        
        dialog.add(inputPanel, BorderLayout.NORTH);
        dialog.add(resultScroll, BorderLayout.CENTER);
        dialog.add(buttonPanel, BorderLayout.SOUTH);
        
        dialog.pack();
        dialog.setLocationRelativeTo(this);
        dialog.setVisible(true);
    }
    
    private void searchFiles(File dir, String mask, String containsText, 
                            DefaultListModel<String> results, int depth) {
        if (depth > 10 || results.size() > 500) return;
        
        String regex = mask.replace(".", "\\.").replace("*", ".*").replace("?", ".");
        Pattern pattern = Pattern.compile(regex, Pattern.CASE_INSENSITIVE);
        
        File[] files = dir.listFiles();
        if (files == null) return;
        
        for (File file : files) {
            if (file.isDirectory()) {
                searchFiles(file, mask, containsText, results, depth + 1);
            } else if (pattern.matcher(file.getName()).matches()) {
                if (containsText.isEmpty() || fileContainsText(file, containsText)) {
                    results.addElement(file.getAbsolutePath());
                }
            }
        }
    }
    
    private boolean fileContainsText(File file, String text) {
        try (BufferedReader reader = new BufferedReader(new FileReader(file))) {
            String line;
            while ((line = reader.readLine()) != null) {
                if (line.toLowerCase().contains(text.toLowerCase())) {
                    return true;
                }
            }
        } catch (IOException e) {
            // Not a text file or can't read
        }
        return false;
    }
    
    private void showHistory() {
        JOptionPane.showMessageDialog(this, 
            "Command history would show recent commands here.\n" +
            "Use up/down arrows in command line to navigate history.",
            "History - Alt+F8", JOptionPane.INFORMATION_MESSAGE);
    }
    
    private void showDirectoryTree() {
        File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        
        JDialog dialog = new JDialog(this, "Directory Tree", true);
        
        // Create tree from root
        File root = currentDir;
        while (root.getParentFile() != null) {
            root = root.getParentFile();
        }
        
        DefaultMutableTreeNode rootNode = new DefaultMutableTreeNode(root.getAbsolutePath());
        buildTreeNode(rootNode, root, 2);
        
        JTree tree = new JTree(rootNode);
        tree.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 12));
        
        tree.addTreeSelectionListener(e -> {
            DefaultMutableTreeNode node = (DefaultMutableTreeNode) tree.getLastSelectedPathComponent();
            if (node != null) {
                StringBuilder path = new StringBuilder();
                Object[] pathNodes = node.getUserObjectPath();
                for (Object p : pathNodes) {
                    String s = p.toString();
                    if (path.length() > 0 && !path.toString().endsWith(File.separator)) {
                        path.append(File.separator);
                    }
                    path.append(s);
                }
            }
        });
        
        tree.addMouseListener(new MouseAdapter() {
            @Override
            public void mouseClicked(MouseEvent e) {
                if (e.getClickCount() == 2) {
                    DefaultMutableTreeNode node = (DefaultMutableTreeNode) 
                        tree.getLastSelectedPathComponent();
                    if (node != null) {
                        File dir = getFileFromNode(node);
                        if (dir != null && dir.isDirectory()) {
                            changeDirectory(dir);
                            dialog.dispose();
                        }
                    }
                }
            }
        });
        
        JScrollPane scroll = new JScrollPane(tree);
        scroll.setPreferredSize(new Dimension(400, 400));
        
        dialog.add(scroll);
        dialog.pack();
        dialog.setLocationRelativeTo(this);
        dialog.setVisible(true);
    }
    
    private void buildTreeNode(DefaultMutableTreeNode node, File dir, int depth) {
        if (depth <= 0) return;
        
        File[] files = dir.listFiles();
        if (files == null) return;
        
        for (File file : files) {
            if (file.isDirectory() && !file.isHidden()) {
                DefaultMutableTreeNode child = new DefaultMutableTreeNode(file.getName());
                node.add(child);
                buildTreeNode(child, file, depth - 1);
            }
        }
    }
    
    private File getFileFromNode(DefaultMutableTreeNode node) {
        StringBuilder path = new StringBuilder();
        Object[] pathNodes = node.getUserObjectPath();
        for (int i = 0; i < pathNodes.length; i++) {
            String s = pathNodes[i].toString();
            if (i == 0) {
                path.append(s);
            } else {
                if (!path.toString().endsWith(File.separator)) {
                    path.append(File.separator);
                }
                path.append(s);
            }
        }
        return new File(path.toString());
    }
    
    private void selectDrive(boolean leftPanel) {
        File[] roots = File.listRoots();
        if (roots.length == 0) return;
        
        String[] options = new String[roots.length];
        for (int i = 0; i < roots.length; i++) {
            options[i] = roots[i].getAbsolutePath();
        }
        
        String selected = (String) JOptionPane.showInputDialog(this,
            "Select drive:", "Drive Selection",
            JOptionPane.PLAIN_MESSAGE, null, options, options[0]);
        
        if (selected != null) {
            File drive = new File(selected);
            if (leftPanel) {
                leftCurrentDir = drive;
                loadDirectory(drive, leftModel, leftSelected);
                if (activeTable != leftTable) {
                    leftTable.requestFocus();
                    activeTable = leftTable;
                }
            } else {
                rightCurrentDir = drive;
                loadDirectory(drive, rightModel, rightSelected);
                if (activeTable != rightTable) {
                    rightTable.requestFocus();
                    activeTable = rightTable;
                }
            }
            updatePathLabels();
            updateInfoLabels();
            updateActivePanel();
        }
    }
    
    private void swapPanels() {
        File tempDir = leftCurrentDir;
        leftCurrentDir = rightCurrentDir;
        rightCurrentDir = tempDir;
        
        Set<String> tempSel = new HashSet<>(leftSelected);
        leftSelected = new HashSet<>(rightSelected);
        rightSelected = tempSel;
        
        loadDirectory(leftCurrentDir, leftModel, leftSelected);
        loadDirectory(rightCurrentDir, rightModel, rightSelected);
        updatePathLabels();
        updateInfoLabels();
        
        statusLabel.setText(" Panels swapped");
    }
    
    private void togglePanels() {
        panelsVisible = !panelsVisible;
        mainPanel.setVisible(panelsVisible);
        statusLabel.setText(panelsVisible ? " Panels shown" : " Panels hidden (Ctrl+O to restore)");
    }
    
    private void compareDirectories() {
        Set<String> leftFiles = new HashSet<>();
        Set<String> rightFiles = new HashSet<>();
        
        for (int i = 0; i < leftModel.getRowCount(); i++) {
            String name = (String) leftModel.getValueAt(i, 0);
            if (!"..".equals(name)) leftFiles.add(name.toLowerCase());
        }
        
        for (int i = 0; i < rightModel.getRowCount(); i++) {
            String name = (String) rightModel.getValueAt(i, 0);
            if (!"..".equals(name)) rightFiles.add(name.toLowerCase());
        }
        
        // Select files that exist only in one panel
        leftSelected.clear();
        rightSelected.clear();
        
        for (int i = 0; i < leftModel.getRowCount(); i++) {
            String name = (String) leftModel.getValueAt(i, 0);
            if (!rightFiles.contains(name.toLowerCase()) && !"..".equals(name)) {
                leftSelected.add(name);
            }
        }
        
        for (int i = 0; i < rightModel.getRowCount(); i++) {
            String name = (String) rightModel.getValueAt(i, 0);
            if (!leftFiles.contains(name.toLowerCase()) && !"..".equals(name)) {
                rightSelected.add(name);
            }
        }
        
        leftTable.repaint();
        rightTable.repaint();
        updateInfoLabels();
        
        statusLabel.setText(" Unique files selected: " + leftSelected.size() + " left, " + 
                           rightSelected.size() + " right");
    }
    
    private void showConfiguration() {
        JOptionPane.showMessageDialog(this,
            "Configuration options:\n\n" +
            "• Color schemes (via Options menu)\n" +
            "• View modes: Brief/Full\n" +
            "• Sort options\n" +
            "• User menu customization\n\n" +
            "Settings are not persisted in this version.",
            "Configuration", JOptionPane.INFORMATION_MESSAGE);
    }
    
    private void setBriefView(JTable table) {
        // Show only name column prominently
        table.getColumnModel().getColumn(0).setPreferredWidth(200);
        table.getColumnModel().getColumn(1).setPreferredWidth(30);
        table.getColumnModel().getColumn(2).setPreferredWidth(50);
        table.getColumnModel().getColumn(3).setPreferredWidth(0);
        table.getColumnModel().getColumn(4).setPreferredWidth(0);
        statusLabel.setText(" Brief view");
    }
    
    private void setFullView(JTable table) {
        table.getColumnModel().getColumn(0).setPreferredWidth(150);
        table.getColumnModel().getColumn(1).setPreferredWidth(40);
        table.getColumnModel().getColumn(2).setPreferredWidth(70);
        table.getColumnModel().getColumn(3).setPreferredWidth(80);
        table.getColumnModel().getColumn(4).setPreferredWidth(40);
        statusLabel.setText(" Full view");
    }
    
    private void sortBy(DefaultTableModel model, int column) {
        // Re-sort would be implemented here
        statusLabel.setText(" Sorted by column " + COLUMN_NAMES[column]);
        refresh();
    }
    
    private void executeCommand() {
        String cmd = commandLine.getText().trim();
        if (cmd.isEmpty()) return;
        
        try {
            File workDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
            ProcessBuilder pb = new ProcessBuilder();
            
            if (System.getProperty("os.name").toLowerCase().contains("win")) {
                pb.command("cmd.exe", "/c", cmd);
            } else {
                pb.command("sh", "-c", cmd);
            }
            
            pb.directory(workDir);
            pb.inheritIO();
            Process p = pb.start();
            p.waitFor();
            
            refresh();
            statusLabel.setText(" Executed: " + cmd);
        } catch (Exception e) {
            statusLabel.setText(" Error: " + e.getMessage());
        }
        
        commandLine.setText("");
        activeTable.requestFocus();
    }
    
    private void handleQuickSearch(char c) {
        long now = System.currentTimeMillis();
        
        // Reset search after 1 second pause
        if (now - lastKeyTime > 1000) {
            quickSearchBuffer.setLength(0);
        }
        lastKeyTime = now;
        
        quickSearchBuffer.append(c);
        String search = quickSearchBuffer.toString().toLowerCase();
        
        // Find matching file
        DefaultTableModel model = (activeTable == leftTable) ? leftModel : rightModel;
        for (int i = 0; i < model.getRowCount(); i++) {
            String name = ((String) model.getValueAt(i, 0)).toLowerCase();
            if (name.startsWith(search)) {
                activeTable.setRowSelectionInterval(i, i);
                activeTable.scrollRectToVisible(activeTable.getCellRect(i, 0, true));
                break;
            }
        }
        
        statusLabel.setText(" Quick search: " + quickSearchBuffer.toString());
    }
    
    private void confirmExit() {
        int result = JOptionPane.showConfirmDialog(this,
            "Exit RC Commander?", "Quit",
            JOptionPane.YES_NO_OPTION, JOptionPane.QUESTION_MESSAGE);
        
        if (result == JOptionPane.YES_OPTION) {
            System.exit(0);
        }
    }
    
    // ==================== Theme ====================
    
    private void switchTheme(String themeName) {
        currentTheme = THEMES.get(themeName);
        applyTheme();
        statusLabel.setText(" Theme: " + themeName);
    }
    
    private void applyTheme() {
        // Main background
        getContentPane().setBackground(Color.BLACK);
        mainPanel.setBackground(Color.BLACK);
        
        // Panels
        leftPanel.setBackground(currentTheme.background);
        rightPanel.setBackground(currentTheme.background);
        
        // Tables
        applyThemeToTable(leftTable);
        applyThemeToTable(rightTable);
        
        // Path labels
        leftPathLabel.setBackground(currentTheme.headerBg);
        leftPathLabel.setForeground(currentTheme.headerFg);
        rightPathLabel.setBackground(currentTheme.headerBg);
        rightPathLabel.setForeground(currentTheme.headerFg);
        
        // Info labels
        leftInfoLabel.setBackground(currentTheme.headerBg);
        leftInfoLabel.setForeground(currentTheme.headerFg);
        rightInfoLabel.setBackground(currentTheme.headerBg);
        rightInfoLabel.setForeground(currentTheme.headerFg);
        
        // Status bar
        statusLabel.setBackground(currentTheme.statusBg);
        statusLabel.setForeground(currentTheme.statusFg);
        statusLabel.setOpaque(true);
        
        // Command line
        commandLine.setBackground(currentTheme.background);
        commandLine.setForeground(currentTheme.foreground);
        commandLine.setCaretColor(currentTheme.foreground);
        
        // Function key panel
        functionKeyPanel.setBackground(currentTheme.statusBg);
        for (Component c : functionKeyPanel.getComponents()) {
            if (c instanceof JButton) {
                c.setBackground(currentTheme.statusBg);
                c.setForeground(currentTheme.statusFg);
            }
        }
        
        // Menu bar
        menuBar.setBackground(currentTheme.headerBg);
        menuBar.setForeground(currentTheme.headerFg);
        
        updateActivePanel();
        repaint();
    }
    
    private void applyThemeToTable(JTable table) {
        table.setBackground(currentTheme.background);
        table.setForeground(currentTheme.foreground);
        table.setSelectionBackground(currentTheme.selection);
        table.setSelectionForeground(currentTheme.selectedText);
        table.setGridColor(currentTheme.background);
        table.getTableHeader().setBackground(currentTheme.headerBg);
        table.getTableHeader().setForeground(currentTheme.headerFg);
        
        // Scroll pane
        Container parent = table.getParent();
        if (parent instanceof JViewport) {
            parent = parent.getParent();
            if (parent instanceof JScrollPane) {
                ((JScrollPane) parent).getViewport().setBackground(currentTheme.background);
            }
        }
    }
    
    // ==================== Custom Renderer ====================
    
    private class NCTableCellRenderer extends DefaultTableCellRenderer {
        private final boolean isLeftTable;
        
        NCTableCellRenderer(boolean isLeft) {
            this.isLeftTable = isLeft;
        }
        
        @Override
        public Component getTableCellRendererComponent(JTable table, Object value,
                boolean isSelected, boolean hasFocus, int row, int column) {
            
            Component comp = super.getTableCellRendererComponent(
                table, value, isSelected, hasFocus, row, column);
            
            String name = (String) table.getValueAt(row, 0);
            String sizeStr = (String) table.getValueAt(row, 2);
            Set<String> selected = isLeftTable ? leftSelected : rightSelected;
            
            // Check if file is selected (via INSERT key)
            boolean fileSelected = selected.contains(name);
            
            if (isSelected) {
                // Cursor selection
                comp.setBackground(currentTheme.selection);
                comp.setForeground(fileSelected ? Color.YELLOW : currentTheme.selectedText);
            } else {
                comp.setBackground(currentTheme.background);
                
                if (fileSelected) {
                    // Selected files shown in yellow
                    comp.setForeground(Color.YELLOW);
                } else if ("..".equals(name) || "<DIR>".equals(sizeStr) || "<UP-DIR>".equals(sizeStr)) {
                    // Directories in white/bright
                    comp.setForeground(currentTheme.directory);
                } else {
                    // Regular files
                    String ext = (String) table.getValueAt(row, 1);
                    if (isExecutable(ext)) {
                        comp.setForeground(currentTheme.executable);
                    } else {
                        comp.setForeground(currentTheme.foreground);
                    }
                }
            }
            
            // Left-align name, right-align size
            if (column == 2) {
                setHorizontalAlignment(SwingConstants.RIGHT);
            } else {
                setHorizontalAlignment(SwingConstants.LEFT);
            }
            
            return comp;
        }
    }
    
    // ==================== Main ====================
    
    public static void main(String[] args) {
        // Set system look and feel for better integration
        try {
            // Use cross-platform for consistent NC look
            UIManager.setLookAndFeel(UIManager.getCrossPlatformLookAndFeelClassName());
        } catch (Exception e) {
            e.printStackTrace();
        }
        
        SwingUtilities.invokeLater(() -> {
            RCCommander nc = new RCCommander();
            nc.setVisible(true);
        });
    }
}
