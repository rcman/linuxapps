// ggml.c
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

// ... (implement other operations similarly)

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
