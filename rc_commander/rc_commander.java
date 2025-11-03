import javax.swing.*;
import javax.swing.border.Border;
import javax.swing.table.DefaultTableCellRenderer;
import javax.swing.table.DefaultTableModel;
import javax.swing.table.TableCellRenderer;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyEvent;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.io.File;
import java.io.IOException;
import java.nio.file.*;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;

public class NortonCommanderClone extends JFrame {
    private static final String[] COLUMN_NAMES = {"Name", "Size", "Date", "Attr"};
    
    // Theme definitions
    private static final Map<String, Theme> THEMES = new HashMap<>();
    static {
        THEMES.put("Classic DOS", new Theme(
            Color.BLACK, Color.CYAN, Color.BLUE, Color.WHITE, Color.YELLOW, Color.GREEN
        ));
        THEMES.put("Modern Dark", new Theme(
            new Color(30, 30, 30), Color.WHITE, new Color(60, 60, 60), 
            new Color(200, 200, 200), new Color(100, 150, 255), new Color(80, 200, 120)
        ));
        THEMES.put("Light", new Theme(
            Color.WHITE, Color.BLACK, new Color(240, 240, 240), 
            Color.DARK_GRAY, new Color(0, 120, 215), new Color(34, 139, 34)
        ));
        THEMES.put("Matrix", new Theme(
            Color.BLACK, new Color(0, 255, 0), new Color(20, 20, 20), 
            new Color(150, 255, 150), new Color(0, 200, 0), new Color(0, 180, 0)
        ));
    }
    
    private Theme currentTheme = THEMES.get("Classic DOS");
    
    // UI Components
    private JTable leftTable, rightTable;
    private DefaultTableModel leftModel, rightModel;
    private JLabel leftPathLabel, rightPathLabel;
    private JLabel statusLabel;
    private JTable activeTable;
    private File leftCurrentDir, rightCurrentDir;
    private JPanel leftPanel, rightPanel;
    
    static class Theme {
        Color background, foreground, panelBackground, text, selection, directory;
        
        Theme(Color bg, Color fg, Color panelBg, Color txt, Color sel, Color dir) {
            this.background = bg;
            this.foreground = fg;
            this.panelBackground = panelBg;
            this.text = txt;
            this.selection = sel;
            this.directory = dir;
        }
    }
    
    public NortonCommanderClone() {
        initializeUI();
        setupKeyBindings();
        loadDirectory(new File(System.getProperty("user.home")), leftModel);
        loadDirectory(new File(System.getProperty("user.home")), rightModel);
        leftCurrentDir = new File(System.getProperty("user.home"));
        rightCurrentDir = new File(System.getProperty("user.home"));
        updatePathLabels();
        activeTable = leftTable;
        updateActivePanel();
        applyTheme();
    }
    
    private void initializeUI() {
        setTitle("Norton Commander Clone");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(new BorderLayout());
        
        // Create menu bar
        createMenuBar();
        
        // Create main panel
        JPanel mainPanel = new JPanel(new GridLayout(1, 2, 2, 0));
        
        // Left panel
        leftPanel = createFilePanel();
        leftModel = new DefaultTableModel(COLUMN_NAMES, 0) {
            @Override
            public boolean isCellEditable(int row, int column) {
                return false;
            }
        };
        leftTable = createFileTable(leftModel);
        leftPathLabel = new JLabel();
        setupFilePanel(leftPanel, leftTable, leftPathLabel, "Left Panel");
        
        // Right panel
        rightPanel = createFilePanel();
        rightModel = new DefaultTableModel(COLUMN_NAMES, 0) {
            @Override
            public boolean isCellEditable(int row, int column) {
                return false;
            }
        };
        rightTable = createFileTable(rightModel);
        rightPathLabel = new JLabel();
        setupFilePanel(rightPanel, rightTable, rightPathLabel, "Right Panel");
        
        mainPanel.add(leftPanel);
        mainPanel.add(rightPanel);
        
        // Status bar
        statusLabel = new JLabel(" Ready");
        statusLabel.setBorder(BorderFactory.createLoweredBevelBorder());
        
        // Function key panel
        JPanel functionPanel = createFunctionKeyPanel();
        
        add(mainPanel, BorderLayout.CENTER);
        add(statusLabel, BorderLayout.SOUTH);
        add(functionPanel, BorderLayout.PAGE_END);
        
        setSize(1000, 700);
        setLocationRelativeTo(null);
    }
    
    private void createMenuBar() {
        JMenuBar menuBar = new JMenuBar();
        
        // File menu
        JMenu fileMenu = new JMenu("File");
        fileMenu.add(createMenuItem("Copy", KeyEvent.VK_F5, e -> copyFile()));
        fileMenu.add(createMenuItem("Move", KeyEvent.VK_F6, e -> moveFile()));
        fileMenu.add(createMenuItem("Delete", KeyEvent.VK_F8, e -> deleteFile()));
        fileMenu.addSeparator();
        fileMenu.add(createMenuItem("Exit", KeyEvent.VK_F10, e -> System.exit(0)));
        
        // View menu
        JMenu viewMenu = new JMenu("View");
        JMenu themeMenu = new JMenu("Themes");
        for (String themeName : THEMES.keySet()) {
            themeMenu.add(createMenuItem(themeName, 0, e -> switchTheme(themeName)));
        }
        viewMenu.add(themeMenu);
        viewMenu.add(createMenuItem("Refresh", KeyEvent.VK_F2, e -> refresh()));
        
        // Tools menu
        JMenu toolsMenu = new JMenu("Tools");
        toolsMenu.add(createMenuItem("New Folder", KeyEvent.VK_F7, e -> createNewFolder()));
        
        menuBar.add(fileMenu);
        menuBar.add(viewMenu);
        menuBar.add(toolsMenu);
        
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
    
    private JPanel createFilePanel() {
        JPanel panel = new JPanel(new BorderLayout());
        panel.setBorder(BorderFactory.createRaisedBevelBorder());
        return panel;
    }
    
    private JTable createFileTable(DefaultTableModel model) {
        JTable table = new JTable(model);
        table.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
        table.setRowSelectionAllowed(true);
        table.setColumnSelectionAllowed(false);
        table.getTableHeader().setReorderingAllowed(false);
        
        // Set column widths
        table.getColumnModel().getColumn(0).setPreferredWidth(200);
        table.getColumnModel().getColumn(1).setPreferredWidth(80);
        table.getColumnModel().getColumn(2).setPreferredWidth(120);
        table.getColumnModel().getColumn(3).setPreferredWidth(50);
        
        // Custom cell renderer
        table.setDefaultRenderer(Object.class, new FileTableCellRenderer());
        
        // Mouse listener for double-click
        table.addMouseListener(new MouseAdapter() {
            @Override
            public void mouseClicked(MouseEvent e) {
                if (e.getClickCount() == 2) {
                    enterDirectory();
                }
            }
        });
        
        // Focus listener to track active panel
        table.addFocusListener(new java.awt.event.FocusAdapter() {
            @Override
            public void focusGained(java.awt.event.FocusEvent e) {
                activeTable = table;
                updateActivePanel();
            }
        });
        
        return table;
    }
    
    private void setupFilePanel(JPanel panel, JTable table, JLabel pathLabel, String title) {
        pathLabel.setText(title);
        pathLabel.setBorder(BorderFactory.createEmptyBorder(2, 5, 2, 5));
        
        JScrollPane scrollPane = new JScrollPane(table);
        scrollPane.setVerticalScrollBarPolicy(JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED);
        scrollPane.setHorizontalScrollBarPolicy(JScrollPane.HORIZONTAL_SCROLLBAR_AS_NEEDED);
        
        panel.add(pathLabel, BorderLayout.NORTH);
        panel.add(scrollPane, BorderLayout.CENTER);
    }
    
    private JPanel createFunctionKeyPanel() {
        JPanel panel = new JPanel(new GridLayout(1, 10, 1, 1));
        panel.setBorder(BorderFactory.createLoweredBevelBorder());
        
        String[] functions = {"F1 Help", "F2 Refresh", "F3 View", "F4 Edit", "F5 Copy", 
                             "F6 Move", "F7 MkDir", "F8 Delete", "F9 Menu", "F10 Exit"};
        
        for (String func : functions) {
            JButton btn = new JButton(func);
            btn.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 10));
            btn.setMargin(new Insets(2, 2, 2, 2));
            panel.add(btn);
        }
        
        return panel;
    }
    
    private void setupKeyBindings() {
        // Set up key bindings for function keys
        JRootPane rootPane = getRootPane();
        
        addKeyBinding(rootPane, KeyEvent.VK_F2, "refresh", e -> refresh());
        addKeyBinding(rootPane, KeyEvent.VK_F5, "copy", e -> copyFile());
        addKeyBinding(rootPane, KeyEvent.VK_F6, "move", e -> moveFile());
        addKeyBinding(rootPane, KeyEvent.VK_F7, "mkdir", e -> createNewFolder());
        addKeyBinding(rootPane, KeyEvent.VK_F8, "delete", e -> deleteFile());
        addKeyBinding(rootPane, KeyEvent.VK_F10, "exit", e -> System.exit(0));
        addKeyBinding(rootPane, KeyEvent.VK_TAB, "switch", e -> switchPanel());
        addKeyBinding(rootPane, KeyEvent.VK_ENTER, "enter", e -> enterDirectory());
    }
    
    private void addKeyBinding(JRootPane rootPane, int keyCode, String actionName, ActionListener action) {
        KeyStroke keyStroke = KeyStroke.getKeyStroke(keyCode, 0);
        rootPane.getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW).put(keyStroke, actionName);
        rootPane.getActionMap().put(actionName, new AbstractAction() {
            @Override
            public void actionPerformed(ActionEvent e) {
                action.actionPerformed(e);
            }
        });
    }
    
    private void loadDirectory(File directory, DefaultTableModel model) {
        model.setRowCount(0);
        
        if (directory == null || !directory.exists() || !directory.isDirectory()) {
            return;
        }
        
        // Add parent directory entry
        if (directory.getParent() != null) {
            model.addRow(new Object[]{"..", "<DIR>", "", ""});
        }
        
        File[] files = directory.listFiles();
        if (files != null) {
            for (File file : files) {
                String name = file.getName();
                String size = file.isDirectory() ? "<DIR>" : formatFileSize(file.length());
                String date = new SimpleDateFormat("MM/dd/yyyy HH:mm").format(new Date(file.lastModified()));
                String attr = getFileAttributes(file);
                
                model.addRow(new Object[]{name, size, date, attr});
            }
        }
    }
    
    private String formatFileSize(long size) {
        if (size < 1024) return size + " B";
        if (size < 1024 * 1024) return (size / 1024) + " KB";
        if (size < 1024 * 1024 * 1024) return (size / (1024 * 1024)) + " MB";
        return (size / (1024 * 1024 * 1024)) + " GB";
    }
    
    private String getFileAttributes(File file) {
        StringBuilder attr = new StringBuilder();
        if (file.canRead()) attr.append("r");
        if (file.canWrite()) attr.append("w");
        if (file.isHidden()) attr.append("h");
        if (file.isDirectory()) attr.append("d");
        return attr.toString();
    }
    
    private void updatePathLabels() {
        leftPathLabel.setText("Left: " + leftCurrentDir.getAbsolutePath());
        rightPathLabel.setText("Right: " + rightCurrentDir.getAbsolutePath());
    }
    
    private void updateActivePanel() {
        Border activeBorder = BorderFactory.createLineBorder(currentTheme.selection, 2);
        Border inactiveBorder = BorderFactory.createRaisedBevelBorder();
        
        if (activeTable == leftTable) {
            leftPanel.setBorder(activeBorder);
            rightPanel.setBorder(inactiveBorder);
        } else {
            leftPanel.setBorder(inactiveBorder);
            rightPanel.setBorder(activeBorder);
        }
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
    }
    
    private void enterDirectory() {
        int selectedRow = activeTable.getSelectedRow();
        if (selectedRow == -1) return;
        
        String fileName = (String) activeTable.getValueAt(selectedRow, 0);
        File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        
        if ("..".equals(fileName)) {
            File parent = currentDir.getParentFile();
            if (parent != null) {
                changeDirectory(parent);
            }
        } else {
            File file = new File(currentDir, fileName);
            if (file.isDirectory()) {
                changeDirectory(file);
            } else {
                viewFile(file);
            }
        }
    }
    
    private void changeDirectory(File newDir) {
        if (activeTable == leftTable) {
            leftCurrentDir = newDir;
            loadDirectory(newDir, leftModel);
        } else {
            rightCurrentDir = newDir;
            loadDirectory(newDir, rightModel);
        }
        updatePathLabels();
        statusLabel.setText(" Changed to: " + newDir.getAbsolutePath());
    }
    
    private void viewFile(File file) {
        if (Desktop.isDesktopSupported()) {
            try {
                Desktop.getDesktop().open(file);
            } catch (IOException e) {
                statusLabel.setText(" Error opening file: " + e.getMessage());
            }
        }
    }
    
    private void copyFile() {
        performFileOperation("Copy");
    }
    
    private void moveFile() {
        performFileOperation("Move");
    }
    
    private void performFileOperation(String operation) {
        int selectedRow = activeTable.getSelectedRow();
        if (selectedRow == -1) {
            statusLabel.setText(" No file selected");
            return;
        }
        
        String fileName = (String) activeTable.getValueAt(selectedRow, 0);
        if ("..".equals(fileName)) {
            statusLabel.setText(" Cannot " + operation.toLowerCase() + " parent directory");
            return;
        }
        
        File sourceDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
        File targetDir = (activeTable == leftTable) ? rightCurrentDir : leftCurrentDir;
        File sourceFile = new File(sourceDir, fileName);
        File targetFile = new File(targetDir, fileName);
        
        if (targetFile.exists()) {
            int result = JOptionPane.showConfirmDialog(this, 
                "File already exists. Overwrite?", 
                operation, 
                JOptionPane.YES_NO_OPTION);
            if (result != JOptionPane.YES_OPTION) {
                return;
            }
        }
        
        try {
            if ("Copy".equals(operation)) {
                Files.copy(sourceFile.toPath(), targetFile.toPath(), 
                          StandardCopyOption.REPLACE_EXISTING);
            } else {
                Files.move(sourceFile.toPath(), targetFile.toPath(), 
                          StandardCopyOption.REPLACE_EXISTING);
            }
            refresh();
            statusLabel.setText(" " + operation + " completed: " + fileName);
        } catch (IOException e) {
            statusLabel.setText(" Error: " + e.getMessage());
        }
    }
    
    private void deleteFile() {
        int selectedRow = activeTable.getSelectedRow();
        if (selectedRow == -1) {
            statusLabel.setText(" No file selected");
            return;
        }
        
        String fileName = (String) activeTable.getValueAt(selectedRow, 0);
        if ("..".equals(fileName)) {
            statusLabel.setText(" Cannot delete parent directory");
            return;
        }
        
        int result = JOptionPane.showConfirmDialog(this, 
            "Delete " + fileName + "?", 
            "Delete", 
            JOptionPane.YES_NO_OPTION);
        
        if (result == JOptionPane.YES_OPTION) {
            File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
            File fileToDelete = new File(currentDir, fileName);
            
            if (fileToDelete.delete()) {
                refresh();
                statusLabel.setText(" Deleted: " + fileName);
            } else {
                statusLabel.setText(" Error deleting: " + fileName);
            }
        }
    }
    
    private void createNewFolder() {
        String folderName = JOptionPane.showInputDialog(this, "Enter folder name:");
        if (folderName != null && !folderName.trim().isEmpty()) {
            File currentDir = (activeTable == leftTable) ? leftCurrentDir : rightCurrentDir;
            File newFolder = new File(currentDir, folderName.trim());
            
            if (newFolder.mkdir()) {
                refresh();
                statusLabel.setText(" Created folder: " + folderName);
            } else {
                statusLabel.setText(" Error creating folder: " + folderName);
            }
        }
    }
    
    private void refresh() {
        loadDirectory(leftCurrentDir, leftModel);
        loadDirectory(rightCurrentDir, rightModel);
        statusLabel.setText(" Refreshed");
    }
    
    private void switchTheme(String themeName) {
        currentTheme = THEMES.get(themeName);
        applyTheme();
        statusLabel.setText(" Theme changed to: " + themeName);
    }
    
    private void applyTheme() {
        // Apply theme to main components
        getContentPane().setBackground(currentTheme.background);
        
        // Tables
        applyThemeToTable(leftTable);
        applyThemeToTable(rightTable);
        
        // Labels
        leftPathLabel.setBackground(currentTheme.panelBackground);
        leftPathLabel.setForeground(currentTheme.text);
        leftPathLabel.setOpaque(true);
        
        rightPathLabel.setBackground(currentTheme.panelBackground);
        rightPathLabel.setForeground(currentTheme.text);
        rightPathLabel.setOpaque(true);
        
        // Status bar
        statusLabel.setBackground(currentTheme.panelBackground);
        statusLabel.setForeground(currentTheme.text);
        statusLabel.setOpaque(true);
        
        // Update active panel border
        updateActivePanel();
        
        repaint();
    }
    
    private void applyThemeToTable(JTable table) {
        table.setBackground(currentTheme.background);
        table.setForeground(currentTheme.foreground);
        table.setSelectionBackground(currentTheme.selection);
        table.setSelectionForeground(currentTheme.background);
        table.setGridColor(currentTheme.foreground);
        table.getTableHeader().setBackground(currentTheme.panelBackground);
        table.getTableHeader().setForeground(currentTheme.text);
    }
    
    private class FileTableCellRenderer extends DefaultTableCellRenderer {
        @Override
        public Component getTableCellRendererComponent(JTable table, Object value, 
                boolean isSelected, boolean hasFocus, int row, int column) {
            
            Component comp = super.getTableCellRendererComponent(table, value, isSelected, hasFocus, row, column);
            
            if (!isSelected) {
                if (column == 0) { // Name column
                    String name = (String) value;
                    if ("..".equals(name) || "<DIR>".equals(table.getValueAt(row, 1))) {
                        comp.setForeground(currentTheme.directory);
                    } else {
                        comp.setForeground(currentTheme.foreground);
                    }
                } else {
                    comp.setForeground(currentTheme.foreground);
                }
                comp.setBackground(currentTheme.background);
            }
            
            return comp;
        }
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            try {
                UIManager.setLookAndFeel(UIManager.getSystemLookAndFeel());
            } catch (Exception e) {
                e.printStackTrace();
            }
            
            new NortonCommanderClone().setVisible(true);
        });
    }
}
