# BCC-500 Simulator Architecture Diagram

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                  RASPBERRY PI PICO 2 (RP2350)                   │
│                     Running MicroPython                          │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                       main.py (Entry Point)                      │
│  • Interactive menu system                                       │
│  • Demo programs                                                 │
│  • User interface                                                │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                    Supervisor (supervisor.py)                    │
│  • Coordinates processors                                        │
│  • Round-robin scheduler                                         │
│  • System call handler                                           │
│  • Statistics & debugging                                        │
└─────────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        ↓                     ↓                     ↓
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ Processor 0  │     │ Processor 1  │     │ Processor 2  │
│ (User)       │     │ (User)       │     │ (Supervisor) │
├──────────────┤     ├──────────────┤     ├──────────────┤
│ Registers:   │     │ Registers:   │     │ Registers:   │
│ • A (24-bit) │     │ • A (24-bit) │     │ • A (24-bit) │
│ • B (24-bit) │     │ • B (24-bit) │     │ • B (24-bit) │
│ • X (18-bit) │     │ • X (18-bit) │     │ • X (18-bit) │
│ • PC(18-bit) │     │ • PC(18-bit) │     │ • PC(18-bit) │
├──────────────┤     ├──────────────┤     ├──────────────┤
│ Status Flags:│     │ Status Flags:│     │ Status Flags:│
│ • Zero       │     │ • Zero       │     │ • Zero       │
│ • Negative   │     │ • Negative   │     │ • Negative   │
│ • Carry      │     │ • Carry      │     │ • Carry      │
│ • Overflow   │     │ • Overflow   │     │ • Overflow   │
└──────────────┘     └──────────────┘     └──────────────┘
        │                     │                     │
        └─────────────────────┼─────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                    Shared Memory (memory.py)                     │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  32,768 words × 24 bits = 96 KB                          │    │
│  │                                                           │    │
│  │  Address Space: 0x00000 - 0x07FFF (18-bit addressing)   │    │
│  │                                                           │    │
│  │  Layout:                                                  │    │
│  │  0x00000-0x000FF : System reserved                       │    │
│  │  0x00100-0x07FFF : User programs and data               │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                   Assembler (assembler.py)                       │
│  • Compiles assembly to machine code                             │
│  • Label resolution                                              │
│  • Example programs included                                     │
└─────────────────────────────────────────────────────────────────┘
```

## Instruction Execution Flow

```
┌──────────────────────────────────────────────────────────────────┐
│                         Single Instruction Cycle                  │
└──────────────────────────────────────────────────────────────────┘
                              │
                              ↓
                    ┌──────────────────┐
                    │   FETCH PHASE    │
                    │                  │
                    │ 1. Read PC       │
                    │ 2. Load from     │
                    │    memory[PC]    │
                    │ 3. Increment PC  │
                    └──────────────────┘
                              │
                              ↓
                    ┌──────────────────┐
                    │   DECODE PHASE   │
                    │                  │
                    │ Extract:         │
                    │ • Opcode (6 bit) │
                    │ • Address(18 bit)│
                    └──────────────────┘
                              │
                              ↓
                    ┌──────────────────┐
                    │  EXECUTE PHASE   │
                    │                  │
                    │ • Perform        │
                    │   operation      │
                    │ • Update regs    │
                    │ • Set flags      │
                    └──────────────────┘
```

## Instruction Format

```
24-bit Instruction Word:
┌──────────┬─────────────────────────────────────────────┐
│ Bits     │ Description                                  │
├──────────┼─────────────────────────────────────────────┤
│ 23-18    │ Opcode (6 bits) - Which instruction         │
│ 17-0     │ Address (18 bits) - Operand address         │
└──────────┴─────────────────────────────────────────────┘

Example: LDA 0x1234
┌──────────┬──────────────────────────┐
│ 000001   │ 000001001000110100        │
└──────────┴──────────────────────────┘
  Opcode=1     Address=0x1234
  (LDA)
```

## Memory Layout

```
Memory Map (32K words):

0x00000 ┌─────────────────────────────────────┐
        │  System Reserved (256 words)        │
        │  • Interrupt vectors                 │
        │  • System variables                  │
0x000FF └─────────────────────────────────────┘
0x00100 ┌─────────────────────────────────────┐
        │                                      │
        │  Processor 0 Program Space           │
        │                                      │
        ├─────────────────────────────────────┤
        │                                      │
        │  Processor 1 Program Space           │
        │                                      │
        ├─────────────────────────────────────┤
        │                                      │
        │  Processor 2 Program Space           │
        │                                      │
        ├─────────────────────────────────────┤
        │                                      │
        │  Shared Data Area                    │
        │                                      │
        ├─────────────────────────────────────┤
        │                                      │
        │  Free Space                          │
        │                                      │
0x07FFF └─────────────────────────────────────┘
```

## System Call Flow

```
User Program                Processor              Supervisor
     │                          │                      │
     │  SYS n                   │                      │
     ├─────────────────────────>│                      │
     │                          │                      │
     │                          │  system_call(n)      │
     │                          ├─────────────────────>│
     │                          │                      │
     │                          │                      │ Execute
     │                          │                      │ system
     │                          │                      │ function
     │                          │                      │
     │                          │      Return          │
     │                          │<─────────────────────┤
     │  Continue execution      │                      │
     │<─────────────────────────┤                      │
     │                          │                      │
```

## Multi-Processor Scheduling

```
Time-slice Round-Robin:

Cycle:  1    2    3    4    5    6    7    8    9   10   11   12
       ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────
P0:    │ ██ │    │    │ ██ │    │    │ ██ │    │    │ ██ │    │
       ├────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────
P1:    │    │ ██ │    │    │ ██ │    │    │ ██ │    │    │ ██ │
       ├────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────
P2:    │    │    │ ██ │    │    │ ██ │    │    │ ██ │    │    │ ██
       └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────

Legend: ██ = Processor executing instruction
        Each cycle: one instruction per active processor
```

## File Dependencies

```
main.py
  │
  ├─> supervisor.py
  │     │
  │     ├─> processor.py
  │     │     │
  │     │     └─> memory.py
  │     │
  │     └─> memory.py
  │
  └─> assembler.py
        │
        └─> processor.py (for disassembly)
```

## Data Flow Example (Addition Program)

```
Program: Add 42 + 17

Step 1: LDA 0x100
┌─────────┐     ┌─────────┐     ┌─────────┐
│ PC=0x50 │────>│Mem[0x50]│────>│Opcode=1 │
│         │     │         │     │Addr=0x100│
└─────────┘     └─────────┘     └─────────┘
                                      │
                                      ↓
                                ┌─────────┐
                                │A=Mem[100]│
                                │A = 42   │
                                └─────────┘

Step 2: ADD 0x101
┌─────────┐     ┌─────────┐     ┌─────────┐
│ PC=0x51 │────>│Mem[0x51]│────>│Opcode=3 │
│         │     │         │     │Addr=0x101│
└─────────┘     └─────────┘     └─────────┘
                                      │
                                      ↓
                                ┌─────────┐
                                │A=A+Mem[]│
                                │A = 59   │
                                └─────────┘

Step 3: STA 0x102
┌─────────┐     ┌─────────┐     ┌─────────┐
│ PC=0x52 │────>│Mem[0x52]│────>│Opcode=2 │
│         │     │         │     │Addr=0x102│
└─────────┘     └─────────┘     └─────────┘
                                      │
                                      ↓
                                ┌─────────┐
                                │Mem[102]=A│
                                │Result=59│
                                └─────────┘
```

## Component Sizes

```
Component          Lines of Code    Memory Usage
─────────────────────────────────────────────────
memory.py                 98          ~10 KB
processor.py             315          ~30 KB
supervisor.py            233          ~25 KB
assembler.py             267          ~30 KB
main.py                  270          ~25 KB
─────────────────────────────────────────────────
Total                  1,183          ~120 KB

Memory at Runtime:
- Simulated memory: 96 KB
- Python code: ~120 KB
- Runtime data: ~60 KB
─────────────────────
Total: ~276 KB (out of 520 KB available)
```

## Performance Characteristics

```
Operation                    Approximate Time
──────────────────────────────────────────────
Single instruction           50-500 μs
Fetch-decode-execute         ~200 μs average
System call                  100-1000 μs
Memory read/write            10-20 μs
Context switch               ~500 μs
──────────────────────────────────────────────

Throughput: ~2,000-20,000 instructions/second
            (~0.4-4% of original BCC-500)
```

This architecture provides a faithful simulation of BCC-500 concepts
while being practical on modern embedded hardware!
