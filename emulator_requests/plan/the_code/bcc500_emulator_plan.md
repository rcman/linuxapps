# BCC-500 Emulator Project Plan for Raspberry Pi Pico 2

## Executive Summary

This document outlines a comprehensive plan for creating a BCC-500 emulator on the Raspberry Pi Pico 2 using MicroPython. This is an **extremely ambitious project** that will require significant research, development time, and likely some compromises.

## System Overview

### BCC-500 Architecture
- **Six independent microcoded processors** running concurrently
- **24-bit word width** with 18-bit addressing (262,144 words = 786KB)
- **6-bit opcode** + 18-bit address instruction format
- **Shared memory architecture** between all six processors
- **Mixed microcode/macrocode operating system**
- Implements subset of SDS 940 instruction set
- Multi-processing coordination between processors

### Target Platform: Raspberry Pi Pico 2
- **CPU**: Dual ARM Cortex-M33 cores @ 150MHz
- **RAM**: 520KB SRAM
- **Flash**: 4MB
- **Language**: MicroPython (interpreted)

## Major Challenges

### 1. Memory Constraints
**Problem**: BCC-500 needs ~786KB just for main memory
**Pico 2 has**: 520KB total SRAM (MicroPython uses significant portion)

**Possible Solutions**:
- Implement paging/swapping to external storage (Flash, SD card)
- Reduce emulated memory size (32K words = 96KB)
- Use compression for inactive memory pages
- Implement sparse memory allocation

### 2. Multi-Processor Emulation
**Problem**: Emulating 6 concurrent processors on 2 cores in interpreted Python

**Possible Solutions**:
- Time-slicing approach (round-robin between processors)
- Event-driven simulation (process only when needed)
- Simplified model (reduce to 2-3 functional processors)
- Use native code (C extensions) for critical paths

### 3. Performance
**Problem**: Interpreted MicroPython is 10-100x slower than native code

**Possible Solutions**:
- Write core emulation engine in C/C++ with MicroPython wrapper
- Use @micropython.native and @micropython.viper decorators
- Implement JIT-like optimization for hot code paths
- Consider dropping MicroPython entirely for C/C++

### 4. Documentation Gaps
**Problem**: Limited detailed technical documentation on BCC-500 specifics

**Resources Available**:
- SDS 940 documentation (subset compatibility)
- Bitsavers archives (wiring diagrams, some memos)
- Butler Lampson's lectures and papers
- University of Hawaii ALOHAnet documentation

## Phase 1: Research & Documentation (Est. 2-4 weeks)

### Tasks:
1. **Gather SDS 940 Documentation**
   - Instruction set manual
   - Assembler reference
   - Operating system documentation
   - Example programs

2. **Study BCC-500 Specific Materials**
   - Download all available documents from Bitsavers
   - Study wiring diagrams at Computer History Museum
   - Read Butler Lampson's papers on BCC architecture
   - Contact retro computing communities for additional info

3. **Understand Microcode Architecture**
   - How microcode implements macrocode instructions
   - Processor coordination mechanisms
   - Memory arbitration between processors

4. **Find Test Software**
   - Locate or create simple test programs
   - Identify diagnostic routines
   - Find bootloader/monitor code

### Key Resources:
- http://www.bitsavers.org/pdf/bcc/
- http://www.bitsavers.org/pdf/univOfHawaii/Aloha_BCC-500/
- http://www.bitsavers.org/pdf/sds/9xx/940/
- Butler Lampson's publications: http://bwlampson.site/

## Phase 2: Prototype Core Components (Est. 4-6 weeks)

### 2.1 Memory System
```python
class BCC500Memory:
    """
    24-bit words, 18-bit addressing
    Implement with paging/compression for Pico 2's limited RAM
    """
    def __init__(self, size_words=32768):  # Start with 32K words = 96KB
        pass
    
    def read_word(self, address):
        pass
    
    def write_word(self, address, value):
        pass
```

### 2.2 Single Processor Core
```python
class BCC500Processor:
    """
    Single processor emulation
    24-bit registers, 6-bit opcode decoder
    """
    def __init__(self, proc_id, memory):
        self.proc_id = proc_id
        self.memory = memory
        self.registers = {
            'A': 0,  # Accumulator
            'B': 0,  # Extension
            'X': 0,  # Index
            'PC': 0, # Program Counter (18-bit)
        }
        self.running = False
    
    def fetch_decode_execute(self):
        """Single instruction cycle"""
        pass
    
    def execute_instruction(self, opcode, address):
        """Execute one instruction"""
        pass
```

### 2.3 Instruction Set Implementation
Start with basic SDS 940 instructions:
- Load/Store operations
- Arithmetic (ADD, SUB, MUL, DIV)
- Logical operations (AND, OR, XOR)
- Branches and jumps
- System calls (simplified)

### 2.4 Multi-Processor Coordinator
```python
class BCC500System:
    """
    Coordinates six processors with shared memory
    """
    def __init__(self):
        self.memory = BCC500Memory()
        self.processors = [
            BCC500Processor(i, self.memory) 
            for i in range(6)
        ]
    
    def step(self):
        """Execute one instruction on each processor (round-robin)"""
        pass
    
    def run(self, cycles=None):
        """Run simulation"""
        pass
```

## Phase 3: Optimization & Practicality (Est. 3-4 weeks)

### 3.1 Performance Optimization
- Profile code to find bottlenecks
- Convert critical sections to native code
- Implement instruction caching
- Optimize memory access patterns

### 3.2 Compromise Decisions
You'll likely need to choose:

**Option A: Faithful Emulation** (slow but accurate)
- Full 6-processor simulation
- Complete instruction set
- Accurate timing
- Speed: ~1-10% of original

**Option B: Functional Emulation** (faster, less accurate)
- Simplified to 2-3 processors
- Core instruction subset only
- Approximate timing
- Speed: ~10-50% of original

**Option C: Hybrid Approach** (recommended)
- 2 user processors (full featured)
- 4 OS processors (simplified/merged)
- Essential instructions only
- Event-based timing
- Speed: ~20-30% of original

### 3.3 Testing Framework
```python
def test_instruction(opcode, operands, expected_result):
    """Unit test for single instruction"""
    pass

def run_test_program(program):
    """Integration test for program execution"""
    pass
```

## Phase 4: I/O and Integration (Est. 2-3 weeks)

### 4.1 Serial Terminal Interface
- UART communication for console I/O
- VT100-compatible terminal emulation
- File transfer protocols

### 4.2 Storage Interface
- SD card for program/data storage
- Flash file system for emulator state
- Save/restore functionality

### 4.3 Front Panel (Optional)
- LED indicators for processor status
- Buttons for control (run/stop/reset/step)
- OLED display for debug info

## Phase 5: Software and Testing (Est. 3-4 weeks)

### 5.1 Assembler/Loader
- Simple assembler for BCC-500/SDS 940 code
- Binary loader for tape/disk images
- Symbol table support

### 5.2 Monitor Program
- Minimal operating system kernel
- Memory dump/modify commands
- Register inspection
- Simple debugger

### 5.3 Test Programs
- "Hello World" equivalent
- Math operations test
- Memory test
- Multi-processor coordination test

## Realistic Assessment

### What You CAN Achieve:
✓ Basic single-processor SDS 940 emulation
✓ Core instruction set execution
✓ Simple I/O via serial terminal
✓ Small test programs running
✓ Educational/demonstration value

### What Will Be VERY Difficult:
⚠ Full 6-processor concurrent operation
⚠ Real-time performance
⚠ Complete instruction set + microcode
⚠ Original ALOHAnet software compatibility
⚠ Accurate hardware timing

### What Is Likely IMPOSSIBLE in MicroPython:
✗ Running original operating system at usable speed
✗ Full 256K word address space
✗ Cycle-accurate emulation
✗ Supporting 32 simultaneous users

## Alternative Approaches

### Consider These Instead:

**1. Use C/C++ Instead of MicroPython**
- 10-100x performance improvement
- Better memory control
- Still challenging but more feasible

**2. Target More Powerful Hardware**
- Raspberry Pi 4/5 (1-8GB RAM, 1.5-2.4GHz)
- Desktop PC (unlimited resources)
- FPGA implementation (hardware-accurate)

**3. Simplify the Target**
- Emulate single SDS 940 processor only
- Create "inspired by" system, not exact replica
- Focus on ALOHAnet protocols rather than hardware

**4. Use Existing Emulators**
- SIMH has SDS 940 support
- Study/extend existing work
- Contribute to preservation community

## Required Skills

To complete this project, you'll need:
- Deep understanding of computer architecture
- Assembly language programming
- Python/MicroPython proficiency
- C/C++ for optimization (likely)
- Debugging complex systems
- Digital electronics (for I/O)
- Significant patience and persistence

## Estimated Timeline

**Minimum Viable Emulator**: 3-4 months full-time
**Functional Multi-Processor**: 6-9 months full-time
**Production Quality**: 12+ months full-time

## Recommended First Steps

1. **Start with SDS 940 emulation** (simpler, better documented)
2. **Use desktop Python first** (faster development, easier debugging)
3. **Build instruction by instruction** (test each one thoroughly)
4. **Create comprehensive test suite** (essential for complex system)
5. **Join retro computing communities** (get help, share progress)
6. **Document everything** (you're doing original research here)

## Success Criteria

**Milestone 1**: Single instruction executes correctly
**Milestone 2**: Simple program (add two numbers) runs
**Milestone 3**: Full core instruction set working
**Milestone 4**: Multi-instruction programs execute
**Milestone 5**: Basic I/O functional
**Milestone 6**: Multiple processors coordinate
**Milestone 7**: Small operating system kernel boots

## Conclusion

This is a **massive undertaking** that pushes the boundaries of what's possible on the Pico 2 with MicroPython. Consider it a multi-month to multi-year project.

However, even a simplified emulator would be:
- A significant technical achievement
- Valuable for computer history preservation
- An excellent learning experience
- Potentially the only BCC-500 emulator in existence

**My strong recommendation**: Start with a desktop-based emulator in Python, then port the working core to MicroPython/Pico 2 once you understand all the challenges. Or better yet, write it in C/C++ from the start.

Are you ready for this challenge?
