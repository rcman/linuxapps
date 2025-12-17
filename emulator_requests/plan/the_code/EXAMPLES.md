# BCC-500 Example Programs

Collection of example assembly programs for the BCC-500 simulator.

## Basic Examples

### 1. Minimal Program
```assembly
; The simplest possible program - just halt
HLT
```

### 2. Print a Single Character
```assembly
; Print the letter 'A'
LDA char_a
SYS 1         ; System call 1: Print character
HLT

char_a: 0x000041  ; ASCII 'A'
```

### 3. Print Multiple Characters
```assembly
; Print "HI"
LDA char_h
SYS 1
LDA char_i
SYS 1
SYS 4         ; Print newline
HLT

char_h: 0x000048  ; 'H'
char_i: 0x000049  ; 'I'
```

## Arithmetic Examples

### 4. Simple Addition
```assembly
; Add 10 + 20 = 30
LDA num1
ADD num2
STA result
SYS 2         ; Print result
SYS 4         ; Newline
HLT

num1:   0x00000A  ; 10
num2:   0x000014  ; 20
result: 0x000000
```

### 5. Multiplication
```assembly
; Multiply 6 * 7 = 42
LDA num1
MUL num2
STA result
SYS 2
SYS 4
HLT

num1:   0x000006  ; 6
num2:   0x000007  ; 7
result: 0x000000
```

### 6. Division
```assembly
; Divide 100 / 7 = 14 remainder 2
LDA dividend
DIV divisor
STA quotient    ; Quotient in A
LDB remainder   ; Remainder in B
SYS 2           ; Print quotient
SYS 4
HLT

dividend:  0x000064  ; 100
divisor:   0x000007  ; 7
quotient:  0x000000
remainder: 0x000000
```

## Loop Examples

### 7. Count to 10
```assembly
; Count from 1 to 10
LDA start
STA counter

loop:
    LDA counter
    SYS 2         ; Print number
    SYS 4         ; Newline
    ADD one
    STA counter
    CMP limit
    BNZ loop      ; Branch if not equal to limit
    HLT

start:   0x000001  ; 1
one:     0x000001
counter: 0x000000
limit:   0x00000B  ; 11 (loop until counter = 11)
```

### 8. Countdown
```assembly
; Count down from 10 to 0
LDA start
STA counter

loop:
    LDA counter
    SYS 2
    SYS 4
    SUB one
    STA counter
    BPL loop      ; Branch if positive
    HLT

start:   0x00000A  ; 10
one:     0x000001
counter: 0x000000
```

### 9. Sum of Numbers 1-10
```assembly
; Calculate 1+2+3+...+10 = 55
CLA            ; Clear A (sum starts at 0)
STA sum
LDA start
STA counter

loop:
    LDA sum
    ADD counter
    STA sum
    
    LDA counter
    ADD one
    STA counter
    CMP limit
    BNZ loop
    
    LDA sum
    SYS 2
    SYS 4
    HLT

start:   0x000001  ; 1
one:     0x000001
sum:     0x000000
counter: 0x000000
limit:   0x00000B  ; 11
```

## Fibonacci Sequence

### 10. First 10 Fibonacci Numbers
```assembly
; Calculate and print first 10 Fibonacci numbers
; 0, 1, 1, 2, 3, 5, 8, 13, 21, 34

    LDA fib_a
    STA current_a
    LDA fib_b
    STA current_b
    LDA count
    STA counter

fib_loop:
    ; Print current number
    LDA current_a
    SYS 2
    SYS 4
    
    ; Calculate next number
    ADD current_b
    STA temp
    
    ; Shift values
    LDA current_b
    STA current_a
    LDA temp
    STA current_b
    
    ; Decrement counter
    LDA counter
    SUB one
    STA counter
    BNZ fib_loop
    
    HLT

fib_a:     0x000000  ; First Fibonacci number
fib_b:     0x000001  ; Second Fibonacci number
current_a: 0x000000
current_b: 0x000000
temp:      0x000000
counter:   0x000000
count:     0x00000A  ; Calculate 10 numbers
one:       0x000001
```

## Factorial

### 11. Factorial of 5 (5! = 120)
```assembly
; Calculate 5! = 5*4*3*2*1 = 120
    LDA number
    STA result
    SUB one
    STA counter

loop:
    LDA result
    MUL counter
    STA result
    
    LDA counter
    SUB one
    STA counter
    BNZ loop
    
    ; Print result
    LDA result
    SYS 2
    SYS 4
    HLT

number:  0x000005  ; 5
one:     0x000001
result:  0x000000
counter: 0x000000
```

## String/Text Examples

### 12. Print "HELLO WORLD"
```assembly
; Print a string character by character
    LDA h
    SYS 1
    LDA e
    SYS 1
    LDA l
    SYS 1
    SYS 1         ; Print 'L' twice
    LDA o
    SYS 1
    LDA space
    SYS 1
    LDA w
    SYS 1
    LDA o
    SYS 1
    LDA r
    SYS 1
    LDA l
    SYS 1
    LDA d
    SYS 1
    SYS 4         ; Newline
    HLT

h:     0x000048  ; 'H'
e:     0x000065  ; 'e'
l:     0x00006C  ; 'l'
o:     0x00006F  ; 'o'
space: 0x000020  ; ' '
w:     0x000077  ; 'w'
r:     0x000072  ; 'r'
d:     0x000064  ; 'd'
```

## Logical Operations

### 13. Bitwise AND
```assembly
; Demonstrate AND operation
; 0xFF & 0x0F = 0x0F
LDA value1
AND value2
SYS 3         ; Print in hex
SYS 4
HLT

value1: 0x0000FF
value2: 0x00000F
```

### 14. Bitwise OR
```assembly
; Demonstrate OR operation
; 0xF0 | 0x0F = 0xFF
LDA value1
IOR value2
SYS 3
SYS 4
HLT

value1: 0x0000F0
value2: 0x00000F
```

### 15. Bitwise XOR (Simple Encryption)
```assembly
; XOR encryption/decryption
; data XOR key = encrypted
; encrypted XOR key = data

    LDA data
    XOR key
    STA encrypted
    
    ; Print encrypted value
    SYS 3
    SYS 4
    
    ; Decrypt
    XOR key
    STA decrypted
    
    ; Print decrypted value (should match original)
    SYS 3
    SYS 4
    HLT

data:      0x0042AF
key:       0x00DEAD
encrypted: 0x000000
decrypted: 0x000000
```

## Subroutine Examples

### 16. Simple Subroutine Call
```assembly
; Demonstrate JSR (Jump to Subroutine) and RTS (Return)
    JSR print_hello
    JSR print_hello
    HLT

print_hello:
    LDA h
    SYS 1
    LDA i
    SYS 1
    SYS 4
    RTS           ; Return from subroutine

h: 0x000048
i: 0x000069
```

## Multi-Processor Examples

### 17. Processor ID Display
```assembly
; Each processor prints its own ID
    SYS 6         ; Get processor ID (returns in A)
    SYS 2         ; Print ID
    SYS 4         ; Newline
    HLT
```

### 18. Shared Counter
```assembly
; Multiple processors increment a shared counter
; WARNING: This has race conditions (no synchronization)

    ; Read shared counter
    LDA shared_counter
    
    ; Increment it
    ADD one
    
    ; Write back
    STA shared_counter
    
    ; Print result
    SYS 2
    SYS 4
    
    HLT

one: 0x000001
shared_counter: 0x001000  ; Shared memory location
```

## Algorithm Examples

### 19. Greatest Common Divisor (GCD)
```assembly
; Calculate GCD using Euclidean algorithm
; GCD(48, 18) = 6

    LDA num_a
    STA a
    LDA num_b
    STA b

gcd_loop:
    ; Check if b == 0
    LDA b
    BZE done
    
    ; temp = a % b
    LDA a
    DIV b
    LDB temp      ; Remainder goes to B register
    
    ; a = b
    LDA b
    STA a
    
    ; b = temp
    LDA temp
    STA b
    
    JMP gcd_loop

done:
    ; Result is in 'a'
    LDA a
    SYS 2
    SYS 4
    HLT

num_a: 0x000030  ; 48
num_b: 0x000012  ; 18
a:     0x000000
b:     0x000000
temp:  0x000000
```

### 20. Power Function (2^8 = 256)
```assembly
; Calculate base^exponent (2^8 = 256)
    LDA one
    STA result
    LDA exponent
    STA counter

power_loop:
    LDA result
    MUL base
    STA result
    
    LDA counter
    SUB one
    STA counter
    BNZ power_loop
    
    LDA result
    SYS 2
    SYS 4
    HLT

base:     0x000002  ; 2
exponent: 0x000008  ; 8
result:   0x000000
counter:  0x000000
one:      0x000001
```

## Testing Examples

### 21. All Instructions Test
```assembly
; Test most available instructions
    ; Data movement
    LDA test_val
    STA temp
    LDX test_val
    STX temp
    
    ; Arithmetic
    ADD test_val
    SUB test_val
    MUL test_val
    DIV test_val
    
    ; Logical
    AND test_val
    IOR test_val
    XOR test_val
    NOT
    
    ; Shifts
    SHL
    SHR
    
    ; Branches
    CLA
    BZE is_zero
    JMP not_zero

is_zero:
    LDA success
    SYS 1
    
not_zero:
    HLT

test_val: 0x0000FF
temp:     0x000000
success:  0x000059  ; 'Y'
```

## How to Use These Programs

1. Copy the assembly code
2. In the simulator, select option 6 (Run Custom Program)
3. Paste the code (without the triple backticks)
4. Type END or press Enter on an empty line
5. Watch it run!

Or modify the examples in `assembler.py` and upload to your Pico.

## Tips for Writing Programs

- Use labels for data and jump targets
- Comments start with semicolon (;)
- Hex numbers: 0x1234
- Decimal numbers: 4660
- Always end with HLT
- Use SYS calls for output
- Memory from 0x100 onwards is safe for user programs

## Common Patterns

**Loop Pattern:**
```assembly
    LDA counter
loop:
    ; Do something
    SUB one
    STA counter
    BNZ loop
```

**Conditional Pattern:**
```assembly
    LDA value
    CMP threshold
    BPL greater
    ; Less than code
    JMP done
greater:
    ; Greater than code
done:
    HLT
```

**Subroutine Pattern:**
```assembly
    JSR my_function
    HLT

my_function:
    ; Do work
    RTS
```

Have fun experimenting! 🎉
