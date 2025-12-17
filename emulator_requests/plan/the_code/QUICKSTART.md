# BCC-500 Simulator - Quick Start Guide

## Get Running in 5 Minutes!

### Step 1: Prepare Your Pico 2 (2 minutes)

1. Download MicroPython for Pico 2:
   - Go to https://micropython.org/download/RPI_PICO2/
   - Download the latest .uf2 file

2. Install MicroPython:
   - Hold BOOTSEL button on Pico 2
   - Plug into USB while holding button
   - Drag .uf2 file to RPI-RP2 drive
   - Pico will reboot automatically

### Step 2: Upload Files (2 minutes)

**Using Thonny (Easiest):**

1. Install Thonny: https://thonny.org/
2. Open Thonny
3. Go to Tools → Options → Interpreter
4. Select "MicroPython (Raspberry Pi Pico)"
5. Click OK
6. Open each .py file and click "Save to Raspberry Pi Pico"
   - memory.py
   - processor.py
   - supervisor.py
   - assembler.py
   - main.py

**Using Command Line:**

```bash
# Install mpremote
pip install mpremote

# Upload files
mpremote cp memory.py :
mpremote cp processor.py :
mpremote cp supervisor.py :
mpremote cp assembler.py :
mpremote cp main.py :
```

### Step 3: Run It! (30 seconds)

**In Thonny:**
1. Click the "Stop/Restart backend" button (or press Ctrl+D)
2. You'll see the BCC-500 banner and menu!

**In Terminal:**
```bash
# Connect
screen /dev/ttyACM0 115200

# Press Ctrl+D to reset
# Menu appears!
```

### Step 4: Try a Demo (30 seconds)

At the menu, press:
- **1** for Hello World
- **2** for Counting
- **5** for Multi-Processor demo

Press Enter after each choice to return to menu.

## What You'll See

```
============================================================
    BCC-500 TIMESHARING SYSTEM SIMULATOR
    Berkeley Computer Corporation
    Raspberry Pi Pico 2 Implementation
============================================================

Running on Raspberry Pi Pico 2
Initializing system...
Free memory: 245,760 bytes

============================================================
BCC-500 SIMULATOR - Main Menu
============================================================
1. Demo 1: Hello World
2. Demo 2: Counting Loop
3. Demo 3: Simple Addition
4. Demo 4: Fibonacci Sequence
5. Demo 5: Multi-Processor Coordination
6. Run Custom Program
7. Run All Demos
8. System Information
Q. Quit
============================================================

Select option: 
```

## Try Your Own Program

Select option **6** and enter:

```
LDA 0x100
ADD 0x101
STA 0x102
SYS 2
HLT
```

Then press Enter twice. The program adds two numbers!

## Common Issues

**"ModuleNotFoundError"**
- One or more .py files not uploaded
- Re-upload all 5 files

**"Out of memory"**
- Press Ctrl+D to reset
- Should free up memory

**No serial connection**
- Linux: Try /dev/ttyACM0 or /dev/ttyUSB0
- Windows: Check Device Manager for COM port
- Mac: Try /dev/cu.usbmodem*

**Pico not responding**
- Unplug and replug USB
- Hold BOOTSEL and press Reset button
- Re-upload MicroPython if needed

## Next Steps

- Read the full README.md for assembly language guide
- Try modifying the example programs
- Create your own multi-processor demos
- Explore the instruction set

## Files You Need

All files are in the output directory:

1. **memory.py** - Memory system (96 KB simulated)
2. **processor.py** - CPU simulation with ~28 instructions
3. **supervisor.py** - Multi-processor coordinator
4. **assembler.py** - Assembly language compiler + examples
5. **main.py** - Main program with demos and menu
6. **README.md** - Complete documentation

## Performance Expectations

- Runs at ~2,000-20,000 instructions/second
- Fast enough to watch programs execute
- Perfect for learning and demonstrations
- Not fast enough for "production" use (that's OK!)

## Have Fun!

You're now running a simulator of a historic 1960s multi-processor timesharing system on a $4 microcontroller. Pretty cool! 🎉

The original BCC-500 was the size of a refrigerator and cost hundreds of thousands of dollars. Your Pico 2 is more powerful and fits in your pocket.

Enjoy exploring computer history! 🚀
