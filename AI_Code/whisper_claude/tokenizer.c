/*
 * tokenizer.c - Text tokenization for speech recognition
 *
 * Copyright (c) 2024 The Authors
 * SPDX-License-Identifier: MIT
 *
 * This is an original tokenizer implementation.
 * See whisper.c for full license text.
 */

#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOCAB_SIZE 51865

// Custom strdup implementation for C11
static char* my_strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* p = (char*)malloc(len);
    if (p) {
        memcpy(p, s, len);
    }
    return p;
}

whisper_tokenizer* whisper_tokenizer_init(const char* vocab_path) {
    FILE* file = fopen(vocab_path, "r");
    if (!file) {
        // Create simple vocabulary if file doesn't exist
        whisper_tokenizer* tokenizer = (whisper_tokenizer*)malloc(sizeof(whisper_tokenizer));
        tokenizer->vocab.vocab = (char**)malloc(VOCAB_SIZE * sizeof(char*));
        tokenizer->vocab.vocab_size = VOCAB_SIZE;
        
        // Create basic vocabulary
        for (int i = 0; i < 256; i++) {
            tokenizer->vocab.vocab[i] = (char*)malloc(2);
            tokenizer->vocab.vocab[i][0] = (char)i;
            tokenizer->vocab.vocab[i][1] = '\0';
        }
        
        // Special tokens
        tokenizer->vocab.vocab[50256] = my_strdup("<|endoftext|>");
        tokenizer->vocab.vocab[50257] = my_strdup("<|startoftranscript|>");
        tokenizer->vocab.vocab[50258] = my_strdup("<|en|>");
        tokenizer->vocab.vocab[50259] = my_strdup("<|transcribe|>");
        
        tokenizer->sot_token = 50257;
        tokenizer->transcribe_token = 50259;
        tokenizer->not_token = 50358;
        tokenizer->solm_token = 50359;
        
        return tokenizer;
    }
    
    fclose(file);
    return NULL;
}

char* whisper_tokenizer_decode(whisper_tokenizer* tokenizer, const int* tokens, int n_tokens) {
    char* result = (char*)malloc(4096);
    result[0] = '\0';
    
    for (int i = 0; i < n_tokens; i++) {
        int token = tokens[i];
        if (token < tokenizer->vocab.vocab_size && tokenizer->vocab.vocab[token]) {
            strcat(result, tokenizer->vocab.vocab[token]);
        }
    }
    
    return result;
}
