# assembler.py - BCC-500 Simulator Assembler
"""
Simple assembler for BCC-500 machine language
Translates assembly code to 24-bit machine instructions
"""

class Assembler:
    """
    Assembles BCC-500 assembly language into machine code
    
    Format: OPCODE ADDRESS  ; comment
    Labels: LABEL:
    Data: value (hex or decimal)
    """
    
    # Opcode mnemonics to numeric values
    OPCODES = {
        'HLT': 0x00, 'LDA': 0x01, 'STA': 0x02, 'ADD': 0x03,
        'SUB': 0x04, 'MUL': 0x05, 'DIV': 0x06, 'JMP': 0x07,
        'JSR': 0x08, 'BZE': 0x09, 'BNZ': 0x0A, 'BPL': 0x0B,
        'BMI': 0x0C, 'AND': 0x0D, 'IOR': 0x0E, 'XOR': 0x0F,
        'LDX': 0x10, 'STX': 0x11, 'LDB': 0x12, 'STB': 0x13,
        'SYS': 0x14, 'NOP': 0x15, 'CLA': 0x16, 'NOT': 0x17,
        'SHL': 0x18, 'SHR': 0x19, 'CMP': 0x1A, 'RTS': 0x1B,
    }
    
    # Instructions that don't take an address operand
    NO_OPERAND = {'HLT', 'NOP', 'CLA', 'NOT', 'SHL', 'SHR', 'RTS'}
    
    def __init__(self):
        self.labels = {}
        self.errors = []
    
    def assemble(self, source, base_addr=0):
        """
        Assemble source code to machine code
        Args:
            source: Assembly source code (string)
            base_addr: Base address for labels
        Returns:
            List of 24-bit instructions
        """
        self.labels = {}
        self.errors = []
        
        lines = source.split('\n')
        
        # First pass: collect labels and count instructions
        addr = base_addr
        parsed_lines = []
        
        for line_num, line in enumerate(lines, 1):
            original_line = line
            
            # Remove comments
            if ';' in line:
                line = line[:line.index(';')]
            
            line = line.strip()
            
            # Skip empty lines
            if not line:
                continue
            
            # Handle labels
            if ':' in line:
                parts = line.split(':', 1)
                label = parts[0].strip()
                self.labels[label] = addr
                line = parts[1].strip() if len(parts) > 1 else ''
                
                if not line:
                    continue
            
            # Parse instruction or data
            parts = line.split()
            if parts:
                mnemonic = parts[0].upper()
                
                # Check if it's data (numeric value)
                if mnemonic.startswith('0X') or mnemonic.isdigit() or \
                   (mnemonic.startswith('-') and mnemonic[1:].isdigit()):
                    # It's data, not an instruction
                    parsed_lines.append({
                        'type': 'data',
                        'line_num': line_num,
                        'value': mnemonic
                    })
                    addr += 1
                elif mnemonic in self.OPCODES:
                    # It's an instruction
                    operand = parts[1] if len(parts) > 1 else None
                    parsed_lines.append({
                        'type': 'instruction',
                        'line_num': line_num,
                        'mnemonic': mnemonic,
                        'operand': operand
                    })
                    addr += 1
                else:
                    self.errors.append(f"Line {line_num}: Unknown instruction '{mnemonic}'")
        
        # Second pass: generate machine code
        program = []
        
        for item in parsed_lines:
            if item['type'] == 'data':
                # Parse data value
                try:
                    value = self._parse_value(item['value'])
                    program.append(value & 0xFFFFFF)
                except ValueError as e:
                    self.errors.append(f"Line {item['line_num']}: {e}")
                    program.append(0)
                    
            else:  # instruction
                mnemonic = item['mnemonic']
                operand = item['operand']
                opcode = self.OPCODES[mnemonic]
                
                # Get address operand
                if mnemonic in self.NO_OPERAND:
                    address = 0
                else:
                    if operand is None:
                        self.errors.append(
                            f"Line {item['line_num']}: {mnemonic} requires an operand"
                        )
                        address = 0
                    else:
                        try:
                            address = self._parse_operand(operand)
                        except ValueError as e:
                            self.errors.append(f"Line {item['line_num']}: {e}")
                            address = 0
                
                # Encode: 6-bit opcode in bits 18-23, 18-bit address in bits 0-17
                instruction = (opcode << 18) | (address & 0x3FFFF)
                program.append(instruction)
        
        if self.errors:
            print("\n=== Assembly Errors ===")
            for error in self.errors:
                print(f"  {error}")
            print()
        
        return program
    
    def _parse_operand(self, operand):
        """
        Parse operand - could be label, hex, or decimal
        Returns:
            18-bit address value
        """
        # Check if it's a label
        if operand in self.labels:
            return self.labels[operand] & 0x3FFFF
        
        # Try to parse as number
        return self._parse_value(operand) & 0x3FFFF
    
    def _parse_value(self, value_str):
        """
        Parse numeric value (hex or decimal)
        Returns:
            Integer value
        """
        value_str = value_str.strip()
        
        # Hex number
        if value_str.startswith('0x') or value_str.startswith('0X'):
            return int(value_str, 16)
        
        # Binary number
        if value_str.startswith('0b') or value_str.startswith('0B'):
            return int(value_str, 2)
        
        # Decimal number
        return int(value_str, 10)
    
    def disassemble(self, program, base_addr=0):
        """
        Disassemble machine code to assembly
        Args:
            program: List of 24-bit instructions
            base_addr: Base address for display
        Returns:
            Assembly source code (string)
        """
        from processor import Processor
        
        lines = []
        proc = Processor(0, None, None)  # Dummy processor for disassembly
        
        for i, instruction in enumerate(program):
            addr = base_addr + i
            disasm = proc.disassemble(instruction)
            lines.append(f"{addr:05X}: {instruction:06X}  {disasm}")
        
        return '\n'.join(lines)
    
    def get_label_address(self, label):
        """Get address of a label"""
        return self.labels.get(label, None)


# Example assembly programs
EXAMPLE_HELLO = """
; Print "Hello" using system calls
    LDA msg_h
    SYS 1         ; Print 'H'
    LDA msg_e
    SYS 1         ; Print 'e'
    LDA msg_l
    SYS 1         ; Print 'l'
    SYS 1         ; Print 'l' again
    LDA msg_o
    SYS 1         ; Print 'o'
    SYS 4         ; Print newline
    HLT

msg_h: 0x000048   ; 'H'
msg_e: 0x000065   ; 'e'
msg_l: 0x00006C   ; 'l'
msg_o: 0x00006F   ; 'o'
"""

EXAMPLE_COUNT = """
; Count from 1 to 10
    LDA start
    STA counter

loop:
    LDA counter
    SYS 2         ; Print number
    SYS 4         ; Print newline
    ADD one
    STA counter
    CMP limit
    BNZ loop      ; Loop if not equal
    HLT

start:   0x000001
one:     0x000001
counter: 0x000000
limit:   0x00000A  ; 10
"""

EXAMPLE_ADD = """
; Add two numbers
    LDA num1
    ADD num2
    STA result
    SYS 2         ; Print result
    SYS 4         ; Newline
    HLT

num1:   0x00002A  ; 42
num2:   0x000011  ; 17
result: 0x000000
"""

EXAMPLE_FIBONACCI = """
; Calculate Fibonacci sequence
    LDA fib_a
    STA current_a
    LDA fib_b
    STA current_b
    
    LDA count
    STA counter

fib_loop:
    LDA current_a
    SYS 2         ; Print number
    SYS 4         ; Newline
    
    ADD current_b
    STA temp
    
    LDA current_b
    STA current_a
    
    LDA temp
    STA current_b
    
    LDA counter
    SUB one
    STA counter
    BNZ fib_loop
    
    HLT

fib_a:     0x000000
fib_b:     0x000001
current_a: 0x000000
current_b: 0x000000
temp:      0x000000
counter:   0x000000
count:     0x00000A  ; Calculate 10 numbers
one:       0x000001
"""

EXAMPLE_MULTIPROC = """
; Multi-processor demo - each processor prints its ID
    SYS 6         ; Get processor ID into A
    SYS 2         ; Print ID
    LDA space
    SYS 1         ; Print space
    HLT

space: 0x000020   ; Space character
"""
