import javax.swing.*;
import javax.swing.table.DefaultTableModel;
import java.awt.*;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.*;
import java.util.List;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public class LinuxResourceMonitor extends JFrame {
    private JTable processTable;
    private JTable serviceTable;
    private DefaultTableModel processModel;
    private DefaultTableModel serviceModel;
    private ScheduledExecutorService scheduler;
    private JLabel lastUpdateLabel;
    
    public LinuxResourceMonitor() {
        setTitle("Linux Resource Monitor");
        setSize(1200, 700);
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(new BorderLayout(10, 10));
        
        // Create main panel with padding
        JPanel mainPanel = new JPanel(new GridLayout(1, 2, 10, 10));
        mainPanel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));
        
        // Process panel
        JPanel processPanel = createTablePanel("Top 10 Processes by CPU/Memory", true);
        mainPanel.add(processPanel);
        
        // Service panel
        JPanel servicePanel = createTablePanel("Top 10 Services by Memory", false);
        mainPanel.add(servicePanel);
        
        add(mainPanel, BorderLayout.CENTER);
        
        // Control panel
        JPanel controlPanel = new JPanel(new FlowLayout(FlowLayout.LEFT));
        JButton refreshButton = new JButton("Refresh Now");
        refreshButton.addActionListener(e -> updateData());
        
        JButton stopButton = new JButton("Stop Auto-Refresh");
        stopButton.addActionListener(e -> stopAutoRefresh());
        
        JButton startButton = new JButton("Start Auto-Refresh");
        startButton.addActionListener(e -> startAutoRefresh());
        
        lastUpdateLabel = new JLabel("Last update: Never");
        
        controlPanel.add(refreshButton);
        controlPanel.add(stopButton);
        controlPanel.add(startButton);
        controlPanel.add(Box.createHorizontalStrut(20));
        controlPanel.add(lastUpdateLabel);
        
        add(controlPanel, BorderLayout.SOUTH);
        
        // Start auto-refresh
        startAutoRefresh();
        updateData();
    }
    
    private JPanel createTablePanel(String title, boolean isProcess) {
        JPanel panel = new JPanel(new BorderLayout(5, 5));
        panel.setBorder(BorderFactory.createTitledBorder(title));
        
        String[] columns = isProcess 
            ? new String[]{"PID", "Name", "CPU%", "MEM%", "User"}
            : new String[]{"Service", "Status", "Memory", "PID"};
        
        DefaultTableModel model = new DefaultTableModel(columns, 0) {
            @Override
            public boolean isCellEditable(int row, int column) {
                return false;
            }
        };
        
        JTable table = new JTable(model);
        table.setFillsViewportHeight(true);
        table.getTableHeader().setReorderingAllowed(false);
        
        if (isProcess) {
            processTable = table;
            processModel = model;
            table.getColumnModel().getColumn(0).setPreferredWidth(60);
            table.getColumnModel().getColumn(1).setPreferredWidth(150);
            table.getColumnModel().getColumn(2).setPreferredWidth(60);
            table.getColumnModel().getColumn(3).setPreferredWidth(60);
            table.getColumnModel().getColumn(4).setPreferredWidth(80);
        } else {
            serviceTable = table;
            serviceModel = model;
            table.getColumnModel().getColumn(0).setPreferredWidth(200);
            table.getColumnModel().getColumn(1).setPreferredWidth(80);
            table.getColumnModel().getColumn(2).setPreferredWidth(100);
            table.getColumnModel().getColumn(3).setPreferredWidth(60);
        }
        
        JScrollPane scrollPane = new JScrollPane(table);
        panel.add(scrollPane, BorderLayout.CENTER);
        
        return panel;
    }
    
    private void startAutoRefresh() {
        if (scheduler == null || scheduler.isShutdown()) {
            scheduler = Executors.newSingleThreadScheduledExecutor();
            scheduler.scheduleAtFixedRate(this::updateData, 0, 3, TimeUnit.SECONDS);
        }
    }
    
    private void stopAutoRefresh() {
        if (scheduler != null && !scheduler.isShutdown()) {
            scheduler.shutdown();
        }
    }
    
    private void updateData() {
        SwingUtilities.invokeLater(() -> {
            updateProcesses();
            updateServices();
            lastUpdateLabel.setText("Last update: " + new Date().toString());
        });
    }
    
    private void updateProcesses() {
        processModel.setRowCount(0);
        
        try {
            Process process = Runtime.getRuntime().exec(new String[]{
                "bash", "-c", "ps aux --sort=-%cpu | head -n 11 | tail -n 10"
            });
            
            BufferedReader reader = new BufferedReader(
                new InputStreamReader(process.getInputStream())
            );
            
            String line;
            while ((line = reader.readLine()) != null) {
                String[] parts = line.trim().split("\\s+");
                if (parts.length >= 11) {
                    String user = parts[0];
                    String pid = parts[1];
                    String cpu = parts[2];
                    String mem = parts[3];
                    String command = parts[10];
                    
                    processModel.addRow(new Object[]{pid, command, cpu, mem, user});
                }
            }
            
            reader.close();
            process.waitFor();
        } catch (Exception e) {
            processModel.addRow(new Object[]{"Error", e.getMessage(), "", "", ""});
        }
    }
    
    private void updateServices() {
        serviceModel.setRowCount(0);
        
        try {
            // Get all running services
            Process serviceList = Runtime.getRuntime().exec(new String[]{
                "systemctl", "list-units", "--type=service", "--state=running", "--no-pager"
            });
            
            BufferedReader reader = new BufferedReader(
                new InputStreamReader(serviceList.getInputStream())
            );
            
            List<ServiceInfo> services = new ArrayList<>();
            String line;
            
            while ((line = reader.readLine()) != null) {
                if (line.contains(".service") && line.contains("running")) {
                    String[] parts = line.trim().split("\\s+");
                    if (parts.length > 0) {
                        String serviceName = parts[0];
                        ServiceInfo info = getServiceMemoryUsage(serviceName);
                        if (info != null) {
                            services.add(info);
                        }
                    }
                }
            }
            
            reader.close();
            serviceList.waitFor();
            
            // Sort by memory usage and get top 10
            services.sort((a, b) -> Long.compare(b.memoryKB, a.memoryKB));
            
            for (int i = 0; i < Math.min(10, services.size()); i++) {
                ServiceInfo info = services.get(i);
                serviceModel.addRow(new Object[]{
                    info.name,
                    "running",
                    formatMemory(info.memoryKB),
                    info.pid
                });
            }
            
        } catch (Exception e) {
            serviceModel.addRow(new Object[]{"Error", e.getMessage(), "", ""});
        }
    }
    
    private ServiceInfo getServiceMemoryUsage(String serviceName) {
        try {
            Process pidProcess = Runtime.getRuntime().exec(new String[]{
                "systemctl", "show", serviceName, "--property=MainPID"
            });
            
            BufferedReader reader = new BufferedReader(
                new InputStreamReader(pidProcess.getInputStream())
            );
            
            String line = reader.readLine();
            reader.close();
            pidProcess.waitFor();
            
            if (line != null && line.startsWith("MainPID=")) {
                String pid = line.substring(8).trim();
                if (!pid.equals("0")) {
                    long memory = getProcessMemory(pid);
                    return new ServiceInfo(serviceName, pid, memory);
                }
            }
        } catch (Exception e) {
            // Skip services we can't query
        }
        return null;
    }
    
    private long getProcessMemory(String pid) {
        try {
            Process memProcess = Runtime.getRuntime().exec(new String[]{
                "ps", "-p", pid, "-o", "rss="
            });
            
            BufferedReader reader = new BufferedReader(
                new InputStreamReader(memProcess.getInputStream())
            );
            
            String line = reader.readLine();
            reader.close();
            memProcess.waitFor();
            
            if (line != null) {
                return Long.parseLong(line.trim());
            }
        } catch (Exception e) {
            // Return 0 if we can't get memory info
        }
        return 0;
    }
    
    private String formatMemory(long memoryKB) {
        if (memoryKB < 1024) {
            return memoryKB + " KB";
        } else if (memoryKB < 1024 * 1024) {
            return String.format("%.1f MB", memoryKB / 1024.0);
        } else {
            return String.format("%.2f GB", memoryKB / (1024.0 * 1024.0));
        }
    }
    
    private static class ServiceInfo {
        String name;
        String pid;
        long memoryKB;
        
        ServiceInfo(String name, String pid, long memoryKB) {
            this.name = name;
            this.pid = pid;
            this.memoryKB = memoryKB;
        }
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            LinuxResourceMonitor monitor = new LinuxResourceMonitor();
            monitor.setVisible(true);
        });
    }
}

class ServiceInfo {
    String name;
    String pid;
    long memoryKB;
    
    ServiceInfo(String name, String pid, long memoryKB) {
        this.name = name;
        this.pid = pid;
        this.memoryKB = memoryKB;
    }
}
