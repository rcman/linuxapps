# processor.py - BCC-500 Simulator Processor
"""
Single processor simulation with BCC-500/SDS 940 instruction set
"""

class Processor:
    """
    Simulates one BCC-500 processor
    - 24-bit word architecture
    - 6-bit opcodes, 18-bit addresses
    - Basic SDS 940-compatible instruction set
    """
    
    # Instruction opcodes (6-bit)
    OP_HLT = 0x00   # Halt processor
    OP_LDA = 0x01   # Load A from memory
    OP_STA = 0x02   # Store A to memory
    OP_ADD = 0x03   # Add memory to A
    OP_SUB = 0x04   # Subtract memory from A
    OP_MUL = 0x05   # Multiply A by memory
    OP_DIV = 0x06   # Divide A by memory
    OP_JMP = 0x07   # Unconditional jump
    OP_JSR = 0x08   # Jump to subroutine
    OP_BZE = 0x09   # Branch if A is zero
    OP_BNZ = 0x0A   # Branch if A is not zero
    OP_BPL = 0x0B   # Branch if A is positive
    OP_BMI = 0x0C   # Branch if A is negative
    OP_AND = 0x0D   # Logical AND
    OP_IOR = 0x0E   # Logical OR (Inclusive OR)
    OP_XOR = 0x0F   # Logical XOR
    OP_LDX = 0x10   # Load X register
    OP_STX = 0x11   # Store X register
    OP_LDB = 0x12   # Load B register
    OP_STB = 0x13   # Store B register
    OP_SYS = 0x14   # System call
    OP_NOP = 0x15   # No operation
    OP_CLA = 0x16   # Clear A
    OP_NOT = 0x17   # Bitwise NOT of A
    OP_SHL = 0x18   # Shift left
    OP_SHR = 0x19   # Shift right
    OP_CMP = 0x1A   # Compare (sets flags)
    OP_RTS = 0x1B   # Return from subroutine
    
    # Opcode names for debugging
    OP_NAMES = {
        0x00: 'HLT', 0x01: 'LDA', 0x02: 'STA', 0x03: 'ADD',
        0x04: 'SUB', 0x05: 'MUL', 0x06: 'DIV', 0x07: 'JMP',
        0x08: 'JSR', 0x09: 'BZE', 0x0A: 'BNZ', 0x0B: 'BPL',
        0x0C: 'BMI', 0x0D: 'AND', 0x0E: 'IOR', 0x0F: 'XOR',
        0x10: 'LDX', 0x11: 'STX', 0x12: 'LDB', 0x13: 'STB',
        0x14: 'SYS', 0x15: 'NOP', 0x16: 'CLA', 0x17: 'NOT',
        0x18: 'SHL', 0x19: 'SHR', 0x1A: 'CMP', 0x1B: 'RTS'
    }
    
    def __init__(self, proc_id, memory, supervisor):
        """
        Initialize processor
        Args:
            proc_id: Processor ID number
            memory: Memory object
            supervisor: Supervisor object for system calls
        """
        self.id = proc_id
        self.memory = memory
        self.supervisor = supervisor
        
        # Registers (all 24-bit except PC and X which are 18-bit)
        self.A = 0          # Accumulator
        self.B = 0          # Extension/multiplier register
        self.X = 0          # Index register (18-bit)
        self.PC = 0         # Program counter (18-bit)
        
        # Status flags
        self.running = False
        self.overflow = False
        self.carry = False
        self.negative = False
        self.zero = False
        
        # Statistics
        self.instructions_executed = 0
        
    def reset(self):
        """Reset processor to initial state"""
        self.A = 0
        self.B = 0
        self.X = 0
        self.PC = 0
        self.running = False
        self.overflow = False
        self.carry = False
        self.negative = False
        self.zero = False
        self.instructions_executed = 0
    
    def load_program(self, program, start_addr=0x100):
        """
        Load program into memory and initialize PC
        Args:
            program: List of 24-bit instructions
            start_addr: Starting address
        """
        for i, instruction in enumerate(program):
            self.memory.write(start_addr + i, instruction)
        self.PC = start_addr
        self.running = True
        print(f"P{self.id}: Loaded {len(program)} instructions at {start_addr:05X}")
    
    def step(self):
        """
        Execute one instruction
        Returns:
            True if continuing, False if halted
        """
        if not self.running:
            return False
        
        try:
            # Fetch
            if self.PC >= self.memory.size:
                print(f"P{self.id}: PC out of range: {self.PC:05X}")
                self.running = False
                return False
                
            instruction = self.memory.read(self.PC)
            self.PC = (self.PC + 1) & 0x3FFFF  # 18-bit wrap
            
            # Decode
            opcode = (instruction >> 18) & 0x3F     # Top 6 bits
            address = instruction & 0x3FFFF          # Bottom 18 bits
            
            # Execute
            self.execute(opcode, address)
            self.instructions_executed += 1
            
            return self.running
            
        except Exception as e:
            print(f"P{self.id}: Exception at PC={self.PC-1:05X}: {e}")
            self.running = False
            return False
    
    def execute(self, opcode, address):
        """Execute decoded instruction"""
        
        # Update flags based on A register
        def update_flags():
            self.zero = (self.A == 0)
            self.negative = (self.A & 0x800000) != 0  # Check sign bit
        
        if opcode == self.OP_HLT:
            # Halt
            self.running = False
            
        elif opcode == self.OP_LDA:
            # Load A from memory
            self.A = self.memory.read(address)
            update_flags()
            
        elif opcode == self.OP_STA:
            # Store A to memory
            self.memory.write(address, self.A)
            
        elif opcode == self.OP_ADD:
            # Add memory to A
            result = self.A + self.memory.read(address)
            self.overflow = result > 0xFFFFFF
            self.A = result & 0xFFFFFF
            update_flags()
            
        elif opcode == self.OP_SUB:
            # Subtract memory from A
            operand = self.memory.read(address)
            result = self.A - operand
            self.carry = result < 0
            self.A = result & 0xFFFFFF
            update_flags()
            
        elif opcode == self.OP_MUL:
            # Multiply (result in A, overflow in B)
            result = self.A * self.memory.read(address)
            self.B = (result >> 24) & 0xFFFFFF
            self.A = result & 0xFFFFFF
            update_flags()
            
        elif opcode == self.OP_DIV:
            # Divide A by memory
            divisor = self.memory.read(address)
            if divisor == 0:
                print(f"P{self.id}: Division by zero")
                self.overflow = True
            else:
                self.B = self.A % divisor  # Remainder in B
                self.A = self.A // divisor  # Quotient in A
                update_flags()
            
        elif opcode == self.OP_JMP:
            # Unconditional jump
            self.PC = address
            
        elif opcode == self.OP_JSR:
            # Jump to subroutine (save return address in A)
            self.A = self.PC
            self.PC = address
            
        elif opcode == self.OP_RTS:
            # Return from subroutine (get address from A)
            self.PC = self.A & 0x3FFFF
            
        elif opcode == self.OP_BZE:
            # Branch if zero
            if self.zero:
                self.PC = address
                
        elif opcode == self.OP_BNZ:
            # Branch if not zero
            if not self.zero:
                self.PC = address
                
        elif opcode == self.OP_BPL:
            # Branch if positive
            if not self.negative:
                self.PC = address
                
        elif opcode == self.OP_BMI:
            # Branch if negative
            if self.negative:
                self.PC = address
                
        elif opcode == self.OP_AND:
            # Logical AND
            self.A &= self.memory.read(address)
            update_flags()
            
        elif opcode == self.OP_IOR:
            # Logical OR
            self.A |= self.memory.read(address)
            update_flags()
            
        elif opcode == self.OP_XOR:
            # Logical XOR
            self.A ^= self.memory.read(address)
            update_flags()
            
        elif opcode == self.OP_LDX:
            # Load X register
            self.X = self.memory.read(address) & 0x3FFFF
            
        elif opcode == self.OP_STX:
            # Store X register
            self.memory.write(address, self.X)
            
        elif opcode == self.OP_LDB:
            # Load B register
            self.B = self.memory.read(address)
            
        elif opcode == self.OP_STB:
            # Store B register
            self.memory.write(address, self.B)
            
        elif opcode == self.OP_SYS:
            # System call
            self.supervisor.system_call(self, address)
            
        elif opcode == self.OP_NOP:
            # No operation
            pass
            
        elif opcode == self.OP_CLA:
            # Clear A
            self.A = 0
            update_flags()
            
        elif opcode == self.OP_NOT:
            # Bitwise NOT
            self.A = (~self.A) & 0xFFFFFF
            update_flags()
            
        elif opcode == self.OP_SHL:
            # Shift left
            self.A = (self.A << 1) & 0xFFFFFF
            update_flags()
            
        elif opcode == self.OP_SHR:
            # Shift right
            self.A = self.A >> 1
            update_flags()
            
        elif opcode == self.OP_CMP:
            # Compare (subtract but don't store result)
            result = self.A - self.memory.read(address)
            self.zero = (result == 0)
            self.negative = (result & 0x800000) != 0
            self.carry = result < 0
            
        else:
            print(f"P{self.id}: Unknown opcode {opcode:02X} at PC={self.PC-1:05X}")
            self.running = False
    
    def get_status(self):
        """Return processor status as string"""
        status = "RUN" if self.running else "HLT"
        flags = ""
        if self.zero: flags += "Z"
        if self.negative: flags += "N"
        if self.carry: flags += "C"
        if self.overflow: flags += "V"
        if not flags: flags = "-"
        
        return (f"P{self.id} [{status}] PC={self.PC:05X} A={self.A:06X} "
                f"B={self.B:06X} X={self.X:05X} [{flags}] "
                f"Instructions={self.instructions_executed}")
    
    def disassemble(self, instruction):
        """
        Disassemble instruction for display
        Returns:
            String representation
        """
        opcode = (instruction >> 18) & 0x3F
        address = instruction & 0x3FFFF
        
        op_name = self.OP_NAMES.get(opcode, f"UK{opcode:02X}")
        
        # Instructions that don't use address
        if opcode in [self.OP_HLT, self.OP_NOP, self.OP_CLA, 
                      self.OP_NOT, self.OP_SHL, self.OP_SHR, self.OP_RTS]:
            return f"{op_name}"
        else:
            return f"{op_name} {address:05X}"
