/*
 * whisper.h - Speech-to-text model header
 *
 * Copyright (c) 2024 The Authors
 * SPDX-License-Identifier: MIT
 *
 * See whisper.c for full license text.
 */

#ifndef WHISPER_H
#define WHISPER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Model types
enum whisper_model_type {
    WHISPER_MODEL_TINY,
    WHISPER_MODEL_BASE,
    WHISPER_MODEL_SMALL,
    WHISPER_MODEL_MEDIUM,
    WHISPER_MODEL_LARGE,
};

// Forward declarations
struct whisper_context;
struct whisper_state;

typedef struct whisper_token_data {
    int id;
    float p;
    float plog;
    const char* text;
} whisper_token_data;

typedef struct whisper_segment {
    int64_t t0;
    int64_t t1;
    const char* text;
    float confidence;
} whisper_segment;

typedef struct whisper_full_params {
    int strategy;
    int n_threads;
    int language;
    int n_max_text_ctx;
    bool translate;
    bool no_context;
    bool single_segment;
    bool print_special;
    bool print_progress;
    bool print_realtime;
    bool print_timestamps;
} whisper_full_params;

// API
struct whisper_context* whisper_init_from_file(const char* path_model);
struct whisper_context* whisper_init_from_buffer(void* buffer, size_t buffer_size);
struct whisper_context* whisper_init(struct whisper_context* ctx);
void whisper_free(struct whisper_context* ctx);

int whisper_pcm_to_mel(
    struct whisper_context* ctx,
    const float* samples,
    int n_samples,
    int n_threads);

int whisper_pcm_to_mel_with_state(
    struct whisper_context* ctx,
    struct whisper_state* state,
    const float* samples,
    int n_samples,
    int n_threads);

int whisper_encode(
    struct whisper_context* ctx,
    int offset,
    int n_threads);

int whisper_encode_with_state(
    struct whisper_context* ctx,
    struct whisper_state* state,
    int offset,
    int n_threads);

int whisper_decode(
    struct whisper_context* ctx,
    const whisper_token_data* tokens,
    int n_tokens,
    int n_past,
    int n_threads);

int whisper_decode_with_state(
    struct whisper_context* ctx,
    struct whisper_state* state,
    const whisper_token_data* tokens,
    int n_tokens,
    int n_past,
    int n_threads);

int whisper_tokenize(
    struct whisper_context* ctx,
    const char* text,
    whisper_token_data* tokens,
    int n_max_tokens);

int whisper_lang_id(const char* lang);

int whisper_n_len(struct whisper_context* ctx);
int whisper_n_vocab(struct whisper_context* ctx);
int whisper_n_text_ctx(struct whisper_context* ctx);
int whisper_n_audio_ctx(struct whisper_context* ctx);
int whisper_is_multilingual(struct whisper_context* ctx);

float* whisper_get_logits(struct whisper_context* ctx);
float* whisper_get_logits_from_state(struct whisper_state* state);

const char* whisper_token_to_str(struct whisper_context* ctx, int token);
whisper_token_data whisper_sample_best(struct whisper_context* ctx);
whisper_token_data whisper_sample_timestamp(struct whisper_context* ctx, bool initial);

// Streaming transcription
int whisper_full(
    struct whisper_context* ctx,
    struct whisper_full_params params,
    const float* samples,
    int n_samples);

int whisper_full_with_state(
    struct whisper_context* ctx,
    struct whisper_state* state,
    struct whisper_full_params params,
    const float* samples,
    int n_samples);

// Results
int whisper_full_n_segments(struct whisper_context* ctx);
int whisper_full_n_segments_from_state(struct whisper_state* state);
const char* whisper_full_get_segment_text(struct whisper_context* ctx, int i_segment);
int64_t whisper_full_get_segment_t0(struct whisper_context* ctx, int i_segment);
int64_t whisper_full_get_segment_t1(struct whisper_context* ctx, int i_segment);

// Print info
void whisper_print_timings(struct whisper_context* ctx);
void whisper_reset_timings(struct whisper_context* ctx);

// Timing accessors
int64_t whisper_get_time_mel(struct whisper_context* ctx);
int64_t whisper_get_time_encode(struct whisper_context* ctx);
int64_t whisper_get_time_decode(struct whisper_context* ctx);

// System info
const char* whisper_print_system_info(void);

#ifdef __cplusplus
}
#endif

#endif // WHISPER_H
