# main.py - BCC-500 Simulator Main Program
"""
Main entry point for BCC-500 simulator on Raspberry Pi Pico 2
Run this file to start the simulation
"""

from memory import Memory
from supervisor import Supervisor
from assembler import Assembler, EXAMPLE_HELLO, EXAMPLE_COUNT, EXAMPLE_ADD, EXAMPLE_FIBONACCI, EXAMPLE_MULTIPROC
import time

def print_banner():
    """Display startup banner"""
    print("\n" + "="*60)
    print("    BCC-500 TIMESHARING SYSTEM SIMULATOR")
    print("    Berkeley Computer Corporation")
    print("    Raspberry Pi Pico 2 Implementation")
    print("="*60)
    print()

def run_demo_1():
    """Demo 1: Hello World on single processor"""
    print("\n=== DEMO 1: Hello World ===\n")
    
    # Initialize system
    memory = Memory(size=8192)
    supervisor = Supervisor(memory, num_processors=1)
    assembler = Assembler()
    
    # Assemble and load program
    program = assembler.assemble(EXAMPLE_HELLO, base_addr=0x100)
    supervisor.load_program(0, program, start_addr=0x100)
    
    # Run
    print("Output:")
    supervisor.run(max_cycles=1000, show_progress=False)
    supervisor.display_status()

def run_demo_2():
    """Demo 2: Counting loop"""
    print("\n=== DEMO 2: Counting Loop ===\n")
    
    memory = Memory(size=8192)
    supervisor = Supervisor(memory, num_processors=1)
    assembler = Assembler()
    
    program = assembler.assemble(EXAMPLE_COUNT, base_addr=0x100)
    supervisor.load_program(0, program, start_addr=0x100)
    
    print("Output:")
    supervisor.run(max_cycles=5000, show_progress=False)

def run_demo_3():
    """Demo 3: Simple arithmetic"""
    print("\n=== DEMO 3: Addition (42 + 17) ===\n")
    
    memory = Memory(size=8192)
    supervisor = Supervisor(memory, num_processors=1)
    assembler = Assembler()
    
    program = assembler.assemble(EXAMPLE_ADD, base_addr=0x100)
    supervisor.load_program(0, program, start_addr=0x100)
    
    print("Output:")
    supervisor.run(max_cycles=500, show_progress=False)
    
    # Show the memory location with the result
    print(f"Result stored at memory location 0x{0x100 + len(program) - 1:05X}")

def run_demo_4():
    """Demo 4: Fibonacci sequence"""
    print("\n=== DEMO 4: Fibonacci Sequence ===\n")
    
    memory = Memory(size=8192)
    supervisor = Supervisor(memory, num_processors=1)
    assembler = Assembler()
    
    program = assembler.assemble(EXAMPLE_FIBONACCI, base_addr=0x100)
    supervisor.load_program(0, program, start_addr=0x100)
    
    print("Output (first 10 Fibonacci numbers):")
    supervisor.run(max_cycles=10000, show_progress=False)

def run_demo_5():
    """Demo 5: Multi-processor coordination"""
    print("\n=== DEMO 5: Multi-Processor Demo ===\n")
    print("Three processors each print their ID...\n")
    
    memory = Memory(size=8192)
    supervisor = Supervisor(memory, num_processors=3)
    assembler = Assembler()
    
    # All processors run the same program
    program = assembler.assemble(EXAMPLE_MULTIPROC, base_addr=0x100)
    
    # Load program for each processor at different addresses
    supervisor.load_program(0, program, start_addr=0x100)
    supervisor.load_program(1, program, start_addr=0x200)
    supervisor.load_program(2, program, start_addr=0x300)
    
    print("Output (processor IDs):")
    supervisor.run(max_cycles=500, show_progress=False)
    print()
    
    supervisor.display_status()

def run_custom_program():
    """Run a custom program entered by user"""
    print("\n=== Custom Program Entry ===")
    print("Enter assembly code (end with empty line or 'END'):")
    print("Example:")
    print("  LDA 0x100")
    print("  ADD 0x101")
    print("  STA 0x102")
    print("  HLT")
    print()
    
    lines = []
    while True:
        try:
            line = input("> ")
            if not line or line.upper() == 'END':
                break
            lines.append(line)
        except:
            break
    
    if not lines:
        print("No program entered")
        return
    
    source = '\n'.join(lines)
    
    memory = Memory(size=8192)
    supervisor = Supervisor(memory, num_processors=1)
    assembler = Assembler()
    
    program = assembler.assemble(source, base_addr=0x100)
    
    if not program:
        print("Assembly failed")
        return
    
    supervisor.load_program(0, program, start_addr=0x100)
    
    print("\nRunning program...")
    supervisor.run(max_cycles=10000, show_progress=True)
    supervisor.display_status()

def interactive_menu():
    """Interactive menu for running demos"""
    while True:
        print("\n" + "="*60)
        print("BCC-500 SIMULATOR - Main Menu")
        print("="*60)
        print("1. Demo 1: Hello World")
        print("2. Demo 2: Counting Loop")
        print("3. Demo 3: Simple Addition")
        print("4. Demo 4: Fibonacci Sequence")
        print("5. Demo 5: Multi-Processor Coordination")
        print("6. Run Custom Program")
        print("7. Run All Demos")
        print("8. System Information")
        print("Q. Quit")
        print("="*60)
        
        try:
            choice = input("\nSelect option: ").strip().upper()
            
            if choice == '1':
                run_demo_1()
            elif choice == '2':
                run_demo_2()
            elif choice == '3':
                run_demo_3()
            elif choice == '4':
                run_demo_4()
            elif choice == '5':
                run_demo_5()
            elif choice == '6':
                run_custom_program()
            elif choice == '7':
                run_demo_1()
                run_demo_2()
                run_demo_3()
                run_demo_4()
                run_demo_5()
            elif choice == '8':
                show_system_info()
            elif choice == 'Q':
                print("\nGoodbye!")
                break
            else:
                print(f"Invalid option: {choice}")
                
            input("\nPress Enter to continue...")
            
        except KeyboardInterrupt:
            print("\n\nInterrupted")
            break
        except Exception as e:
            print(f"\nError: {e}")
            import sys
            sys.print_exception(e)

def show_system_info():
    """Display system information"""
    import gc
    import sys
    
    print("\n" + "="*60)
    print("System Information")
    print("="*60)
    
    # Memory info
    gc.collect()
    free = gc.mem_free()
    allocated = gc.mem_alloc()
    total = free + allocated
    
    print(f"Python Memory:")
    print(f"  Total:     {total:,} bytes")
    print(f"  Used:      {allocated:,} bytes ({allocated*100//total}%)")
    print(f"  Free:      {free:,} bytes ({free*100//total}%)")
    
    print(f"\nPython Version: {sys.version}")
    print(f"Platform: {sys.platform}")
    
    print("\nSimulator Configuration:")
    print(f"  Default Memory Size: 32,768 words (96 KB)")
    print(f"  Word Size: 24 bits")
    print(f"  Address Space: 18 bits (262,144 words max)")
    print(f"  Default Processors: 3")
    print(f"  Instruction Set: ~28 opcodes")
    
    print("="*60)

def main():
    """Main entry point"""
    print_banner()
    
    # Check if we're running on Pico
    import sys
    if sys.platform == 'rp2':
        print("Running on Raspberry Pi Pico 2")
    else:
        print(f"Running on {sys.platform}")
    
    print("\nInitializing system...")
    
    # Garbage collection
    import gc
    gc.collect()
    
    # Show memory status
    free = gc.mem_free()
    print(f"Free memory: {free:,} bytes\n")
    
    # Start interactive menu
    try:
        interactive_menu()
    except Exception as e:
        print(f"\nFatal error: {e}")
        import sys
        sys.print_exception(e)

# Auto-run on import
if __name__ == '__main__':
    main()
