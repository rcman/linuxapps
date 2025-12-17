/*
 * tokenizer.h - Text tokenization header
 *
 * Copyright (c) 2024 The Authors
 * SPDX-License-Identifier: MIT
 *
 * See whisper.c for full license text.
 */

#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdint.h>

typedef struct {
    char** vocab;
    int vocab_size;
    float* token_scores;
    unsigned int max_token_length;
} whisper_vocab;

typedef struct {
    whisper_vocab vocab;
    int language;
    int task;
    int sot_token;
    int transcribe_token;
    int not_token;
    int solm_token;
    int prev_token;
    int* tokens;
    int n_tokens;
} whisper_tokenizer;

whisper_tokenizer* whisper_tokenizer_init(const char* vocab_path);
void whisper_tokenizer_free(whisper_tokenizer* tokenizer);
int whisper_tokenizer_encode(whisper_tokenizer* tokenizer, const char* text, int* tokens, int max_tokens);
char* whisper_tokenizer_decode(whisper_tokenizer* tokenizer, const int* tokens, int n_tokens);

#endif
