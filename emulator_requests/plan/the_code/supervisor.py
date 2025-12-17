# supervisor.py - BCC-500 Simulator Supervisor
"""
Manages multiple processors and system resources
"""

from processor import Processor
import time

class Supervisor:
    """
    Coordinates multiple processors with shared memory
    Provides system calls and scheduling
    """
    
    def __init__(self, memory, num_processors=3):
        """
        Initialize supervisor
        Args:
            memory: Memory object
            num_processors: Number of processors to simulate
        """
        self.memory = memory
        self.processors = []
        self.num_processors = num_processors
        self.total_cycles = 0
        self.start_time = 0
        
        # Create processors
        for i in range(num_processors):
            proc = Processor(i, memory, self)
            self.processors.append(proc)
        
        print(f"Supervisor initialized with {num_processors} processors")
    
    def load_program(self, proc_id, program, start_addr=0x100):
        """
        Load program for specific processor
        Args:
            proc_id: Processor ID
            program: List of instructions
            start_addr: Starting address
        """
        if proc_id >= len(self.processors):
            raise ValueError(f"Invalid processor ID: {proc_id}")
        
        self.processors[proc_id].load_program(program, start_addr)
    
    def reset_all(self):
        """Reset all processors"""
        for proc in self.processors:
            proc.reset()
        self.total_cycles = 0
        print("All processors reset")
    
    def step_all(self):
        """
        Execute one instruction on each running processor (round-robin)
        Returns:
            True if any processor is still running
        """
        any_running = False
        
        for proc in self.processors:
            if proc.running:
                proc.step()
                any_running = True
        
        self.total_cycles += 1
        return any_running
    
    def run(self, max_cycles=100000, show_progress=True):
        """
        Run simulation until all processors halt or max cycles reached
        Args:
            max_cycles: Maximum cycles to execute
            show_progress: Show periodic status updates
        """
        print(f"\n=== Starting simulation (max {max_cycles} cycles) ===\n")
        self.start_time = time.ticks_ms()
        
        cycle = 0
        last_update = 0
        update_interval = max(1, max_cycles // 20)  # Update ~20 times
        
        while cycle < max_cycles:
            if not self.step_all():
                print(f"\n✓ All processors halted after {cycle} cycles")
                break
            
            cycle += 1
            
            # Periodic status update
            if show_progress and (cycle - last_update >= update_interval):
                self._show_progress(cycle, max_cycles)
                last_update = cycle
        
        if cycle >= max_cycles:
            print(f"\n⚠ Reached maximum cycles ({max_cycles})")
        
        self._show_final_stats()
    
    def run_cycles(self, count):
        """
        Run specific number of cycles
        Args:
            count: Number of cycles to execute
        """
        for _ in range(count):
            if not self.step_all():
                break
    
    def _show_progress(self, current, maximum):
        """Show progress bar and status"""
        percent = (current * 100) // maximum
        bar_length = 20
        filled = (percent * bar_length) // 100
        bar = '█' * filled + '░' * (bar_length - filled)
        
        print(f"\r[{bar}] {percent}% ({current}/{maximum})", end='')
    
    def _show_final_stats(self):
        """Show final statistics"""
        elapsed = time.ticks_diff(time.ticks_ms(), self.start_time)
        
        print(f"\n{'='*60}")
        print(f"Simulation Statistics:")
        print(f"{'='*60}")
        print(f"Total cycles:          {self.total_cycles}")
        print(f"Elapsed time:          {elapsed} ms")
        
        if elapsed > 0:
            cycles_per_sec = (self.total_cycles * 1000) // elapsed
            print(f"Simulation speed:      {cycles_per_sec} cycles/sec")
        
        total_instructions = sum(p.instructions_executed for p in self.processors)
        print(f"Total instructions:    {total_instructions}")
        print(f"{'='*60}\n")
    
    def display_status(self):
        """Display current status of all processors"""
        print(f"\n{'='*70}")
        print(f"Cycle {self.total_cycles} - Processor Status:")
        print(f"{'='*70}")
        
        for proc in self.processors:
            print(proc.get_status())
        
        print(f"{'='*70}\n")
    
    def system_call(self, proc, call_num):
        """
        Handle system calls from processors
        Args:
            proc: Calling processor
            call_num: System call number
        """
        if call_num == 0:
            # SYS 0: No operation
            pass
            
        elif call_num == 1:
            # SYS 1: Print character in A
            char = chr(proc.A & 0xFF)
            print(char, end='')
            
        elif call_num == 2:
            # SYS 2: Print number in A (decimal)
            print(proc.A, end=' ')
            
        elif call_num == 3:
            # SYS 3: Print number in A (hex)
            print(f"{proc.A:06X}", end=' ')
            
        elif call_num == 4:
            # SYS 4: Print newline
            print()
            
        elif call_num == 5:
            # SYS 5: Halt this processor
            proc.running = False
            
        elif call_num == 6:
            # SYS 6: Get processor ID (return in A)
            proc.A = proc.id
            
        elif call_num == 7:
            # SYS 7: Get cycle count (return in A)
            proc.A = self.total_cycles & 0xFFFFFF
            
        elif call_num == 8:
            # SYS 8: Yield (give up time slice - no-op in round-robin)
            pass
            
        elif call_num == 9:
            # SYS 9: Sleep (skip N cycles, N in address field)
            # For now, just a no-op
            pass
            
        elif call_num == 10:
            # SYS 10: Print processor status
            print(f"\n{proc.get_status()}")
            
        else:
            print(f"\nP{proc.id}: Unknown system call {call_num}")
    
    def get_running_count(self):
        """Return number of processors still running"""
        return sum(1 for p in self.processors if p.running)
    
    def halt_all(self):
        """Halt all processors"""
        for proc in self.processors:
            proc.running = False
        print("All processors halted")
    
    def interactive_debug(self):
        """Interactive debugging interface"""
        print("\n=== Interactive Debug Mode ===")
        print("Commands: s=step, r=run, d=display, m=memory, q=quit")
        
        while True:
            cmd = input("\nDebug> ").strip().lower()
            
            if cmd == 's':
                # Single step
                if self.step_all():
                    self.display_status()
                else:
                    print("All processors halted")
                    
            elif cmd == 'r':
                # Run
                cycles = input("How many cycles? (or 'a' for all): ").strip()
                if cycles == 'a':
                    self.run(show_progress=True)
                else:
                    try:
                        self.run(max_cycles=int(cycles), show_progress=False)
                    except ValueError:
                        print("Invalid number")
                        
            elif cmd == 'd':
                # Display status
                self.display_status()
                
            elif cmd == 'm':
                # Memory dump
                try:
                    addr = input("Start address (hex): ").strip()
                    addr = int(addr, 16)
                    count = input("Word count (default 16): ").strip()
                    count = int(count) if count else 16
                    self.memory.dump(addr, count)
                except ValueError:
                    print("Invalid input")
                    
            elif cmd == 'q':
                # Quit
                break
                
            else:
                print(f"Unknown command: {cmd}")
