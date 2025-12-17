# BCC-500 Simulator Project Plan for Raspberry Pi Pico 2

## Simulator vs Emulator: Key Difference

**Emulator**: Mimics hardware precisely, cycle-accurate, runs original software
**Simulator**: Reproduces functionality and behavior, not cycle-accurate, educational/demonstration

This is a **much more achievable** project that focuses on demonstrating BCC-500 concepts rather than perfect hardware replication.

## Project Goals

### Primary Goals:
✓ Demonstrate multi-processor time-sharing concepts
✓ Show how six processors coordinate via shared memory
✓ Execute simple programs in BCC-500/SDS 940 style
✓ Educational tool for understanding 1960s computing
✓ Interactive demonstration of ALOHAnet-era technology

### Non-Goals:
✗ Run original BCC-500 software
✗ Cycle-accurate timing
✗ Perfect hardware fidelity
✗ Production time-sharing system

## Simplified Architecture

### What We'll Simulate:

**Memory**: 
- 32K words (96KB) - much smaller but workable
- 24-bit words
- Simple array-based storage
- No paging complexity needed

**Processors**:
- **2 User Processors** (P0, P1): Run user programs
- **1 Supervisor Processor** (P2): Represents merged OS functions
- **Optional**: Add more processors if memory/performance allows

**Instruction Set**:
- ~20-30 core instructions (not the full SDS 940 set)
- Focus on commonly used operations
- Simplified I/O

**Operating System**:
- Simple round-robin scheduler
- Basic memory protection
- Minimal system calls

## Feasibility Analysis for Pico 2

### Memory Budget:
```
Memory Array (32K * 24-bit):     96 KB
MicroPython Runtime:            ~100 KB
Simulator Code:                  ~50 KB
Buffers/Stack:                   ~20 KB
Display/I/O:                     ~10 KB
                        Total:   ~276 KB
Available on Pico 2:            520 KB
                Headroom:       ~244 KB ✓
```

### Performance Estimate:
- Simple instruction: ~100-500 Python operations
- Pico 2 at 150MHz can do ~1-5M Python ops/sec
- Expected speed: **2,000-20,000 simulated instructions/sec**
- Original BCC-500: ~500,000 instructions/sec per processor
- **Simulation speed: 0.4-4% of original** (acceptable for demonstration)

## Phase 1: Core Simulator (2-3 weeks)

### 1.1 Memory System
```python
class Memory:
    """Simplified 24-bit word memory"""
    def __init__(self, size=32768):
        # Use bytearray for efficiency (3 bytes per word)
        self.data = bytearray(size * 3)
        self.size = size
    
    def read(self, addr):
        """Read 24-bit word"""
        if addr >= self.size:
            raise MemoryError(f"Address {addr} out of range")
        base = addr * 3
        return (self.data[base] << 16) | (self.data[base+1] << 8) | self.data[base+2]
    
    def write(self, addr, value):
        """Write 24-bit word"""
        if addr >= self.size:
            raise MemoryError(f"Address {addr} out of range")
        value &= 0xFFFFFF  # Ensure 24-bit
        base = addr * 3
        self.data[base] = (value >> 16) & 0xFF
        self.data[base+1] = (value >> 8) & 0xFF
        self.data[base+2] = value & 0xFF
```

### 1.2 Processor Core
```python
class Processor:
    """Single BCC-500 processor simulator"""
    
    # Opcodes (6-bit, so 0-63)
    OP_HLT = 0x00  # Halt
    OP_LDA = 0x01  # Load A from memory
    OP_STA = 0x02  # Store A to memory
    OP_ADD = 0x03  # Add memory to A
    OP_SUB = 0x04  # Subtract memory from A
    OP_MUL = 0x05  # Multiply
    OP_DIV = 0x06  # Divide
    OP_JMP = 0x07  # Jump
    OP_JSR = 0x08  # Jump to subroutine
    OP_BRN = 0x09  # Branch (various conditions)
    OP_AND = 0x0A  # Logical AND
    OP_IOR = 0x0B  # Logical OR (IOR = Inclusive OR)
    OP_XOR = 0x0C  # Logical XOR
    OP_SKP = 0x0D  # Skip if condition
    OP_SYS = 0x0E  # System call
    # ... more opcodes
    
    def __init__(self, proc_id, memory, supervisor):
        self.id = proc_id
        self.memory = memory
        self.supervisor = supervisor
        
        # Registers
        self.A = 0      # Accumulator (24-bit)
        self.B = 0      # Extension/multiplier (24-bit)
        self.X = 0      # Index register (18-bit)
        self.PC = 0     # Program counter (18-bit)
        
        # Status
        self.running = False
        self.mode = 'USER'  # USER or SYSTEM
        self.overflow = False
        self.carry = False
        
    def step(self):
        """Execute one instruction"""
        if not self.running:
            return False
            
        # Fetch
        instruction = self.memory.read(self.PC)
        self.PC = (self.PC + 1) & 0x3FFFF  # 18-bit wrap
        
        # Decode
        opcode = (instruction >> 18) & 0x3F    # Top 6 bits
        address = instruction & 0x3FFFF         # Bottom 18 bits
        
        # Execute
        return self.execute(opcode, address)
    
    def execute(self, opcode, address):
        """Execute decoded instruction"""
        
        if opcode == self.OP_HLT:
            self.running = False
            return False
            
        elif opcode == self.OP_LDA:
            self.A = self.memory.read(address)
            
        elif opcode == self.OP_STA:
            self.memory.write(address, self.A)
            
        elif opcode == self.OP_ADD:
            result = self.A + self.memory.read(address)
            self.overflow = result > 0xFFFFFF
            self.A = result & 0xFFFFFF
            
        elif opcode == self.OP_SUB:
            operand = self.memory.read(address)
            result = self.A - operand
            self.carry = result < 0
            self.A = result & 0xFFFFFF
            
        elif opcode == self.OP_JMP:
            self.PC = address
            
        elif opcode == self.OP_JSR:
            # Save return address in A register (simplified)
            self.A = self.PC
            self.PC = address
            
        elif opcode == self.OP_AND:
            self.A &= self.memory.read(address)
            
        elif opcode == self.OP_IOR:
            self.A |= self.memory.read(address)
            
        elif opcode == self.OP_XOR:
            self.A ^= self.memory.read(address)
            
        elif opcode == self.OP_SYS:
            # System call - handled by supervisor
            self.supervisor.system_call(self, address)
            
        else:
            print(f"P{self.id}: Unknown opcode {opcode:02X} at PC={self.PC-1:05X}")
            self.running = False
            return False
        
        return True
```

### 1.3 Supervisor/Scheduler
```python
class Supervisor:
    """Manages multiple processors and system resources"""
    
    def __init__(self, memory, num_processors=3):
        self.memory = memory
        self.processors = [
            Processor(i, memory, self) 
            for i in range(num_processors)
        ]
        self.current_proc = 0
        self.time_slice = 100  # Instructions per time slice
        self.total_cycles = 0
        
    def load_program(self, proc_id, program, start_addr=0x100):
        """Load program into memory for a processor"""
        for i, instruction in enumerate(program):
            self.memory.write(start_addr + i, instruction)
        
        # Initialize processor
        proc = self.processors[proc_id]
        proc.PC = start_addr
        proc.running = True
        proc.A = 0
        proc.B = 0
        proc.X = 0
    
    def step_all(self):
        """Execute one instruction on each running processor (round-robin)"""
        any_running = False
        
        for proc in self.processors:
            if proc.running:
                proc.step()
                any_running = True
        
        self.total_cycles += 1
        return any_running
    
    def run(self, max_cycles=10000):
        """Run simulation until all processors halt or max_cycles reached"""
        cycle = 0
        while cycle < max_cycles:
            if not self.step_all():
                print(f"All processors halted after {cycle} cycles")
                break
            cycle += 1
            
            # Optional: Display status periodically
            if cycle % 1000 == 0:
                self.display_status()
        
        if cycle >= max_cycles:
            print(f"Reached maximum cycles ({max_cycles})")
    
    def display_status(self):
        """Show current processor states"""
        print(f"\n=== Cycle {self.total_cycles} ===")
        for proc in self.processors:
            status = "RUN" if proc.running else "HLT"
            print(f"P{proc.id} [{status}] PC={proc.PC:05X} A={proc.A:06X} "
                  f"B={proc.B:06X} X={proc.X:05X}")
    
    def system_call(self, proc, call_num):
        """Handle system calls from processors"""
        if call_num == 1:  # PRINT - print value in A as character
            print(chr(proc.A & 0xFF), end='')
        elif call_num == 2:  # PRINT_NUM - print value in A as number
            print(proc.A, end=' ')
        elif call_num == 3:  # YIELD - give up time slice
            pass  # In round-robin, this is automatic
        elif call_num == 4:  # EXIT - halt this processor
            proc.running = False
        else:
            print(f"P{proc.id}: Unknown system call {call_num}")
```

## Phase 2: Assembler & Tools (1-2 weeks)

### 2.1 Simple Assembler
```python
class Assembler:
    """Translate assembly language to machine code"""
    
    OPCODES = {
        'HLT': 0x00, 'LDA': 0x01, 'STA': 0x02, 'ADD': 0x03,
        'SUB': 0x04, 'MUL': 0x05, 'DIV': 0x06, 'JMP': 0x07,
        'JSR': 0x08, 'BRN': 0x09, 'AND': 0x0A, 'IOR': 0x0B,
        'XOR': 0x0C, 'SKP': 0x0D, 'SYS': 0x0E,
    }
    
    def assemble(self, source):
        """
        Assemble text to machine code
        
        Example:
            LDA 0x1000    ; Load from address 0x1000
            ADD 0x1001    ; Add value at 0x1001
            STA 0x1002    ; Store result
            HLT           ; Stop
        """
        program = []
        labels = {}
        
        # First pass: collect labels
        addr = 0
        for line in source.split('\n'):
            line = line.split(';')[0].strip()  # Remove comments
            if not line:
                continue
            
            if ':' in line:
                label = line.split(':')[0].strip()
                labels[label] = addr
            else:
                addr += 1
        
        # Second pass: generate code
        for line in source.split('\n'):
            line = line.split(';')[0].strip()
            if not line or ':' in line:
                continue
            
            parts = line.split()
            mnemonic = parts[0].upper()
            
            if mnemonic not in self.OPCODES:
                raise ValueError(f"Unknown instruction: {mnemonic}")
            
            opcode = self.OPCODES[mnemonic]
            
            # Get address operand
            if len(parts) > 1:
                operand = parts[1]
                # Handle label or numeric address
                if operand in labels:
                    address = labels[operand]
                else:
                    address = int(operand, 0)  # Auto-detect hex/dec
            else:
                address = 0
            
            # Encode: 6-bit opcode in top bits, 18-bit address in bottom
            instruction = (opcode << 18) | (address & 0x3FFFF)
            program.append(instruction)
        
        return program
```

### 2.2 Example Programs
```python
# Simple addition program
PROGRAM_ADD = """
    LDA 0x1000    ; Load first number
    ADD 0x1001    ; Add second number
    STA 0x1002    ; Store result
    HLT           ; Stop
"""

# Counting loop
PROGRAM_LOOP = """
    LDA counter   ; Load counter value
loop:
    SYS 2         ; Print number (syscall 2)
    ADD one       ; Increment
    STA counter   ; Save back
    SUB limit     ; Compare with limit
    BRN loop      ; Branch if not zero
    HLT           ; Done
    
counter: 0x000000
one:     0x000001
limit:   0x00000A  ; Count to 10
"""

# Multi-processor demo
PROGRAM_HELLO_P0 = """
    LDA msg_h
    SYS 1         ; Print 'H'
    LDA msg_e
    SYS 1         ; Print 'e'
    HLT
msg_h: 0x000048   ; 'H'
msg_e: 0x000065   ; 'e'
"""

PROGRAM_HELLO_P1 = """
    LDA msg_l1
    SYS 1         ; Print 'l'
    LDA msg_l2
    SYS 1         ; Print 'l'
    LDA msg_o
    SYS 1         ; Print 'o'
    HLT
msg_l1: 0x00006C  ; 'l'
msg_l2: 0x00006C  ; 'l'
msg_o:  0x00006F  ; 'o'
"""
```

## Phase 3: User Interface (1-2 weeks)

### 3.1 Serial Console Interface
```python
class Console:
    """Serial terminal interface for Pico 2"""
    
    def __init__(self):
        from machine import UART
        self.uart = UART(0, baudrate=115200)
        
    def print(self, text):
        """Output to terminal"""
        self.uart.write(text + '\r\n')
    
    def read_line(self):
        """Read a line from terminal"""
        line = ''
        while True:
            if self.uart.any():
                char = self.uart.read(1).decode('utf-8')
                if char == '\r' or char == '\n':
                    return line
                line += char
    
    def menu(self):
        """Interactive menu"""
        self.print("\n=== BCC-500 Simulator ===")
        self.print("1. Load and run demo program")
        self.print("2. Single step execution")
        self.print("3. View memory")
        self.print("4. View processor status")
        self.print("5. Reset system")
        self.print("q. Quit")
        self.print("\nChoice: ")
```

### 3.2 Optional: OLED Display
```python
class Display:
    """Show processor status on OLED (SSD1306)"""
    
    def __init__(self):
        from machine import I2C, Pin
        import ssd1306
        
        i2c = I2C(0, scl=Pin(1), sda=Pin(0))
        self.oled = ssd1306.SSD1306_I2C(128, 64, i2c)
    
    def show_status(self, supervisor):
        """Display current simulation state"""
        self.oled.fill(0)
        self.oled.text("BCC-500 SIM", 0, 0)
        self.oled.text(f"Cycles: {supervisor.total_cycles}", 0, 10)
        
        y = 20
        for proc in supervisor.processors:
            status = "R" if proc.running else "H"
            self.oled.text(f"P{proc.id}:{status} {proc.PC:04X}", 0, y)
            y += 10
        
        self.oled.show()
```

## Phase 4: Integration & Demo (1 week)

### 4.1 Main Simulator Program
```python
# main.py - Main simulator entry point

from memory import Memory
from processor import Processor
from supervisor import Supervisor
from assembler import Assembler
from console import Console
import time

def main():
    print("Initializing BCC-500 Simulator...")
    
    # Create system
    memory = Memory(size=32768)
    supervisor = Supervisor(memory, num_processors=3)
    assembler = Assembler()
    console = Console()
    
    # Load demo programs
    print("Loading demonstration programs...")
    
    # Processor 0: Count to 10
    prog0 = assembler.assemble(PROGRAM_LOOP)
    supervisor.load_program(0, prog0, start_addr=0x100)
    
    # Processor 1: Simple calculation
    prog1 = assembler.assemble(PROGRAM_ADD)
    supervisor.load_program(1, prog1, start_addr=0x200)
    memory.write(0x1000, 42)   # First number
    memory.write(0x1001, 17)   # Second number
    
    print("Starting simulation...")
    print("Press Ctrl+C to stop\n")
    
    try:
        supervisor.run(max_cycles=50000)
    except KeyboardInterrupt:
        print("\n\nSimulation interrupted")
    
    # Show final state
    supervisor.display_status()
    
    # Show results
    print(f"\nResult at 0x1002: {memory.read(0x1002)}")
    print(f"Total cycles executed: {supervisor.total_cycles}")

if __name__ == '__main__':
    main()
```

## Demonstration Scenarios

### Demo 1: Multi-Processor Cooperation
Three processors work together to process data:
- P0: Producer (generates data)
- P1: Consumer (processes data)
- P2: Monitor (tracks activity)

### Demo 2: Time-Sharing Simulation
Show how multiple "users" (processors) share resources:
- Each processor runs different program
- Round-robin scheduling
- Memory protection between processes

### Demo 3: Simple ALOHAnet Concept
Simulate packet transmission:
- Processors represent network nodes
- Shared memory = shared medium
- Demonstrate collision detection

## File Structure
```
/bcc500-simulator/
├── main.py              # Main program
├── memory.py            # Memory system
├── processor.py         # Processor core
├── supervisor.py        # Multi-processor coordinator
├── assembler.py         # Simple assembler
├── console.py           # Serial I/O
├── display.py           # OLED display (optional)
├── programs/            # Example programs
│   ├── hello.asm
│   ├── counter.asm
│   └── math.asm
└── README.md           # Documentation
```

## Timeline (Realistic)

**Week 1-2**: Core simulator (memory, processor, basic instructions)
**Week 3**: Assembler and test programs
**Week 4**: Multi-processor coordination
**Week 5**: User interface and polish
**Week 6**: Documentation and demo programs

**Total: 6 weeks part-time** (or 2-3 weeks full-time)

## Success Metrics

✓ Can execute basic programs (arithmetic, loops, branches)
✓ Multiple processors run concurrently
✓ Interactive console works
✓ Educational demonstrations run smoothly
✓ Code is documented and understandable
✓ Runs at reasonable speed (1000+ instructions/sec)

## Advantages of Simulator Approach

1. **Achievable**: Can be done in 4-6 weeks
2. **Educational**: Shows concepts clearly
3. **Flexible**: Easy to add features
4. **Practical**: Runs at usable speed
5. **Extensible**: Can grow over time
6. **Demonstrable**: Great for showing others

## What's Next?

Want me to:
1. Generate the actual Python code files to get started?
2. Create specific example programs?
3. Design the hardware interface (buttons, LEDs, display)?
4. Help with testing strategy?

This is a **very achievable project** that you could have running in a month or two!
