# GGML Quantization - Complete Guide

## What You Asked For

**"How do I get proper GGML weight loading (quantized tensors)?"**

This guide shows you **exactly** how GGML quantization works and how to load/dequantize model weights.

## Quick Answer

**Quantization** = Compressing float32 weights to smaller formats (4-bit, 8-bit, fp16) to save memory.

**Your model** (`ggml-base.en.bin`) uses **MOSTLY_F16** format:
- Most weights are 16-bit floats (F16)
- Small weights (1D tensors) stay as F32
- 2x compression with minimal quality loss

## Understanding Quantization

### Why Quantize?

```
Whisper base model (fp32):  ~290 MB
Whisper base model (fp16):  ~145 MB  ← Your model
Whisper base model (q4_0):   ~75 MB
```

**Benefits:**
- Uses less RAM/VRAM
- Faster inference (less memory bandwidth)
- Can run larger models on same hardware

**Trade-off:**
- Tiny accuracy loss (usually <1%)

### How It Works

**Normal weight storage:**
```
Float32: 0.123456789 → 32 bits
```

**Quantized storage (Q4_0):**
```
Block of 32 values:
  - 1 scale factor (fp16): 2 bytes
  - 32 values at 4 bits each: 16 bytes
  Total: 18 bytes for 32 floats (was 128 bytes!)
  Compression: 7.1x
```

## GGML Quantization Formats

### Most Common Types

| Format | Bits/Value | Compression | Use Case |
|--------|-----------|-------------|----------|
| **F32** | 32 | 1.0x | Reference quality |
| **F16** | 16 | 2.0x | **Your model uses this** ⭐ |
| **Q8_0** | 8 | 3.8x | High quality, moderate compression |
| **Q4_1** | 4 | 6.4x | Good quality, high compression |
| **Q4_0** | 4 | 7.1x | Most popular quantization |
| **Q2_K** | 2-3 | ~10x | Maximum compression |

### Block Structures

#### Q4_0 (Most Common)

```c
#define QK4_0 32  // Process 32 values at a time

typedef struct {
    uint16_t d;              // Scale factor (fp16)
    uint8_t qs[16];          // 32×4-bit values packed in 16 bytes
} block_q4_0;

// Dequantization formula:
// value[i] = scale * (quant[i] - 8)
//
// quant[i] is 0-15 (4 bits)
// Subtract 8 to center: -8 to +7
```

**Example:**
```
Original values: [0.5, -0.3, 0.7, -0.2, ...]

After quantization:
  scale = 0.1
  quants = [13, 5, 15, 6, ...]  (4-bit values)

Dequantized:
  0.1 * (13-8) = 0.5
  0.1 * (5-8)  = -0.3
  0.1 * (15-8) = 0.7
  0.1 * (6-8)  = -0.2
```

#### Q4_1 (With Min/Max)

```c
typedef struct {
    uint16_t d;              // Scale (delta)
    uint16_t m;              // Minimum value
    uint8_t qs[16];          // Quants
} block_q4_1;

// Dequantization:
// value[i] = scale * quant[i] + min
```

Better for values not centered around zero.

#### Q8_0 (8-bit)

```c
typedef struct {
    uint16_t d;              // Scale
    int8_t qs[32];           // Signed 8-bit values
} block_q8_0;

// Dequantization:
// value[i] = scale * quant[i]
```

Higher precision than Q4.

#### F16 (Half-Precision Float)

```c
typedef uint16_t ggml_fp16_t;

// Standard IEEE 754 half-precision format:
// 1 sign bit + 5 exponent bits + 10 mantissa bits
```

**Your model (ggml-base.en.bin) uses this format!**

## How to Load Quantized Weights

### Step 1: Understand GGML File Format

```
GGML File Structure:
┌──────────────────────┐
│ Magic: 0x67676d6c    │  4 bytes ("ggml")
│ Hyperparameters      │  44 bytes (Whisper config)
│ Mel Filters          │  Variable (80×201 floats)
│ Vocabulary           │  Variable (51864 tokens)
│ ───────────────────  │
│ Tensor 1:            │
│   - n_dims           │  4 bytes
│   - name_length      │  4 bytes
│   - type             │  4 bytes (0=F32, 1=F16, 2=Q4_0, etc.)
│   - dimensions[]     │  n_dims × 4 bytes
│   - name             │  name_length bytes
│   - [padding]        │  Align to 32 bytes
│   - data             │  Quantized weight data
│ ───────────────────  │
│ Tensor 2:            │
│   ...                │
└──────────────────────┘
```

### Step 2: Parse Tensor Header

```c
typedef struct {
    int32_t n_dims;
    int32_t type;
    int64_t dims[4];     // Shape: [dim0, dim1, dim2, dim3]
    char name[128];
    size_t data_offset;  // Where in file
} TensorInfo;

// Read from file:
fread(&n_dims, sizeof(int32_t), 1, f);
fread(&name_length, sizeof(int32_t), 1, f);
fread(&type, sizeof(int32_t), 1, f);
fread(dims, sizeof(int32_t), n_dims, f);
fread(name, 1, name_length, f);
```

### Step 3: Load and Dequantize

```c
float* load_tensor(FILE* f, TensorInfo* info) {
    // Calculate size
    int64_t n_elements = dims[0] * dims[1] * ... ;

    // Allocate output
    float* output = malloc(n_elements * sizeof(float));

    // Seek to data
    fseek(f, info->data_offset, SEEK_SET);

    // Load based on type
    switch (info->type) {
        case 1: {  // F16
            uint16_t* data = malloc(n_elements * sizeof(uint16_t));
            fread(data, sizeof(uint16_t), n_elements, f);

            // Dequantize fp16 → fp32
            for (int i = 0; i < n_elements; i++) {
                output[i] = fp16_to_fp32(data[i]);
            }
            free(data);
            break;
        }

        case 2: {  // Q4_0
            int n_blocks = (n_elements + 31) / 32;  // Round up
            block_q4_0* blocks = malloc(n_blocks * sizeof(block_q4_0));
            fread(blocks, sizeof(block_q4_0), n_blocks, f);

            // Dequantize Q4_0 → fp32
            dequantize_row_q4_0(blocks, output, n_elements);
            free(blocks);
            break;
        }

        case 0: {  // F32 - already in target format
            fread(output, sizeof(float), n_elements, f);
            break;
        }
    }

    return output;
}
```

### Step 4: Dequantize Q4_0 (Most Important)

```c
void dequantize_row_q4_0(const block_q4_0* blocks, float* output, int n) {
    int num_blocks = n / 32;

    for (int b = 0; b < num_blocks; b++) {
        // Get scale factor
        float scale = fp16_to_fp32(blocks[b].d);

        // Dequantize 32 values
        for (int i = 0; i < 32; i++) {
            // Get 4-bit value from packed byte
            uint8_t byte = blocks[b].qs[i / 2];
            uint8_t val;

            if (i % 2 == 0) {
                val = byte & 0x0F;        // Low 4 bits
            } else {
                val = (byte >> 4) & 0x0F; // High 4 bits
            }

            // Dequantize: center around 8, scale
            output[b * 32 + i] = scale * (val - 8);
        }
    }
}
```

## Working Example

### Compile and Run

```bash
gcc -O2 -o ggml_quantization_guide ggml_quantization_guide.c -lm
./ggml_quantization_guide
```

**Output:**
```
=== Quantization Format Comparison ===

For 32 float values:

Format  | Bits/val | Block Size | Compression | Quality
--------|----------|------------|-------------|---------
F32     |   32     | 128 bytes  |    1.0x     | Perfect
F16     |   16     |  64 bytes  |    2.0x     | Excellent
Q8_0    |    8     |  34 bytes  |    3.8x     | Very Good
Q4_1    |    4     |  20 bytes  |    6.4x     | Good
Q4_0    |    4     |  18 bytes  |    7.1x     | Good (most common)

=== Live Dequantization Example ===

Dequantized first 8 values:
  [0]: 0.00
  [1]: 4.00
  [2]: -4.00
  [3]: 7.00
  ...
```

## Complete Code Example

See `ggml_quantization_guide.c` for:
- All quantization type definitions
- FP16 ↔ FP32 conversion
- Dequantization functions for Q4_0, Q4_1, Q8_0, F16
- Tensor loading from GGML files
- Working examples with real data

## Your Model (ggml-base.en.bin)

**Format:** MOSTLY_F16 (ftype=1)

This means:
```c
Large tensors (2D+):
  - encoder.layers.*.self_attn.*.weight  → F16
  - encoder.layers.*.mlp.*.weight        → F16
  - decoder.layers.*.*.weight            → F16

Small tensors (1D):
  - *.bias                               → F32
  - layer_norm weights                   → F32
  - token_embedding                      → F16 or F32
```

**To use your model:**

1. **Load F16 tensors:**
```c
uint16_t* fp16_data = load_from_file();
float* fp32_data = malloc(size * sizeof(float));

for (int i = 0; i < size; i++) {
    fp32_data[i] = fp16_to_fp32(fp16_data[i]);
}
```

2. **Load F32 tensors:**
```c
float* data = malloc(size * sizeof(float));
fread(data, sizeof(float), size, file);
// Already in correct format!
```

## Integration Into Your Code

### Option 1: Add to whisper_educational.c

```c
// Add these functions to whisper_educational.c:

#include "ggml_quantization_guide.c"  // Or copy functions

// In load_whisper_model():
void load_whisper_model(const char* filename, WhisperModel* model) {
    // ... existing code to load header ...

    // Then load each tensor:
    while (more_tensors) {
        TensorInfo info;
        parse_tensor_header(f, &info);

        float* weights = load_and_dequantize_tensor(f, &info);

        // Store in model structure
        if (strcmp(info.name, "encoder.layers.0.self_attn.q_proj.weight") == 0) {
            model->encoder.layer[0].attn_q = weights;
        }
        // ... map all tensors to model structure ...
    }
}
```

### Option 2: Standalone Tensor Dumper

```c
// Create a tool to extract and inspect tensors
void dump_model_tensors(const char* model_path) {
    FILE* f = fopen(model_path, "rb");

    // Skip to tensors section
    // ... parse header ...

    // Dump each tensor
    for (int i = 0; i < num_tensors; i++) {
        TensorInfo info;
        parse_tensor_header(f, &info);

        float* data = load_and_dequantize_tensor(f, &info);

        printf("Tensor %d: %s\n", i, info.name);
        printf("  First 10 values: ");
        for (int j = 0; j < 10; j++) {
            printf("%.6f ", data[j]);
        }
        printf("\n\n");

        free(data);
    }
}
```

## Memory Usage Comparison

**Your whisper base model:**

```
Format      | Memory Used
------------|-------------
Original    | ~290 MB (all fp32)
Your model  | ~142 MB (mostly fp16)  ⭐
Q4_0        |  ~75 MB (4-bit)
```

**Inference memory (additional):**
```
Activations: ~64 MB
KV cache:    ~25 MB
Total runtime: ~230 MB with your model
```

## Advanced: K-Quantization

Newer GGML models use "K-quants" (Q2_K, Q3_K, Q4_K, etc.):

```c
// K-quants use blocks of 256 values with sophisticated encoding
// More complex but better quality per bit

#define QK_K 256

typedef struct {
    uint8_t scales[QK_K/16];   // Per-group scales
    uint8_t qs[QK_K/2];        // Quantized values
    ggml_half d;               // Block scale
    ggml_half dmin;            // Min value
} block_q4_K;

// Dequantization is more complex but achieves better
// compression/quality trade-off
```

## Resources

**Files created:**
- `ggml_quantization_guide.c` - Complete working example
- `GGML_QUANTIZATION_README.md` - This guide

**To learn more:**
- Study `whisper.cpp/ggml/src/ggml-quants.c` - All quantization formats
- Read `whisper.cpp/src/whisper.cpp:1400-1700` - Model loading code
- See `whisper.cpp/ggml/include/ggml.h:380-460` - Type definitions

## Summary

**To load GGML quantized weights:**

1. **Identify tensor type** (F32=0, F16=1, Q4_0=2, etc.)
2. **Calculate size** = (elements / block_size) × block_struct_size
3. **Load binary data** from file
4. **Dequantize** using appropriate function
5. **Result**: fp32 array ready to use

**Key insight:**
- Quantization is just compression
- GGML stores weights in compressed format
- Dequantize when loading OR during inference
- Your model uses F16 (simple 2x compression)

The `ggml_quantization_guide.c` file has **all the code you need** - it's ready to use!
