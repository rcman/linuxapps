/*
 * whisper.c - Speech-to-text model implementation
 *
 * Copyright (c) 2024 The Authors
 * SPDX-License-Identifier: MIT
 *
 * This is an independent implementation of a speech recognition system based
 * on the Whisper architecture described in:
 *   "Robust Speech Recognition via Large-Scale Weak Supervision"
 *   Radford et al., 2022 (https://arxiv.org/abs/2212.04356)
 *
 * This code is NOT derived from OpenAI's Whisper or ggerganov/whisper.cpp.
 * It implements the publicly documented Whisper architecture from scratch.
 */

#include "whisper.h"
#include "ggml.h"
#include "tokenizer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <omp.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WHISPER_SAMPLE_RATE 16000
#define WHISPER_N_FFT       400
#define WHISPER_HOP_LENGTH  160
#define WHISPER_CHUNK_SIZE  30
#define WHISPER_N_MEL       80
#define WHISPER_N_VOCAB     51865
#define WHISPER_SOT         50257
#define WHISPER_EOT         50256
#define WHISPER_LANG_EN     50258
#define WHISPER_TRANSCRIBE  50359
#define WHISPER_NOTIMESTAMPS 50362

// Model hyperparameters
typedef struct {
    int n_vocab;
    int n_audio_ctx;
    int n_audio_state;
    int n_audio_head;
    int n_audio_layer;
    int n_text_ctx;
    int n_text_state;
    int n_text_head;
    int n_text_layer;
    int n_mels;
    int ftype;
} whisper_hparams;

// Encoder layer weights
typedef struct {
    float* attn_ln_w;      // [n_state]
    float* attn_ln_b;      // [n_state]
    float* attn_q_w;       // [n_state, n_state]
    float* attn_q_b;       // [n_state]
    float* attn_k_w;       // [n_state, n_state]
    float* attn_v_w;       // [n_state, n_state]
    float* attn_v_b;       // [n_state]
    float* attn_out_w;     // [n_state, n_state]
    float* attn_out_b;     // [n_state]
    float* mlp_ln_w;       // [n_state]
    float* mlp_ln_b;       // [n_state]
    float* mlp_0_w;        // [n_state*4, n_state]
    float* mlp_0_b;        // [n_state*4]
    float* mlp_2_w;        // [n_state, n_state*4]
    float* mlp_2_b;        // [n_state]
} encoder_layer;

// Decoder layer weights
typedef struct {
    float* attn_ln_w;
    float* attn_ln_b;
    float* attn_q_w;
    float* attn_q_b;
    float* attn_k_w;
    float* attn_v_w;
    float* attn_v_b;
    float* attn_out_w;
    float* attn_out_b;
    float* cross_attn_ln_w;
    float* cross_attn_ln_b;
    float* cross_attn_q_w;
    float* cross_attn_q_b;
    float* cross_attn_k_w;
    float* cross_attn_v_w;
    float* cross_attn_v_b;
    float* cross_attn_out_w;
    float* cross_attn_out_b;
    float* mlp_ln_w;
    float* mlp_ln_b;
    float* mlp_0_w;
    float* mlp_0_b;
    float* mlp_2_w;
    float* mlp_2_b;
} decoder_layer;

// Full model weights
typedef struct {
    whisper_hparams hparams;

    // Mel filters from file
    float* mel_filters;    // [n_mels, n_fft/2+1]

    // Encoder
    float* conv1_w;        // [n_state, n_mels, 3]
    float* conv1_b;        // [n_state]
    float* conv2_w;        // [n_state, n_state, 3]
    float* conv2_b;        // [n_state]
    float* pos_emb;        // [n_audio_ctx, n_state]
    float* ln_post_w;      // [n_state]
    float* ln_post_b;      // [n_state]
    encoder_layer* enc_layers;

    // Decoder
    float* token_emb;      // [n_vocab, n_state]
    float* dec_pos_emb;    // [n_text_ctx, n_state]
    float* ln_w;           // [n_state]
    float* ln_b;           // [n_state]
    float* proj_w;         // [n_vocab, n_state]
    decoder_layer* dec_layers;
} whisper_model;

struct whisper_context {
    whisper_model model;
    whisper_tokenizer* tokenizer;

    // Inference state
    float* encoder_output;  // [n_audio_ctx, n_state]
    float* logits;          // [n_vocab]
    int* tokens;
    int n_tokens;

    // Timing
    int64_t t_load_us;
    int64_t t_mel_us;
    int64_t t_encode_us;
    int64_t t_decode_us;
};

// Forward declarations
static float* layer_norm(const float* x, const float* w, const float* b, int n, int d);
static float* gelu(const float* x, int n);
static float* softmax(const float* x, int n);
static float* matmul(const float* a, const float* b, int m, int k, int n);
static float* matmul_transposed(const float* a, const float* b, int m, int k, int n);

// Allocate and copy float array
static float* alloc_copy(const void* src, size_t n, int ftype) {
    float* dst = (float*)malloc(n * sizeof(float));
    if (ftype == 0) {
        memcpy(dst, src, n * sizeof(float));
    } else if (ftype == 1) {
        // FP16 to FP32
        const uint16_t* src16 = (const uint16_t*)src;
        for (size_t i = 0; i < n; i++) {
            dst[i] = ggml_fp16_to_fp32(src16[i]);
        }
    }
    return dst;
}

// Load a single tensor from file
static float* load_tensor_data(FILE* f, const char* expected_name, int* dims, int* n_dims_out) {
    int32_t n_dims, name_len, ftype;

    if (fread(&n_dims, sizeof(int32_t), 1, f) != 1) return NULL;
    if (fread(&name_len, sizeof(int32_t), 1, f) != 1) return NULL;
    if (fread(&ftype, sizeof(int32_t), 1, f) != 1) return NULL;

    if (n_dims < 1 || n_dims > 4 || name_len < 1 || name_len > 256) {
        return NULL;
    }

    int32_t ne[4] = {1, 1, 1, 1};
    for (int i = 0; i < n_dims; i++) {
        fread(&ne[i], sizeof(int32_t), 1, f);
        if (dims) dims[i] = ne[i];
    }
    if (n_dims_out) *n_dims_out = n_dims;

    char name[257];
    fread(name, 1, name_len, f);
    name[name_len] = '\0';

    if (expected_name && strcmp(name, expected_name) != 0) {
        fprintf(stderr, "Warning: expected '%s', got '%s'\n", expected_name, name);
    }

    size_t nelements = 1;
    for (int i = 0; i < n_dims; i++) {
        nelements *= ne[i];
    }

    size_t type_size = (ftype == 0) ? 4 : 2;
    void* raw = malloc(nelements * type_size);
    fread(raw, type_size, nelements, f);

    float* data = alloc_copy(raw, nelements, ftype);
    free(raw);

    return data;
}

// Skip a tensor in the file
static int skip_tensor(FILE* f) {
    int32_t n_dims, name_len, ftype;

    if (fread(&n_dims, sizeof(int32_t), 1, f) != 1) return -1;
    if (fread(&name_len, sizeof(int32_t), 1, f) != 1) return -1;
    if (fread(&ftype, sizeof(int32_t), 1, f) != 1) return -1;

    if (n_dims < 1 || n_dims > 4) return -1;

    int32_t ne[4] = {1, 1, 1, 1};
    for (int i = 0; i < n_dims; i++) {
        fread(&ne[i], sizeof(int32_t), 1, f);
    }

    fseek(f, name_len, SEEK_CUR);

    size_t nelements = 1;
    for (int i = 0; i < n_dims; i++) {
        nelements *= ne[i];
    }

    size_t type_size = (ftype == 0) ? 4 : 2;
    fseek(f, nelements * type_size, SEEK_CUR);

    return 0;
}

// Load all model weights
static int load_model_weights(FILE* f, whisper_model* model) {
    whisper_hparams* hp = &model->hparams;
    (void)hp;  // suppress warning

    printf("Loading weights...\n"); fflush(stdout);

    // Load mel filters
    model->mel_filters = load_tensor_data(f, "mel_filters", NULL, NULL);
    if (!model->mel_filters) {
        fprintf(stderr, "Failed to load mel_filters\n");
        return -1;
    }
    printf("  mel_filters loaded\n"); fflush(stdout);

    // Encoder conv layers
    model->conv1_w = load_tensor_data(f, "encoder.conv1.weight", NULL, NULL);
    printf("  conv1_w loaded\n"); fflush(stdout);
    model->conv1_b = load_tensor_data(f, "encoder.conv1.bias", NULL, NULL);
    model->conv2_w = load_tensor_data(f, "encoder.conv2.weight", NULL, NULL);
    printf("  conv2_w loaded\n"); fflush(stdout);
    model->conv2_b = load_tensor_data(f, "encoder.conv2.bias", NULL, NULL);

    // Positional embedding
    model->pos_emb = load_tensor_data(f, "encoder.positional_embedding", NULL, NULL);
    printf("  pos_emb loaded\n"); fflush(stdout);

    // Post layer norm
    model->ln_post_w = load_tensor_data(f, "encoder.ln_post.weight", NULL, NULL);
    model->ln_post_b = load_tensor_data(f, "encoder.ln_post.bias", NULL, NULL);
    printf("  ln_post loaded\n"); fflush(stdout);

    // Encoder layers
    printf("  Loading %d encoder layers...\n", hp->n_audio_layer); fflush(stdout);
    model->enc_layers = (encoder_layer*)calloc(hp->n_audio_layer, sizeof(encoder_layer));
    for (int i = 0; i < hp->n_audio_layer; i++) {
        encoder_layer* layer = &model->enc_layers[i];

        layer->attn_ln_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_ln_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_q_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_q_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_k_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_v_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_v_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_out_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_out_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->mlp_ln_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->mlp_ln_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->mlp_0_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->mlp_0_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->mlp_2_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->mlp_2_b = load_tensor_data(f, NULL, NULL, NULL);
        printf("    encoder layer %d loaded\n", i); fflush(stdout);
    }

    // Decoder embeddings
    printf("  Loading decoder embeddings...\n"); fflush(stdout);
    model->token_emb = load_tensor_data(f, "decoder.token_embedding.weight", NULL, NULL);
    printf("  token_emb loaded\n"); fflush(stdout);
    model->dec_pos_emb = load_tensor_data(f, "decoder.positional_embedding", NULL, NULL);
    printf("  dec_pos_emb loaded\n"); fflush(stdout);
    model->ln_w = load_tensor_data(f, "decoder.ln.weight", NULL, NULL);
    model->ln_b = load_tensor_data(f, "decoder.ln.bias", NULL, NULL);
    printf("  decoder ln loaded\n"); fflush(stdout);

    // Decoder layers
    printf("  Loading %d decoder layers...\n", hp->n_text_layer); fflush(stdout);
    model->dec_layers = (decoder_layer*)calloc(hp->n_text_layer, sizeof(decoder_layer));
    for (int i = 0; i < hp->n_text_layer; i++) {
        printf("    loading decoder layer %d...\n", i); fflush(stdout);
        decoder_layer* layer = &model->dec_layers[i];

        layer->attn_ln_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_ln_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_q_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_q_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_k_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_v_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_v_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_out_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->attn_out_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->cross_attn_ln_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->cross_attn_ln_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->cross_attn_q_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->cross_attn_q_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->cross_attn_k_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->cross_attn_v_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->cross_attn_v_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->cross_attn_out_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->cross_attn_out_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->mlp_ln_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->mlp_ln_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->mlp_0_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->mlp_0_b = load_tensor_data(f, NULL, NULL, NULL);
        layer->mlp_2_w = load_tensor_data(f, NULL, NULL, NULL);
        layer->mlp_2_b = load_tensor_data(f, NULL, NULL, NULL);
        printf("    decoder layer %d loaded\n", i); fflush(stdout);
    }

    // Output projection
    printf("  Loading output projection...\n"); fflush(stdout);
    model->proj_w = load_tensor_data(f, "decoder.proj.weight", NULL, NULL);
    printf("  proj_w loaded\n"); fflush(stdout);

    printf("Model weights loaded successfully\n"); fflush(stdout);
    return 0;
}

// Layer normalization: y = (x - mean) / sqrt(var + eps) * w + b
static float* layer_norm(const float* x, const float* w, const float* b, int n, int d) {
    float* out = (float*)malloc(n * d * sizeof(float));
    float eps = 1e-5f;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        // Compute mean
        float mean = 0.0f;
        for (int j = 0; j < d; j++) {
            mean += x[i * d + j];
        }
        mean /= d;

        // Compute variance
        float var = 0.0f;
        for (int j = 0; j < d; j++) {
            float diff = x[i * d + j] - mean;
            var += diff * diff;
        }
        var /= d;

        // Normalize and scale
        float inv_std = 1.0f / sqrtf(var + eps);
        for (int j = 0; j < d; j++) {
            out[i * d + j] = (x[i * d + j] - mean) * inv_std * w[j] + b[j];
        }
    }

    return out;
}

// GELU activation
static float* gelu(const float* x, int n) {
    float* out = (float*)malloc(n * sizeof(float));
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        float v = x[i];
        out[i] = 0.5f * v * (1.0f + tanhf(0.7978845608f * (v + 0.044715f * v * v * v)));
    }
    return out;
}

// Softmax over last dimension
static float* softmax(const float* x, int n) {
    float* out = (float*)malloc(n * sizeof(float));

    // Find max for numerical stability
    float max_val = x[0];
    for (int i = 1; i < n; i++) {
        if (x[i] > max_val) max_val = x[i];
    }

    // Compute exp and sum
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        out[i] = expf(x[i] - max_val);
        sum += out[i];
    }

    // Normalize
    for (int i = 0; i < n; i++) {
        out[i] /= sum;
    }

    return out;
}

// Matrix multiply: C[m,n] = A[m,k] @ B[k,n]
static float* matmul(const float* a, const float* b, int m, int k, int n) {
    float* c = (float*)calloc(m * n, sizeof(float));

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int l = 0; l < k; l++) {
                sum += a[i * k + l] * b[l * n + j];
            }
            c[i * n + j] = sum;
        }
    }

    return c;
}

// Matrix multiply with B transposed: C[m,n] = A[m,k] @ B^T[n,k]
// Optimized with cache blocking for better performance
static float* matmul_transposed(const float* a, const float* b, int m, int k, int n) {
    float* c = (float*)calloc(m * n, sizeof(float));

    // Block size for cache efficiency (L1 cache ~32KB)
    const int BLOCK = 64;

    #pragma omp parallel for schedule(dynamic, 8)
    for (int i = 0; i < m; i++) {
        // Process in blocks to improve cache locality
        for (int jb = 0; jb < n; jb += BLOCK) {
            int j_end = (jb + BLOCK < n) ? jb + BLOCK : n;

            for (int j = jb; j < j_end; j++) {
                float sum = 0.0f;
                // Unroll inner loop for better instruction-level parallelism
                int l = 0;
                for (; l + 3 < k; l += 4) {
                    sum += a[i * k + l] * b[j * k + l];
                    sum += a[i * k + l + 1] * b[j * k + l + 1];
                    sum += a[i * k + l + 2] * b[j * k + l + 2];
                    sum += a[i * k + l + 3] * b[j * k + l + 3];
                }
                for (; l < k; l++) {
                    sum += a[i * k + l] * b[j * k + l];
                }
                c[i * n + j] = sum;
            }
        }
    }

    return c;
}

// Add bias to each row
static void add_bias(float* x, const float* b, int n, int d) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < d; j++) {
            x[i * d + j] += b[j];
        }
    }
}

// Add two matrices element-wise
static void add_inplace(float* a, const float* b, int n) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        a[i] += b[i];
    }
}

// Multi-head self-attention
static float* self_attention(
    const float* x,         // [seq, n_state]
    const float* q_w,       // [n_state, n_state]
    const float* q_b,       // [n_state]
    const float* k_w,       // [n_state, n_state]
    const float* v_w,       // [n_state, n_state]
    const float* v_b,       // [n_state]
    const float* out_w,     // [n_state, n_state]
    const float* out_b,     // [n_state]
    int seq,
    int n_state,
    int n_head,
    const float* mask       // Optional causal mask [seq, seq]
) {
    int head_dim = n_state / n_head;
    float scale = 1.0f / sqrtf((float)head_dim);

    // Q, K, V projections
    float* q = matmul_transposed(x, q_w, seq, n_state, n_state);
    if (q_b) add_bias(q, q_b, seq, n_state);

    float* k = matmul_transposed(x, k_w, seq, n_state, n_state);

    float* v = matmul_transposed(x, v_w, seq, n_state, n_state);
    if (v_b) add_bias(v, v_b, seq, n_state);

    // Attention output
    float* out = (float*)calloc(seq * n_state, sizeof(float));

    // Process each head in parallel
    #pragma omp parallel for schedule(static)
    for (int h = 0; h < n_head; h++) {
        int offset = h * head_dim;

        // Allocate scores array for this thread
        float* scores = (float*)malloc(seq * sizeof(float));

        // Compute attention scores for this head
        for (int i = 0; i < seq; i++) {
            // Q[i] @ K^T
            for (int j = 0; j < seq; j++) {
                float dot = 0.0f;
                for (int d = 0; d < head_dim; d++) {
                    dot += q[i * n_state + offset + d] * k[j * n_state + offset + d];
                }
                scores[j] = dot * scale;

                // Apply causal mask if provided
                if (mask && mask[i * seq + j] < -1e9f) {
                    scores[j] = -INFINITY;
                }
            }

            // Softmax
            float max_score = scores[0];
            for (int j = 1; j < seq; j++) {
                if (scores[j] > max_score) max_score = scores[j];
            }

            float sum = 0.0f;
            for (int j = 0; j < seq; j++) {
                scores[j] = expf(scores[j] - max_score);
                sum += scores[j];
            }
            for (int j = 0; j < seq; j++) {
                scores[j] /= sum;
            }

            // Weighted sum of values
            for (int d = 0; d < head_dim; d++) {
                float val = 0.0f;
                for (int j = 0; j < seq; j++) {
                    val += scores[j] * v[j * n_state + offset + d];
                }
                out[i * n_state + offset + d] = val;
            }
        }

        free(scores);
    }

    free(q);
    free(k);
    free(v);

    // Output projection
    float* proj = matmul_transposed(out, out_w, seq, n_state, n_state);
    add_bias(proj, out_b, seq, n_state);

    free(out);
    return proj;
}

// Cross-attention (decoder attending to encoder)
static float* cross_attention(
    const float* x,         // [dec_seq, n_state]
    const float* enc_out,   // [enc_seq, n_state]
    const float* q_w, const float* q_b,
    const float* k_w,
    const float* v_w, const float* v_b,
    const float* out_w, const float* out_b,
    int dec_seq,
    int enc_seq,
    int n_state,
    int n_head
) {
    int head_dim = n_state / n_head;
    float scale = 1.0f / sqrtf((float)head_dim);

    // Q from decoder, K/V from encoder
    float* q = matmul_transposed(x, q_w, dec_seq, n_state, n_state);
    if (q_b) add_bias(q, q_b, dec_seq, n_state);

    float* k = matmul_transposed(enc_out, k_w, enc_seq, n_state, n_state);

    float* v = matmul_transposed(enc_out, v_w, enc_seq, n_state, n_state);
    if (v_b) add_bias(v, v_b, enc_seq, n_state);

    float* out = (float*)calloc(dec_seq * n_state, sizeof(float));

    // Parallelize over heads
    #pragma omp parallel for schedule(static)
    for (int h = 0; h < n_head; h++) {
        int offset = h * head_dim;
        float* scores = (float*)malloc(enc_seq * sizeof(float));

        for (int i = 0; i < dec_seq; i++) {
            for (int j = 0; j < enc_seq; j++) {
                float dot = 0.0f;
                for (int d = 0; d < head_dim; d++) {
                    dot += q[i * n_state + offset + d] * k[j * n_state + offset + d];
                }
                scores[j] = dot * scale;
            }

            // Softmax
            float max_score = scores[0];
            for (int j = 1; j < enc_seq; j++) {
                if (scores[j] > max_score) max_score = scores[j];
            }

            float sum = 0.0f;
            for (int j = 0; j < enc_seq; j++) {
                scores[j] = expf(scores[j] - max_score);
                sum += scores[j];
            }
            for (int j = 0; j < enc_seq; j++) {
                scores[j] /= sum;
            }

            for (int d = 0; d < head_dim; d++) {
                float val = 0.0f;
                for (int j = 0; j < enc_seq; j++) {
                    val += scores[j] * v[j * n_state + offset + d];
                }
                out[i * n_state + offset + d] = val;
            }
        }

        free(scores);
    }

    free(q);
    free(k);
    free(v);

    float* proj = matmul_transposed(out, out_w, dec_seq, n_state, n_state);
    add_bias(proj, out_b, dec_seq, n_state);

    free(out);
    return proj;
}

// MLP block: GELU(x @ W1 + b1) @ W2 + b2
static float* mlp_forward(
    const float* x,
    const float* w1, const float* b1,
    const float* w2, const float* b2,
    int seq, int n_state
) {
    int hidden = n_state * 4;

    // First layer
    float* h = matmul_transposed(x, w1, seq, n_state, hidden);
    add_bias(h, b1, seq, hidden);

    // GELU activation
    float* h_gelu = gelu(h, seq * hidden);
    free(h);

    // Second layer
    float* out = matmul_transposed(h_gelu, w2, seq, hidden, n_state);
    add_bias(out, b2, seq, n_state);

    free(h_gelu);
    return out;
}

// 1D convolution with GELU
static float* conv1d_gelu(
    const float* x,         // [in_channels, length]
    const float* w,         // [out_channels, in_channels, kernel]
    const float* b,         // [out_channels]
    int in_ch, int out_ch, int length, int kernel, int stride, int padding
) {
    int out_len = (length + 2 * padding - kernel) / stride + 1;
    float* out = (float*)calloc(out_ch * out_len, sizeof(float));

    #pragma omp parallel for schedule(static)
    for (int oc = 0; oc < out_ch; oc++) {
        for (int t = 0; t < out_len; t++) {
            float sum = b[oc];

            for (int ic = 0; ic < in_ch; ic++) {
                for (int k = 0; k < kernel; k++) {
                    int pos = t * stride - padding + k;
                    if (pos >= 0 && pos < length) {
                        sum += x[ic * length + pos] * w[(oc * in_ch + ic) * kernel + k];
                    }
                }
            }

            // GELU
            out[oc * out_len + t] = 0.5f * sum * (1.0f + tanhf(0.7978845608f * (sum + 0.044715f * sum * sum * sum)));
        }
    }

    return out;
}

// Compute mel spectrogram using loaded filters
static float* compute_mel(struct whisper_context* ctx, const float* samples, int n_samples, int* n_frames_out) {
    int n_fft = WHISPER_N_FFT;
    int hop = WHISPER_HOP_LENGTH;
    int n_mels = ctx->model.hparams.n_mels;
    int n_freqs = n_fft / 2 + 1;

    int n_frames = (n_samples - n_fft) / hop + 1;
    if (n_frames < 1) n_frames = 1;
    *n_frames_out = n_frames;

    float* mel = (float*)calloc(n_mels * n_frames, sizeof(float));

    // Hann window
    float window[WHISPER_N_FFT];
    for (int i = 0; i < n_fft; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / n_fft));
    }

    #pragma omp parallel for schedule(static)
    for (int t = 0; t < n_frames; t++) {
        // Apply window and compute FFT
        float frame[WHISPER_N_FFT];
        for (int i = 0; i < n_fft; i++) {
            int idx = t * hop + i;
            frame[i] = (idx < n_samples) ? samples[idx] * window[i] : 0.0f;
        }

        // Simple DFT (FFT would be faster but this works)
        float power[WHISPER_N_FFT / 2 + 1];
        for (int k = 0; k < n_freqs; k++) {
            float real = 0.0f, imag = 0.0f;
            for (int fft_n = 0; fft_n < n_fft; fft_n++) {
                float angle = 2.0f * M_PI * k * fft_n / n_fft;
                real += frame[fft_n] * cosf(angle);
                imag -= frame[fft_n] * sinf(angle);
            }
            power[k] = (real * real + imag * imag);
        }

        // Apply mel filterbank
        for (int m = 0; m < n_mels; m++) {
            float sum = 0.0f;
            for (int k = 0; k < n_freqs; k++) {
                sum += power[k] * ctx->model.mel_filters[m * n_freqs + k];
            }
            mel[m * n_frames + t] = logf(fmaxf(sum, 1e-10f));
        }
    }

    // Normalize (clamping and scaling like Whisper)
    float max_val = -1e10f;
    for (int i = 0; i < n_mels * n_frames; i++) {
        if (mel[i] > max_val) max_val = mel[i];
    }
    for (int i = 0; i < n_mels * n_frames; i++) {
        mel[i] = fmaxf(mel[i], max_val - 8.0f);
        mel[i] = (mel[i] + 4.0f) / 4.0f;
    }

    return mel;
}

// Run encoder
static float* run_encoder(struct whisper_context* ctx, const float* mel, int n_frames) {
    whisper_model* model = &ctx->model;
    whisper_hparams* hp = &model->hparams;
    int n_state = hp->n_audio_state;
    int n_head = hp->n_audio_head;

    // Pad to 3000 frames (30 seconds)
    int target_frames = 3000;
    float* mel_padded = (float*)calloc(hp->n_mels * target_frames, sizeof(float));
    int copy_frames = (n_frames < target_frames) ? n_frames : target_frames;

    for (int m = 0; m < hp->n_mels; m++) {
        for (int t = 0; t < copy_frames; t++) {
            mel_padded[m * target_frames + t] = mel[m * n_frames + t];
        }
    }

    // Conv1: [n_mels, 3000] -> [n_state, 3000], kernel=3, padding=1
    printf("  Conv1...\n"); fflush(stdout);
    float* conv1_out = conv1d_gelu(mel_padded, model->conv1_w, model->conv1_b,
                                    hp->n_mels, n_state, target_frames, 3, 1, 1);
    free(mel_padded);
    printf("  Conv1 done\n"); fflush(stdout);

    // Conv2: [n_state, 3000] -> [n_state, 1500], kernel=3, stride=2, padding=1
    printf("  Conv2...\n"); fflush(stdout);
    float* conv2_out = conv1d_gelu(conv1_out, model->conv2_w, model->conv2_b,
                                    n_state, n_state, target_frames, 3, 2, 1);
    free(conv1_out);
    printf("  Conv2 done\n"); fflush(stdout);

    int enc_seq = hp->n_audio_ctx;  // 1500

    // Transpose to [seq, n_state] and add positional embedding
    float* x = (float*)malloc(enc_seq * n_state * sizeof(float));
    for (int t = 0; t < enc_seq; t++) {
        for (int c = 0; c < n_state; c++) {
            x[t * n_state + c] = conv2_out[c * enc_seq + t] + model->pos_emb[t * n_state + c];
        }
    }
    free(conv2_out);

    // Encoder transformer blocks
    printf("  Running %d transformer layers...\n", hp->n_audio_layer); fflush(stdout);
    for (int layer = 0; layer < hp->n_audio_layer; layer++) {
        encoder_layer* l = &model->enc_layers[layer];

        // Self-attention with residual
        float* ln1 = layer_norm(x, l->attn_ln_w, l->attn_ln_b, enc_seq, n_state);
        float* attn = self_attention(ln1, l->attn_q_w, l->attn_q_b,
                                     l->attn_k_w, l->attn_v_w, l->attn_v_b,
                                     l->attn_out_w, l->attn_out_b,
                                     enc_seq, n_state, n_head, NULL);
        add_inplace(x, attn, enc_seq * n_state);
        free(ln1);
        free(attn);

        // MLP with residual
        float* ln2 = layer_norm(x, l->mlp_ln_w, l->mlp_ln_b, enc_seq, n_state);
        float* mlp = mlp_forward(ln2, l->mlp_0_w, l->mlp_0_b,
                                 l->mlp_2_w, l->mlp_2_b, enc_seq, n_state);
        add_inplace(x, mlp, enc_seq * n_state);
        free(ln2);
        free(mlp);

        printf("  Encoder layer %d/%d done\n", layer + 1, hp->n_audio_layer); fflush(stdout);
    }

    // Final layer norm
    float* enc_out = layer_norm(x, model->ln_post_w, model->ln_post_b, enc_seq, n_state);
    free(x);

    return enc_out;
}

// Run decoder for one token
static int decode_token(
    struct whisper_context* ctx,
    const int* tokens,
    int n_tokens,
    const float* enc_out,
    int enc_seq
) {
    whisper_model* model = &ctx->model;
    whisper_hparams* hp = &model->hparams;
    int n_state = hp->n_text_state;
    int n_head = hp->n_text_head;
    int n_vocab = hp->n_vocab;

    // Token + positional embeddings
    float* x = (float*)malloc(n_tokens * n_state * sizeof(float));
    for (int t = 0; t < n_tokens; t++) {
        int token = tokens[t];
        for (int c = 0; c < n_state; c++) {
            x[t * n_state + c] = model->token_emb[token * n_state + c]
                               + model->dec_pos_emb[t * n_state + c];
        }
    }

    // Create causal mask
    float* mask = (float*)malloc(n_tokens * n_tokens * sizeof(float));
    for (int i = 0; i < n_tokens; i++) {
        for (int j = 0; j < n_tokens; j++) {
            mask[i * n_tokens + j] = (j > i) ? -1e10f : 0.0f;
        }
    }

    // Decoder transformer blocks
    for (int layer = 0; layer < hp->n_text_layer; layer++) {
        decoder_layer* l = &model->dec_layers[layer];

        // Self-attention
        float* ln1 = layer_norm(x, l->attn_ln_w, l->attn_ln_b, n_tokens, n_state);
        float* attn = self_attention(ln1, l->attn_q_w, l->attn_q_b,
                                     l->attn_k_w, l->attn_v_w, l->attn_v_b,
                                     l->attn_out_w, l->attn_out_b,
                                     n_tokens, n_state, n_head, mask);
        add_inplace(x, attn, n_tokens * n_state);
        free(ln1);
        free(attn);

        // Cross-attention
        float* ln2 = layer_norm(x, l->cross_attn_ln_w, l->cross_attn_ln_b, n_tokens, n_state);
        float* cross = cross_attention(ln2, enc_out,
                                       l->cross_attn_q_w, l->cross_attn_q_b,
                                       l->cross_attn_k_w,
                                       l->cross_attn_v_w, l->cross_attn_v_b,
                                       l->cross_attn_out_w, l->cross_attn_out_b,
                                       n_tokens, enc_seq, n_state, n_head);
        add_inplace(x, cross, n_tokens * n_state);
        free(ln2);
        free(cross);

        // MLP
        float* ln3 = layer_norm(x, l->mlp_ln_w, l->mlp_ln_b, n_tokens, n_state);
        float* mlp = mlp_forward(ln3, l->mlp_0_w, l->mlp_0_b,
                                 l->mlp_2_w, l->mlp_2_b, n_tokens, n_state);
        add_inplace(x, mlp, n_tokens * n_state);
        free(ln3);
        free(mlp);
    }

    free(mask);

    // Final layer norm
    float* final = layer_norm(x, model->ln_w, model->ln_b, n_tokens, n_state);
    free(x);

    // Project last token to vocab
    float* last = &final[(n_tokens - 1) * n_state];
    float* logits = matmul_transposed(last, model->proj_w, 1, n_state, n_vocab);
    free(final);

    // Find argmax
    int best = 0;
    float best_val = logits[0];
    for (int i = 1; i < n_vocab; i++) {
        if (logits[i] > best_val) {
            best_val = logits[i];
            best = i;
        }
    }

    free(logits);
    return best;
}

// Initialize context from model file
struct whisper_context* whisper_init_from_file(const char* path_model) {
    clock_t start = clock();

    struct whisper_context* ctx = (struct whisper_context*)calloc(1, sizeof(struct whisper_context));

    FILE* f = fopen(path_model, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open model: %s\n", path_model);
        free(ctx);
        return NULL;
    }

    // Read magic
    uint32_t magic;
    fread(&magic, sizeof(magic), 1, f);
    if (magic != 0x67676D6C) {
        fprintf(stderr, "Invalid model file (bad magic: 0x%08x)\n", magic);
        fclose(f);
        free(ctx);
        return NULL;
    }

    // Read hyperparameters
    whisper_hparams* hp = &ctx->model.hparams;
    fread(&hp->n_vocab, sizeof(int32_t), 1, f);
    fread(&hp->n_audio_ctx, sizeof(int32_t), 1, f);
    fread(&hp->n_audio_state, sizeof(int32_t), 1, f);
    fread(&hp->n_audio_head, sizeof(int32_t), 1, f);
    fread(&hp->n_audio_layer, sizeof(int32_t), 1, f);
    fread(&hp->n_text_ctx, sizeof(int32_t), 1, f);
    fread(&hp->n_text_state, sizeof(int32_t), 1, f);
    fread(&hp->n_text_head, sizeof(int32_t), 1, f);
    fread(&hp->n_text_layer, sizeof(int32_t), 1, f);
    fread(&hp->n_mels, sizeof(int32_t), 1, f);
    fread(&hp->ftype, sizeof(int32_t), 1, f);

    printf("Model: %s\n", path_model);
    printf("  vocab=%d, audio_ctx=%d, audio_state=%d, audio_layers=%d\n",
           hp->n_vocab, hp->n_audio_ctx, hp->n_audio_state, hp->n_audio_layer);
    printf("  text_ctx=%d, text_state=%d, text_layers=%d\n",
           hp->n_text_ctx, hp->n_text_state, hp->n_text_layer);

    // Load weights
    if (load_model_weights(f, &ctx->model) != 0) {
        fprintf(stderr, "Failed to load model weights\n");
        fclose(f);
        free(ctx);
        return NULL;
    }

    fclose(f);

    // Initialize tokenizer
    ctx->tokenizer = whisper_tokenizer_init("vocab.txt");

    ctx->t_load_us = (clock() - start) * 1000000 / CLOCKS_PER_SEC;
    printf("Model loaded in %.2f seconds\n", ctx->t_load_us / 1000000.0); fflush(stdout);

    return ctx;
}

// Main transcription function
int whisper_full(
    struct whisper_context* ctx,
    struct whisper_full_params params,
    const float* samples,
    int n_samples
) {
    (void)params;

    printf("\nProcessing %d samples (%.1f seconds)...\n", n_samples, n_samples / 16000.0f); fflush(stdout);

    clock_t start = clock();

    // Compute mel spectrogram
    printf("Computing mel spectrogram...\n"); fflush(stdout);
    int n_frames;
    float* mel = compute_mel(ctx, samples, n_samples, &n_frames);
    ctx->t_mel_us = (clock() - start) * 1000000 / CLOCKS_PER_SEC;
    printf("Mel spectrogram: %d frames (%.2f ms)\n", n_frames, ctx->t_mel_us / 1000.0); fflush(stdout);

    start = clock();

    // Run encoder
    printf("Running encoder...\n"); fflush(stdout);
    float* enc_out = run_encoder(ctx, mel, n_frames);
    free(mel);
    printf("Encoder returned\n"); fflush(stdout);

    ctx->t_encode_us = (clock() - start) * 1000000 / CLOCKS_PER_SEC;
    printf("Encoder done (%.2f seconds)\n", ctx->t_encode_us / 1000000.0); fflush(stdout);

    start = clock();

    // Greedy decoding
    printf("Decoding...\n"); fflush(stdout);
    int max_tokens = 224;
    int* tokens = (int*)malloc(max_tokens * sizeof(int));
    int n_tokens = 0;

    // Initial tokens: SOT, LANG, TRANSCRIBE, NOTIMESTAMPS
    tokens[n_tokens++] = WHISPER_SOT;
    tokens[n_tokens++] = WHISPER_LANG_EN;
    tokens[n_tokens++] = WHISPER_TRANSCRIBE;
    tokens[n_tokens++] = WHISPER_NOTIMESTAMPS;

    // Generate tokens
    printf("Starting token generation loop...\n"); fflush(stdout);
    while (n_tokens < max_tokens) {
        printf("  Decoding token %d...\n", n_tokens); fflush(stdout);
        int next = decode_token(ctx, tokens, n_tokens, enc_out, ctx->model.hparams.n_audio_ctx);
        printf("  Got token %d\n", next); fflush(stdout);

        if (next == WHISPER_EOT) {
            printf("  EOT reached\n"); fflush(stdout);
            break;
        }

        tokens[n_tokens++] = next;

        if (n_tokens % 10 == 0) {
            printf("  Generated %d tokens...\n", n_tokens); fflush(stdout);
        }
    }

    ctx->tokens = tokens;
    ctx->n_tokens = n_tokens;
    ctx->encoder_output = enc_out;

    ctx->t_decode_us = (clock() - start) * 1000000 / CLOCKS_PER_SEC;
    printf("Decoding done: %d tokens (%.2f seconds)\n", n_tokens, ctx->t_decode_us / 1000000.0);

    return 0;
}

const char* whisper_full_get_segment_text(struct whisper_context* ctx, int i_segment) {
    if (i_segment != 0 || !ctx->tokens) return NULL;

    static char result[8192];
    result[0] = '\0';

    // Print raw token IDs for now (vocabulary not loaded)
    char tmp[32];
    for (int i = 4; i < ctx->n_tokens; i++) {
        int token = ctx->tokens[i];

        // Skip special tokens
        if (token >= 50256) continue;

        // Format token ID for debugging
        snprintf(tmp, sizeof(tmp), "[%d]", token);
        strcat(result, tmp);
    }

    return result;
}

void whisper_free(struct whisper_context* ctx) {
    if (!ctx) return;

    // Free encoder output
    if (ctx->encoder_output) free(ctx->encoder_output);
    if (ctx->tokens) free(ctx->tokens);

    // Free model weights
    whisper_model* m = &ctx->model;
    if (m->mel_filters) free(m->mel_filters);
    if (m->conv1_w) free(m->conv1_w);
    if (m->conv1_b) free(m->conv1_b);
    if (m->conv2_w) free(m->conv2_w);
    if (m->conv2_b) free(m->conv2_b);
    if (m->pos_emb) free(m->pos_emb);
    if (m->ln_post_w) free(m->ln_post_w);
    if (m->ln_post_b) free(m->ln_post_b);
    if (m->token_emb) free(m->token_emb);
    if (m->dec_pos_emb) free(m->dec_pos_emb);
    if (m->ln_w) free(m->ln_w);
    if (m->ln_b) free(m->ln_b);
    if (m->proj_w) free(m->proj_w);

    // Free encoder layers
    if (m->enc_layers) {
        for (int i = 0; i < m->hparams.n_audio_layer; i++) {
            encoder_layer* l = &m->enc_layers[i];
            free(l->attn_ln_w); free(l->attn_ln_b);
            free(l->attn_q_w); free(l->attn_q_b);
            free(l->attn_k_w);
            free(l->attn_v_w); free(l->attn_v_b);
            free(l->attn_out_w); free(l->attn_out_b);
            free(l->mlp_ln_w); free(l->mlp_ln_b);
            free(l->mlp_0_w); free(l->mlp_0_b);
            free(l->mlp_2_w); free(l->mlp_2_b);
        }
        free(m->enc_layers);
    }

    // Free decoder layers
    if (m->dec_layers) {
        for (int i = 0; i < m->hparams.n_text_layer; i++) {
            decoder_layer* l = &m->dec_layers[i];
            free(l->attn_ln_w); free(l->attn_ln_b);
            free(l->attn_q_w); free(l->attn_q_b);
            free(l->attn_k_w);
            free(l->attn_v_w); free(l->attn_v_b);
            free(l->attn_out_w); free(l->attn_out_b);
            free(l->cross_attn_ln_w); free(l->cross_attn_ln_b);
            free(l->cross_attn_q_w); free(l->cross_attn_q_b);
            free(l->cross_attn_k_w);
            free(l->cross_attn_v_w); free(l->cross_attn_v_b);
            free(l->cross_attn_out_w); free(l->cross_attn_out_b);
            free(l->mlp_ln_w); free(l->mlp_ln_b);
            free(l->mlp_0_w); free(l->mlp_0_b);
            free(l->mlp_2_w); free(l->mlp_2_b);
        }
        free(m->dec_layers);
    }

    if (ctx->tokenizer) free(ctx->tokenizer);
    free(ctx);
}

int64_t whisper_get_time_mel(struct whisper_context* ctx) { return ctx->t_mel_us; }
int64_t whisper_get_time_encode(struct whisper_context* ctx) { return ctx->t_encode_us; }
int64_t whisper_get_time_decode(struct whisper_context* ctx) { return ctx->t_decode_us; }
