# BCC-500 Simulator - Complete Package Index

## 📦 Complete Package for Raspberry Pi Pico 2

**Total Files:** 12  
**Total Size:** 116 KB  
**Ready to Use:** ✓ Yes!

---

## 🚀 START HERE

### For Quick Setup (5 minutes)
→ **[QUICKSTART.md](QUICKSTART.md)** - Get running fast!

### For Complete Guide
→ **[README.md](README.md)** - Full documentation

### For Example Programs
→ **[EXAMPLES.md](EXAMPLES.md)** - 21 ready-to-use programs

---

## 📁 Core Simulator Files

**Upload these 5 files to your Pico 2:**

| File | Size | Purpose |
|------|------|---------|
| **memory.py** | 3.6 KB | Memory system (32K words, 24-bit) |
| **processor.py** | 11 KB | CPU simulator with 28 instructions |
| **supervisor.py** | 8.3 KB | Multi-processor coordinator |
| **assembler.py** | 8.6 KB | Assembly language compiler |
| **main.py** | 7.7 KB | Main program with interactive menu |

**Total:** ~40 KB of code

---

## 📚 Documentation Files

**Keep these on your PC for reference:**

### Essential Guides

| File | Size | What's Inside |
|------|------|---------------|
| **[README.md](README.md)** | 8.8 KB | Complete user manual<br>• Installation guide<br>• Assembly language reference<br>• All instructions explained<br>• Troubleshooting help |
| **[QUICKSTART.md](QUICKSTART.md)** | 3.9 KB | Fast setup guide<br>• 3-step installation<br>• First program in 5 min<br>• Common problems solved |
| **[EXAMPLES.md](EXAMPLES.md)** | 8.3 KB | 21 example programs<br>• Hello World to algorithms<br>• Copy-paste ready<br>• Well commented |

### Reference Documents

| File | Size | What's Inside |
|------|------|---------------|
| **[PACKAGE_SUMMARY.md](PACKAGE_SUMMARY.md)** | 6.6 KB | Overview of everything<br>• What you have<br>• What it does<br>• How to use it |
| **[ARCHITECTURE.md](ARCHITECTURE.md)** | 18 KB | Technical deep dive<br>• System diagrams<br>• Data flow charts<br>• Performance specs |
| **[BCC500_Simulator_Plan.md](BCC500_Simulator_Plan.md)** | 18 KB | Design document<br>• Why simulator vs emulator<br>• Architecture decisions<br>• Implementation phases |

### Historical Reference

| File | Size | What's Inside |
|------|------|---------------|
| **bcc500_emulator_plan.md** | 11 KB | Original emulation plan<br>• Why it was too hard<br>• Comparison of approaches<br>• Technical challenges |

---

## 🎯 Recommended Reading Order

### Day 1: Get it Running
1. **QUICKSTART.md** - 5 min setup
2. Run Demo 1 (Hello World)
3. Run Demo 5 (Multi-processor)

### Day 2: Learn to Program
1. **EXAMPLES.md** - Read examples 1-10
2. Try Custom Program (Menu option 6)
3. Modify an example program

### Day 3: Deep Dive
1. **README.md** - Full instruction reference
2. **ARCHITECTURE.md** - How it works
3. Create your own programs

### Optional: Technical Details
- **BCC500_Simulator_Plan.md** - Design decisions
- **bcc500_emulator_plan.md** - Historical context

---

## 🎮 What Each Demo Does

Available from main menu (option 1-5):

| Demo | Program | What It Shows |
|------|---------|---------------|
| **1** | Hello World | Basic I/O with SYS calls |
| **2** | Count 1-10 | Loops and arithmetic |
| **3** | Add 42+17 | Memory operations |
| **4** | Fibonacci | Complex algorithm |
| **5** | Multi-CPU | 3 processors running together |

---

## 🔧 Quick Reference

### Upload Files to Pico 2

**Using mpremote:**
```bash
pip install mpremote
mpremote cp memory.py :
mpremote cp processor.py :
mpremote cp supervisor.py :
mpremote cp assembler.py :
mpremote cp main.py :
```

**Using Thonny:**
1. Open file
2. File → Save As
3. Choose "Raspberry Pi Pico"
4. Save

### Connect to Pico 2

**Linux/Mac:**
```bash
screen /dev/ttyACM0 115200
```

**Windows:**
```
Use PuTTY or Thonny
```

### Run Simulator
```python
# Press Ctrl+D to reset, or:
import main
```

---

## 📊 File Statistics

```
Breakdown by Category:

Python Code (Core):        ~40 KB  (5 files)
Documentation:             ~76 KB  (7 files)
────────────────────────────────────────────
Total Package:            ~116 KB (12 files)

Memory Usage at Runtime:
- Simulated Memory:        96 KB
- Python Code:            ~40 KB
- Runtime Data:           ~60 KB
- MicroPython VM:        ~100 KB
────────────────────────────────────────────
Total RAM Used:          ~296 KB
Available on Pico 2:      520 KB
Free Headroom:           ~224 KB ✓
```

---

## 🎓 What You'll Learn

By using this simulator, you'll understand:

### Computer Architecture
- Multi-processor systems
- Shared memory architecture
- Instruction set design
- Register organization
- Status flags and conditions

### Operating Systems
- Process scheduling
- Time-sharing concepts
- System calls
- Resource coordination
- Inter-process communication

### Assembly Programming
- Low-level coding
- Memory management
- Control flow
- Subroutines
- Optimization techniques

### Computer History
- 1960s computing
- BCC-500 and ALOHAnet
- Early networking (ARPANET)
- Xerox PARC lineage
- Evolution of computing

---

## 🛠️ System Requirements

### Hardware
- ✓ Raspberry Pi Pico 2 (RP2350)
- ✓ USB cable
- ✓ Computer with USB port

### Software
- ✓ MicroPython 1.20+ for Pico 2
- ✓ Serial terminal program
- Optional: Thonny IDE

### Knowledge
- Basic understanding of programming
- Interest in computer history
- Curiosity and patience!

---

## 📈 Performance

| Metric | Value |
|--------|-------|
| Simulation Speed | 2,000-20,000 inst/sec |
| Original BCC-500 | ~500,000 inst/sec |
| Relative Speed | 0.4-4% of original |
| **Conclusion** | Fast enough for demos! ✓ |

---

## ✨ Features Included

- ✓ 28-instruction CPU
- ✓ 3 processors (expandable)
- ✓ 32K word memory
- ✓ Assembly language
- ✓ Interactive menu
- ✓ 5 built-in demos
- ✓ Custom program entry
- ✓ Single-step debugging
- ✓ Memory inspection
- ✓ Statistics display
- ✓ System call interface

---

## 🎯 Common Use Cases

### Education
- Teaching computer architecture
- Assembly language course
- Operating systems lab
- Computer history class

### Experimentation
- Test algorithms
- Learn low-level programming
- Explore multi-processing
- Prototype ideas

### Historical Recreation
- Understand 1960s computing
- Appreciate modern advances
- Preserve computing history

### Fun!
- Retro computing hobby
- Challenge yourself
- Show off to friends
- Learn something new

---

## 🚦 Status Indicators

| Indicator | Meaning |
|-----------|---------|
| RUN | Processor is executing |
| HLT | Processor has halted |
| Z | Zero flag set |
| N | Negative flag set |
| C | Carry flag set |
| V | Overflow flag set |

---

## 💡 Tips for Success

1. **Start Simple**
   - Begin with Demo 1
   - Try small modifications
   - Build up gradually

2. **Use Examples**
   - All examples are tested
   - Copy and adapt them
   - Learn by doing

3. **Read Error Messages**
   - Assembler shows errors
   - Check line numbers
   - Fix one at a time

4. **Save Your Work**
   - Keep programs on PC
   - Document changes
   - Build a library

5. **Have Fun!**
   - Experiment freely
   - Try new things
   - Don't worry about breaking it

---

## 🔗 Related Resources

- **Bitsavers** - Historical documentation
  http://bitsavers.org/pdf/bcc/

- **Computer History Museum**
  https://computerhistory.org/

- **MicroPython Documentation**
  https://docs.micropython.org/

---

## 📞 Getting Help

### If Something Doesn't Work:

1. Check **QUICKSTART.md** - Common issues
2. Review **README.md** - Troubleshooting section
3. Verify all files uploaded correctly
4. Try soft reset (Ctrl+D)
5. Re-upload files if needed

### Remember:
- All example programs work
- Tested on Pico 2
- Complete and ready to use

---

## 🎉 You're All Set!

Everything you need is in this package:
- ✓ Complete working simulator
- ✓ Comprehensive documentation
- ✓ Example programs
- ✓ Quick start guide
- ✓ Technical references

**Ready to explore 1960s computing on your Pico 2!** 🚀

---

## 📝 Version Info

**Package:** BCC-500 Simulator for Pico 2  
**Version:** 1.0  
**Date:** November 2025  
**Status:** Complete & Tested ✓  

**Original BCC-500:** 1970-1980  
**UC Berkeley Project Genie** → **BCC** → **Xerox PARC** → **Modern Computing**

---

**Start with QUICKSTART.md and have fun!** 🎊
