# memory.py - BCC-500 Simulator Memory System
"""
24-bit word memory with 18-bit addressing
Optimized for MicroPython on Raspberry Pi Pico 2
"""

class Memory:
    """
    Simulates BCC-500 memory system
    - 24-bit words
    - 18-bit addressing (262,144 max addresses)
    - Using 32K words (96KB) for Pico 2 constraints
    """
    
    def __init__(self, size=32768):
        """
        Initialize memory
        Args:
            size: Number of 24-bit words (default 32K = 32768)
        """
        if size > 262144:  # 2^18 max
            raise ValueError("Memory size exceeds 18-bit address space")
        
        self.size = size
        # Use bytearray for efficient storage (3 bytes per 24-bit word)
        self.data = bytearray(size * 3)
        print(f"Memory initialized: {size} words ({size * 3} bytes)")
    
    def read(self, addr):
        """
        Read 24-bit word from memory
        Args:
            addr: 18-bit address (0 to size-1)
        Returns:
            24-bit value
        """
        if addr < 0 or addr >= self.size:
            raise MemoryError(f"Memory read address {addr:05X} out of range (0-{self.size-1:05X})")
        
        base = addr * 3
        # Reconstruct 24-bit word from 3 bytes (big-endian)
        return (self.data[base] << 16) | (self.data[base + 1] << 8) | self.data[base + 2]
    
    def write(self, addr, value):
        """
        Write 24-bit word to memory
        Args:
            addr: 18-bit address
            value: 24-bit value to write
        """
        if addr < 0 or addr >= self.size:
            raise MemoryError(f"Memory write address {addr:05X} out of range (0-{self.size-1:05X})")
        
        # Ensure value is 24-bit
        value &= 0xFFFFFF
        
        base = addr * 3
        # Store as 3 bytes (big-endian)
        self.data[base] = (value >> 16) & 0xFF
        self.data[base + 1] = (value >> 8) & 0xFF
        self.data[base + 2] = value & 0xFF
    
    def read_range(self, start, count):
        """
        Read multiple consecutive words
        Args:
            start: Starting address
            count: Number of words to read
        Returns:
            List of values
        """
        return [self.read(start + i) for i in range(count)]
    
    def write_range(self, start, values):
        """
        Write multiple consecutive words
        Args:
            start: Starting address
            values: List of values to write
        """
        for i, value in enumerate(values):
            self.write(start + i, value)
    
    def dump(self, start, count=16):
        """
        Dump memory contents for debugging
        Args:
            start: Starting address
            count: Number of words to display
        """
        print(f"\nMemory Dump from {start:05X}:")
        for i in range(0, count, 4):
            addr = start + i
            if addr >= self.size:
                break
            
            line = f"{addr:05X}: "
            for j in range(4):
                if addr + j < self.size:
                    value = self.read(addr + j)
                    line += f"{value:06X} "
                else:
                    line += "       "
            print(line)
    
    def clear(self):
        """Clear all memory (set to zero)"""
        for i in range(len(self.data)):
            self.data[i] = 0
        print("Memory cleared")
    
    def get_stats(self):
        """Return memory statistics"""
        return {
            'size_words': self.size,
            'size_bytes': len(self.data),
            'max_address': self.size - 1
        }
