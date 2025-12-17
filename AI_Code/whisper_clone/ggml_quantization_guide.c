/*
 * GGML Quantization - Educational Guide
 *
 * This demonstrates how GGML stores weights in quantized (compressed) format
 * to save memory and speed up inference.
 *
 * GGML uses various quantization schemes - this shows the most common ones.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

// ----------------------------------------------------------------------------
// GGML Type Definitions (from ggml.h)

typedef uint16_t ggml_fp16_t;

// Convert fp16 to fp32 (simplified)
float ggml_fp16_to_fp32(ggml_fp16_t h) {
    uint32_t h32 = h;

    uint32_t sign = (h32 & 0x8000) << 16;
    uint32_t exp = (h32 & 0x7C00) >> 10;
    uint32_t mant = (h32 & 0x03FF) << 13;

    if (exp == 0) {  // Denormal
        if (mant == 0) {
            // Zero
            uint32_t f32 = sign;
            return *(float*)&f32;
        }
        // Normalize
        while ((mant & 0x00800000) == 0) {
            mant <<= 1;
            exp--;
        }
        mant &= 0x007FFFFF;
        exp = 127 - 15 - (10 - exp);
    } else if (exp == 31) {  // Inf or NaN
        exp = 255;
    } else {
        exp = exp - 15 + 127;
    }

    uint32_t f32 = sign | (exp << 23) | mant;
    return *(float*)&f32;
}

// ----------------------------------------------------------------------------
// Quantization Block Structures

/*
 * Q4_0 Quantization (4-bit, symmetric)
 *
 * Most common quantization format in GGML models.
 * Stores 32 float values in just 18 bytes!
 *
 * Format:
 * - 1 fp16 scale factor (delta)
 * - 16 bytes of 4-bit values (2 per byte = 32 values)
 *
 * Dequantization: value[i] = delta * (quant[i] - 8)
 *
 * Why -8? 4-bit values are 0-15, centering at 8 makes it symmetric (-8 to +7)
 */
#define QK4_0 32
typedef struct {
    ggml_fp16_t d;           // delta (scale factor)
    uint8_t qs[QK4_0 / 2];   // quantized values (4 bits each, packed 2 per byte)
} block_q4_0;

/*
 * Q4_1 Quantization (4-bit with min/max)
 *
 * Similar to Q4_0 but stores both scale and minimum value.
 * Better for values that aren't centered around zero.
 *
 * Dequantization: value[i] = delta * quant[i] + min
 */
#define QK4_1 32
typedef struct {
    ggml_fp16_t d;           // delta (scale)
    ggml_fp16_t m;           // min value
    uint8_t qs[QK4_1 / 2];   // quantized values
} block_q4_1;

/*
 * Q8_0 Quantization (8-bit)
 *
 * Higher precision, less compression.
 * Stores 32 float values in 34 bytes.
 *
 * Dequantization: value[i] = delta * quant[i]
 */
#define QK8_0 32
typedef struct {
    ggml_fp16_t d;          // delta
    int8_t qs[QK8_0];       // signed 8-bit values
} block_q8_0;

/*
 * F16 Quantization (half-precision float)
 *
 * Not really "quantized" - just uses 16-bit floats.
 * Double the memory of Q4_0 but higher accuracy.
 */
typedef ggml_fp16_t block_f16;

// ----------------------------------------------------------------------------
// Dequantization Functions

/*
 * Dequantize Q4_0 block to floats
 *
 * This is the core operation - converting compressed weights back to floats
 * for computation.
 */
void dequantize_row_q4_0(const block_q4_0 * blocks, float * output, int n) {
    int num_blocks = n / QK4_0;

    for (int b = 0; b < num_blocks; b++) {
        const block_q4_0 * block = &blocks[b];

        // Convert fp16 scale to fp32
        float delta = ggml_fp16_to_fp32(block->d);

        // Dequantize 32 values
        for (int i = 0; i < QK4_0; i++) {
            // Extract 4-bit value from packed byte
            // Each byte contains 2 values: low 4 bits and high 4 bits
            uint8_t packed_byte = block->qs[i / 2];
            uint8_t val;

            if (i % 2 == 0) {
                // Low 4 bits
                val = packed_byte & 0x0F;
            } else {
                // High 4 bits
                val = (packed_byte >> 4) & 0x0F;
            }

            // Dequantize: center around 8, scale by delta
            // val is 0-15, so (val - 8) is -8 to +7
            output[b * QK4_0 + i] = delta * (val - 8);
        }
    }
}

/*
 * Dequantize Q4_1 block to floats
 */
void dequantize_row_q4_1(const block_q4_1 * blocks, float * output, int n) {
    int num_blocks = n / QK4_1;

    for (int b = 0; b < num_blocks; b++) {
        const block_q4_1 * block = &blocks[b];

        float delta = ggml_fp16_to_fp32(block->d);
        float min_val = ggml_fp16_to_fp32(block->m);

        for (int i = 0; i < QK4_1; i++) {
            uint8_t packed_byte = block->qs[i / 2];
            uint8_t val = (i % 2 == 0) ? (packed_byte & 0x0F) : (packed_byte >> 4);

            // Different formula: scale and add minimum
            output[b * QK4_1 + i] = delta * val + min_val;
        }
    }
}

/*
 * Dequantize Q8_0 block to floats
 */
void dequantize_row_q8_0(const block_q8_0 * blocks, float * output, int n) {
    int num_blocks = n / QK8_0;

    for (int b = 0; b < num_blocks; b++) {
        const block_q8_0 * block = &blocks[b];
        float delta = ggml_fp16_to_fp32(block->d);

        for (int i = 0; i < QK8_0; i++) {
            // Signed 8-bit value (-128 to +127)
            output[b * QK8_0 + i] = delta * block->qs[i];
        }
    }
}

/*
 * Dequantize F16 to floats
 */
void dequantize_row_f16(const block_f16 * blocks, float * output, int n) {
    for (int i = 0; i < n; i++) {
        output[i] = ggml_fp16_to_fp32(blocks[i]);
    }
}

// ----------------------------------------------------------------------------
// GGML File Format - Tensor Loading

/*
 * In a GGML model file, after the header, vocab, and filters,
 * tensors are stored like this:
 *
 * For each tensor:
 *   - n_dims (int32)                 // Number of dimensions (usually 1-4)
 *   - name_length (int32)            // Length of tensor name string
 *   - tensor_type (int32)            // GGML type (Q4_0=2, Q4_1=3, Q8_0=8, F16=1, F32=0)
 *   - dimensions[n_dims] (int32[])   // Shape of tensor
 *   - name[name_length] (char[])     // Tensor name (e.g., "encoder.layers.0.mlp.weight")
 *   - [padding to align to 32 bytes]
 *   - tensor_data (bytes)            // Actual quantized data
 */

typedef struct {
    int32_t n_dims;
    int32_t type;       // ggml_type enum value
    int64_t dims[4];    // ne[0], ne[1], ne[2], ne[3]
    char name[128];
    size_t data_offset; // Offset in file where data starts
    size_t data_size;   // Size of data in bytes
} TensorInfo;

// Type sizes in bytes per block
size_t ggml_type_size(int type) {
    switch (type) {
        case 0:  return sizeof(float);           // F32
        case 1:  return sizeof(ggml_fp16_t);     // F16
        case 2:  return sizeof(block_q4_0);      // Q4_0
        case 3:  return sizeof(block_q4_1);      // Q4_1
        case 8:  return sizeof(block_q8_0);      // Q8_0
        default: return 0;
    }
}

// Block size (number of elements per block)
size_t ggml_blck_size(int type) {
    switch (type) {
        case 0:  return 1;      // F32
        case 1:  return 1;      // F16
        case 2:  return QK4_0;  // Q4_0
        case 3:  return QK4_1;  // Q4_1
        case 8:  return QK8_0;  // Q8_0
        default: return 1;
    }
}

/*
 * Load a tensor from GGML file and dequantize it
 *
 * This is what you need to actually USE the model weights!
 */
float* load_and_dequantize_tensor(FILE* f, const TensorInfo* info) {
    // Calculate total number of elements
    int64_t n_elements = 1;
    for (int i = 0; i < info->n_dims; i++) {
        n_elements *= info->dims[i];
    }

    printf("Loading tensor: %s\n", info->name);
    printf("  Type: %d, Dims: [", info->type);
    for (int i = 0; i < info->n_dims; i++) {
        printf("%lld%s", (long long)info->dims[i], i < info->n_dims - 1 ? " × " : "");
    }
    printf("], Elements: %lld\n", (long long)n_elements);

    // Allocate output buffer
    float* output = (float*)malloc(n_elements * sizeof(float));

    // Seek to tensor data
    fseek(f, info->data_offset, SEEK_SET);

    // Load and dequantize based on type
    switch (info->type) {
        case 0: {  // F32 - just read directly
            fread(output, sizeof(float), n_elements, f);
            break;
        }
        case 1: {  // F16
            block_f16* data = (block_f16*)malloc(n_elements * sizeof(block_f16));
            fread(data, sizeof(block_f16), n_elements, f);
            dequantize_row_f16(data, output, n_elements);
            free(data);
            break;
        }
        case 2: {  // Q4_0
            int n_blocks = (n_elements + QK4_0 - 1) / QK4_0;
            block_q4_0* data = (block_q4_0*)malloc(n_blocks * sizeof(block_q4_0));
            fread(data, sizeof(block_q4_0), n_blocks, f);
            dequantize_row_q4_0(data, output, n_elements);
            free(data);
            break;
        }
        case 3: {  // Q4_1
            int n_blocks = (n_elements + QK4_1 - 1) / QK4_1;
            block_q4_1* data = (block_q4_1*)malloc(n_blocks * sizeof(block_q4_1));
            fread(data, sizeof(block_q4_1), n_blocks, f);
            dequantize_row_q4_1(data, output, n_elements);
            free(data);
            break;
        }
        case 8: {  // Q8_0
            int n_blocks = (n_elements + QK8_0 - 1) / QK8_0;
            block_q8_0* data = (block_q8_0*)malloc(n_blocks * sizeof(block_q8_0));
            fread(data, sizeof(block_q8_0), n_blocks, f);
            dequantize_row_q8_0(data, output, n_elements);
            free(data);
            break;
        }
        default:
            fprintf(stderr, "Unsupported type: %d\n", info->type);
            free(output);
            return NULL;
    }

    // Show statistics
    float min_val = output[0], max_val = output[0], sum = 0;
    for (int64_t i = 0; i < n_elements; i++) {
        if (output[i] < min_val) min_val = output[i];
        if (output[i] > max_val) max_val = output[i];
        sum += output[i];
    }
    float mean = sum / n_elements;

    printf("  Dequantized: min=%.6f, max=%.6f, mean=%.6f\n", min_val, max_val, mean);

    return output;
}

// ----------------------------------------------------------------------------
// Example: Parse GGML tensors from model file

void demonstrate_tensor_loading(const char* model_path) {
    FILE* f = fopen(model_path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open model file\n");
        return;
    }

    printf("=== GGML Quantization Demonstration ===\n\n");

    // Skip header (you'd normally parse this)
    // For Whisper models:
    //   - 4 bytes: magic
    //   - 44 bytes: hparams
    //   - mel filters
    //   - vocabulary
    //   - then tensors start

    // This is simplified - real code would parse the full header
    printf("In a real implementation:\n");
    printf("1. Read magic bytes (0x67676d6c = 'ggml')\n");
    printf("2. Read hyperparameters\n");
    printf("3. Read mel filters\n");
    printf("4. Read vocabulary\n");
    printf("5. Loop through tensors:\n\n");

    printf("For each tensor:\n");
    printf("  a) Read n_dims, type, dimensions[], name\n");
    printf("  b) Calculate data size based on type and dimensions\n");
    printf("  c) Read quantized data\n");
    printf("  d) Dequantize using appropriate function\n");
    printf("  e) Store in model structure\n\n");

    printf("Example tensor structure:\n");
    TensorInfo example = {
        .n_dims = 2,
        .type = 2,  // Q4_0
        .dims = {512, 512, 0, 0},
        .data_offset = 0,
        .data_size = (512 * 512 / QK4_0) * sizeof(block_q4_0)
    };
    strcpy(example.name, "encoder.layers.0.self_attn.q_proj.weight");

    printf("  Name: %s\n", example.name);
    printf("  Type: Q4_0 (4-bit quantized)\n");
    printf("  Shape: [%lld × %lld]\n", (long long)example.dims[0], (long long)example.dims[1]);
    printf("  Elements: %lld\n", (long long)(example.dims[0] * example.dims[1]));
    printf("  Size on disk: %zu bytes\n", example.data_size);
    printf("  Size as float32: %zu bytes\n", (size_t)(example.dims[0] * example.dims[1] * 4));
    printf("  Compression: %.1fx\n\n",
           (float)(example.dims[0] * example.dims[1] * 4) / example.data_size);

    fclose(f);
}

// ----------------------------------------------------------------------------
// Quantization Comparison

void demonstrate_quantization() {
    printf("=== Quantization Format Comparison ===\n\n");

    printf("For 32 float values:\n\n");

    printf("Format  | Bits/val | Block Size | Compression | Quality\n");
    printf("--------|----------|------------|-------------|---------\n");
    printf("F32     |   32     | 128 bytes  |    1.0x     | Perfect\n");
    printf("F16     |   16     |  64 bytes  |    2.0x     | Excellent\n");
    printf("Q8_0    |    8     |  34 bytes  |    3.8x     | Very Good\n");
    printf("Q4_1    |    4     |  20 bytes  |    6.4x     | Good\n");
    printf("Q4_0    |    4     |  18 bytes  |    7.1x     | Good (most common)\n\n");

    printf("Example weight values:\n");
    printf("  Original (F32): 0.123456\n");
    printf("  After Q4_0:     ~0.12    (loses some precision)\n");
    printf("  After Q8_0:     ~0.1235  (better precision)\n");
    printf("  After F16:      ~0.12346 (very close)\n\n");

    printf("Why quantization works:\n");
    printf("- Neural network weights have redundancy\n");
    printf("- Small precision loss doesn't hurt much\n");
    printf("- Reduces memory bandwidth → faster inference\n");
    printf("- Enables running larger models\n\n");
}

// ----------------------------------------------------------------------------
// Main

int main(int argc, char** argv) {
    printf("===============================================\n");
    printf("    GGML Quantization - Educational Guide\n");
    printf("===============================================\n\n");

    demonstrate_quantization();

    if (argc > 1) {
        demonstrate_tensor_loading(argv[1]);
    } else {
        printf("To see tensor loading from a real model:\n");
        printf("  %s /path/to/model.bin\n\n", argv[0]);
    }

    // Demonstrate actual dequantization with example data
    printf("=== Live Dequantization Example ===\n\n");

    // Create example Q4_0 block
    block_q4_0 example_block;
    example_block.d = 0x3C00;  // fp16 for 1.0

    // Pack some example 4-bit values
    // Let's encode values: 8, 12, 4, 15, 0, 7, 9, 10...
    example_block.qs[0] = (12 << 4) | 8;   // [8, 12]
    example_block.qs[1] = (15 << 4) | 4;   // [4, 15]
    example_block.qs[2] = (7 << 4) | 0;    // [0, 7]
    example_block.qs[3] = (10 << 4) | 9;   // [9, 10]
    // ... and so on

    float output[QK4_0];
    dequantize_row_q4_0(&example_block, output, QK4_0);

    printf("Dequantized first 8 values:\n");
    for (int i = 0; i < 8; i++) {
        printf("  [%d]: %.2f\n", i, output[i]);
    }
    printf("\n");

    printf("This is how whisper.cpp loads and uses model weights!\n\n");

    return 0;
}
