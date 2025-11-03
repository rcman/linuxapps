Features:

Top 10 Processes: Shows processes sorted by CPU/Memory usage with PID, name, CPU%, memory%, and user
Top 10 Services: Displays systemd services sorted by memory usage with status and PID
Auto-refresh: Updates every 3 seconds automatically
Manual controls: Buttons to refresh immediately, stop, or start auto-refresh
Split-panel layout: Easy side-by-side comparison
<BR>

Requirements:

Java Development Kit (JDK) 8 or higher
Linux system with ps, systemctl commands
May need to run with appropriate permissions to query some services

The app uses native Linux commands (ps aux for processes and systemctl for services) to gather real-time resource usage data and displays it in sortable tables. The process monitoring focuses on CPU usage while service monitoring tracks memory consumption.
<BR>
Install Java JDK
<BR>
Run javac LinuxResourceMonitor.java
<BR>
java LinuxResourceMonitor
<BR>

A Gui should appear with the top processes

