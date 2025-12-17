/*
 * ggml.h - Tensor computation library header
 *
 * Copyright (c) 2024 The Authors
 * SPDX-License-Identifier: MIT
 *
 * This is an independent implementation of a tensor computation library.
 * See ggml.c for full license text.
 */

#ifndef GGML_H
#define GGML_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GGML_MAX_DIMS 4
#define GGML_MAX_PARAMS 128

enum ggml_type {
    GGML_TYPE_F32  = 0,
    GGML_TYPE_F16  = 1,
    GGML_TYPE_Q4_0 = 2,
    GGML_TYPE_Q4_1 = 3,
    GGML_TYPE_I8   = 8,
    GGML_TYPE_I16  = 9,
    GGML_TYPE_I32  = 10,
};

struct ggml_tensor {
    enum ggml_type type;
    int32_t ne[GGML_MAX_DIMS]; // number of elements
    size_t  nb[GGML_MAX_DIMS]; // stride in bytes
    void* data;
    struct ggml_tensor* src[2];
    struct ggml_tensor* grad;
    char name[32];
};

struct ggml_context;
struct ggml_cgraph;

// Core functions
struct ggml_context* ggml_init(size_t mem_size);
void ggml_free(struct ggml_context* ctx);

struct ggml_tensor* ggml_new_tensor_1d(
    struct ggml_context* ctx,
    enum ggml_type type,
    int64_t ne0);

struct ggml_tensor* ggml_new_tensor_2d(
    struct ggml_context* ctx,
    enum ggml_type type,
    int64_t ne0,
    int64_t ne1);

struct ggml_tensor* ggml_new_tensor_3d(
    struct ggml_context* ctx,
    enum ggml_type type,
    int64_t ne0,
    int64_t ne1,
    int64_t ne2);

// Operations
struct ggml_tensor* ggml_add(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    struct ggml_tensor* b);

struct ggml_tensor* ggml_mul(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    struct ggml_tensor* b);

struct ggml_tensor* ggml_mul_mat(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    struct ggml_tensor* b);

struct ggml_tensor* ggml_scale(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    float s);

struct ggml_tensor* ggml_soft_max(
    struct ggml_context* ctx,
    struct ggml_tensor* a);

struct ggml_tensor* ggml_gelu(
    struct ggml_context* ctx,
    struct ggml_tensor* a);

struct ggml_tensor* ggml_norm(
    struct ggml_context* ctx,
    struct ggml_tensor* a);

struct ggml_tensor* ggml_view_1d(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    int64_t ne0,
    size_t offset);

struct ggml_tensor* ggml_view_2d(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    int64_t ne0,
    int64_t ne1,
    size_t nb1,
    size_t offset);

struct ggml_tensor* ggml_conv_1d(
    struct ggml_context* ctx,
    struct ggml_tensor* a,
    struct ggml_tensor* b,
    int s0,
    int p0,
    int d0);

// Graph building
struct ggml_cgraph* ggml_new_graph(struct ggml_context* ctx);
void ggml_build_forward_expand(struct ggml_cgraph* gf, struct ggml_tensor* tensor);

// Computation
struct ggml_cplan {
    int n_threads;
    size_t work_size;
    void* work_data;
};

struct ggml_cplan ggml_graph_plan(struct ggml_cgraph* cgraph, int n_threads);
void ggml_graph_compute(struct ggml_cgraph* cgraph, struct ggml_cplan* cplan);

// Utility
size_t ggml_type_size(enum ggml_type type);
const char* ggml_type_name(enum ggml_type type);

// F16 conversion
typedef uint16_t ggml_fp16_t;
float ggml_fp16_to_fp32(ggml_fp16_t h);
ggml_fp16_t ggml_fp32_to_fp16(float f);

#ifdef __cplusplus
}
#endif

#endif // GGML_H
