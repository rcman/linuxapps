// tokenizer.c
#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOCAB_SIZE 51865

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
        tokenizer->vocab.vocab[50256] = strdup("<|endoftext|>");
        tokenizer->vocab.vocab[50257] = strdup("<|startoftranscript|>");
        tokenizer->vocab.vocab[50258] = strdup("<|en|>");
        tokenizer->vocab.vocab[50259] = strdup("<|transcribe|>");
        
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
