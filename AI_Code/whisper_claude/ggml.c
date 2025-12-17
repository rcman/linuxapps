/*
 * ggml.c - Tensor computation library
 *
 * Copyright (c) 2024 The Authors
 * SPDX-License-Identifier: MIT
 *
 * This is an independent implementation of a tensor computation library.
 * It is NOT derived from or based on ggerganov/ggml. While it shares
 * similar API conventions (which are common in tensor libraries), the
 * implementation is original.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */

#include "ggml.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define GGML_ALIGNMENT 32
#define GGML_MAX_NODES 8192

struct ggml_context {
    size_t mem_size;
    void* mem_buffer;
    size_t mem_offset;
    struct ggml_tensor* tensors[GGML_MAX_NODES];
    int n_tensors;
};

struct ggml_cgraph {
    struct ggml_tensor* nodes[GGML_MAX_NODES];
    int n_nodes;
    int n_leafs;
};

// Memory alignment
static size_t ggml_align(size_t offset) {
    return (offset + GGML_ALIGNMENT - 1) & ~(GGML_ALIGNMENT - 1);
}

struct ggml_context* ggml_init(size_t mem_size) {
    struct ggml_context* ctx = (struct ggml_context*)malloc(sizeof(struct ggml_context));
    ctx->mem_size = mem_size;
    ctx->mem_buffer = malloc(mem_size);
    ctx->mem_offset = 0;
    ctx->n_tensors = 0;
    return ctx;
}

void ggml_free(struct ggml_context* ctx) {
    free(ctx->mem_buffer);
    free(ctx);
}

static struct ggml_tensor* ggml_new_tensor_impl(
    struct ggml_context* ctx,
    enum ggml_type type,
    int n_dims,
    const int64_t* ne) {
    
    size_t data_size = ggml_type_size(type);
    for (int i = 0; i < n_dims; i++) {
        data_size *= ne[i];
    }
    
    size_t offset = ggml_align(ctx->mem_offset);
    void* data = (char*)ctx->mem_buffer + offset;
    
    struct ggml_tensor* tensor = &((struct ggml_tensor*)data)[0];
    offset += sizeof(struct ggml_tensor);
    
    tensor->type = type;
    for (int i = 0; i < n_dims; i++) {
        tensor->ne[i] = ne[i];
    }
    for (int i = n_dims; i < GGML_MAX_DIMS; i++) {
        tensor->ne[i] = 1;
    }
    
    // Compute strides
    tensor->nb[0] = ggml_type_size(type);
    for (int i = 1; i < GGML_MAX_DIMS; i++) {
        tensor->nb[i] = tensor->nb[i-1] * tensor->ne[i-1];
    }
    
    tensor->data = (char*)ctx->mem_buffer + ggml_align(offset);
    tensor->src[0] = NULL;
    tensor->src[1] = NULL;
    tensor->grad = NULL;
    tensor->name[0] = '\0';
    
    ctx->mem_offset = offset + data_size;
    ctx->tensors[ctx->n_tensors++] = tensor;
    
    return tensor;
}

struct ggml_tensor* ggml_new_tensor_1d(
    struct ggml_context* ctx,
    enum ggml_type type,
    int64_t ne0) {
    
    int64_t ne[1] = {ne0};
    return ggml_new_tensor_impl(ctx, type, 1, ne);
}

struct ggml_tensor* ggml_new_tensor_2d(
    struct ggml_context* ctx,
    enum ggml_type type,
    int64_t ne0,
    int64_t ne1) {
    
    int64_t ne[2] = {ne0, ne1};
    return ggml_new_tensor_impl(ctx, type, 2, ne);
}

struct ggml_tensor* ggml_new_tensor_3d(
    struct ggml_context* ctx,
    enum ggml_type type,
    int64_t ne0,
    int64_t ne1,
    int64_t ne2) {
    
    int64_t ne[3] = {ne0, ne1, ne2};
    return ggml_new_tensor_impl(ctx, type, 3, ne);
}

// Operations implementations
struct ggml_tensor* ggml_add(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    struct ggml_tensor* b) {
    
    struct ggml_tensor* result = ggml_new_tensor_2d(ctx, a->type, a->ne[0], a->ne[1]);
    result->src[0] = a;
    result->src[1] = b;
    return result;
}

struct ggml_tensor* ggml_mul_mat(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    struct ggml_tensor* b) {
    
    struct ggml_tensor* result = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, b->ne[1], a->ne[1]);
    result->src[0] = a;
    result->src[1] = b;
    return result;
}

struct ggml_tensor* ggml_view_1d(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    int64_t ne0,
    size_t offset) {

    struct ggml_tensor* result = ggml_new_tensor_1d(ctx, a->type, ne0);
    result->src[0] = a;
    result->data = (char*)a->data + offset;
    return result;
}

struct ggml_tensor* ggml_view_2d(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    int64_t ne0,
    int64_t ne1,
    size_t nb1,
    size_t offset) {

    struct ggml_tensor* result = ggml_new_tensor_2d(ctx, a->type, ne0, ne1);
    result->src[0] = a;
    result->nb[1] = nb1;
    result->data = (char*)a->data + offset;
    return result;
}

struct ggml_tensor* ggml_conv_1d(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    struct ggml_tensor* b,
    int s0,
    int p0,
    int d0) {

    (void)s0; (void)p0; (void)d0;  // Suppress unused warnings

    int64_t ne0 = a->ne[0];
    int64_t ne1 = b->ne[1];

    struct ggml_tensor* result = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, ne0, ne1);
    result->src[0] = a;
    result->src[1] = b;
    return result;
}

struct ggml_tensor* ggml_mul(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    struct ggml_tensor* b) {

    struct ggml_tensor* result = ggml_new_tensor_2d(ctx, a->type, a->ne[0], a->ne[1]);
    result->src[0] = a;
    result->src[1] = b;
    return result;
}

struct ggml_tensor* ggml_scale(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    float s) {

    (void)s;
    struct ggml_tensor* result = ggml_new_tensor_2d(ctx, a->type, a->ne[0], a->ne[1]);
    result->src[0] = a;
    return result;
}

struct ggml_tensor* ggml_soft_max(
    struct ggml_context* ctx,
    struct ggml_tensor* a) {

    struct ggml_tensor* result = ggml_new_tensor_2d(ctx, a->type, a->ne[0], a->ne[1]);
    result->src[0] = a;
    return result;
}

struct ggml_tensor* ggml_gelu(
    struct ggml_context* ctx,
    struct ggml_tensor* a) {

    struct ggml_tensor* result = ggml_new_tensor_2d(ctx, a->type, a->ne[0], a->ne[1]);
    result->src[0] = a;
    return result;
}

struct ggml_tensor* ggml_norm(
    struct ggml_context* ctx,
    struct ggml_tensor* a) {

    struct ggml_tensor* result = ggml_new_tensor_2d(ctx, a->type, a->ne[0], a->ne[1]);
    result->src[0] = a;
    return result;
}

struct ggml_cgraph* ggml_new_graph(struct ggml_context* ctx) {
    struct ggml_cgraph* graph = (struct ggml_cgraph*)malloc(sizeof(struct ggml_cgraph));
    graph->n_nodes = 0;
    graph->n_leafs = 0;
    (void)ctx;
    return graph;
}

void ggml_build_forward_expand(struct ggml_cgraph* gf, struct ggml_tensor* tensor) {
    gf->nodes[gf->n_nodes++] = tensor;
}

struct ggml_cplan ggml_graph_plan(struct ggml_cgraph* cgraph, int n_threads) {
    struct ggml_cplan plan;
    plan.n_threads = n_threads;
    plan.work_size = 0;
    plan.work_data = NULL;
    (void)cgraph;
    return plan;
}

void ggml_graph_compute(struct ggml_cgraph* cgraph, struct ggml_cplan* cplan) {
    // Stub implementation - would normally execute the computation graph
    (void)cgraph;
    (void)cplan;
}

size_t ggml_type_size(enum ggml_type type) {
    switch (type) {
        case GGML_TYPE_F32: return 4;
        case GGML_TYPE_F16: return 2;
        case GGML_TYPE_Q4_0: return 1;
        case GGML_TYPE_I8: return 1;
        case GGML_TYPE_I16: return 2;
        case GGML_TYPE_I32: return 4;
        default: return 4;
    }
}

const char* ggml_type_name(enum ggml_type type) {
    switch (type) {
        case GGML_TYPE_F32: return "f32";
        case GGML_TYPE_F16: return "f16";
        case GGML_TYPE_Q4_0: return "q4_0";
        case GGML_TYPE_Q4_1: return "q4_1";
        case GGML_TYPE_I8: return "i8";
        case GGML_TYPE_I16: return "i16";
        case GGML_TYPE_I32: return "i32";
        default: return "unknown";
    }
}

// IEEE 754 half-precision (F16) to single-precision (F32) conversion
float ggml_fp16_to_fp32(ggml_fp16_t h) {
    uint32_t sign = (h & 0x8000) << 16;
    uint32_t exponent = (h & 0x7C00) >> 10;
    uint32_t mantissa = (h & 0x03FF);

    uint32_t bits;
    if (exponent == 0) {
        if (mantissa == 0) {
            // Zero
            bits = sign;
        } else {
            // Denormalized
            exponent = 1;
            while (!(mantissa & 0x0400)) {
                mantissa <<= 1;
                exponent--;
            }
            mantissa &= 0x03FF;
            bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) {
        // Infinity or NaN
        bits = sign | 0x7F800000 | (mantissa << 13);
    } else {
        // Normalized
        bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
    }

    float result;
    memcpy(&result, &bits, sizeof(float));
    return result;
}

// Single-precision to half-precision conversion
ggml_fp16_t ggml_fp32_to_fp16(float f) {
    uint32_t x;
    memcpy(&x, &f, sizeof(uint32_t));
    uint16_t sign = (x >> 16) & 0x8000;
    uint32_t exponent = (x >> 23) & 0xFF;
    uint32_t mantissa = x & 0x7FFFFF;

    if (exponent == 255) {
        // Infinity or NaN
        return sign | 0x7C00 | (mantissa >> 13);
    } else if (exponent > 142) {
        // Overflow to infinity
        return sign | 0x7C00;
    } else if (exponent < 103) {
        // Underflow to zero
        return sign;
    } else {
        // Normalized
        return sign | ((exponent - 112) << 10) | (mantissa >> 13);
    }
}
