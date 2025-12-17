/*
 * main.c - Command-line interface for speech recognition
 *
 * Copyright (c) 2024 The Authors
 * SPDX-License-Identifier: MIT
 *
 * See whisper.c for full license text.
 */

#include "whisper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple WAV loader (16-bit mono, 16kHz)
float* load_wav(const char* filename, int* n_samples) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    
    // Skip WAV header (44 bytes for PCM)
    fseek(f, 44, SEEK_SET);
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f) - 44;
    fseek(f, 44, SEEK_SET);
    
    *n_samples = size / 2; // 16-bit samples
    
    int16_t* raw = (int16_t*)malloc(size);
    fread(raw, 1, size, f);
    
    float* samples = (float*)malloc(*n_samples * sizeof(float));
    for (int i = 0; i < *n_samples; i++) {
        samples[i] = raw[i] / 32768.0f;
    }
    
    free(raw);
    fclose(f);
    return samples;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <model.bin> <audio.wav>\n", argv[0]);
        return 1;
    }
    
    printf("Loading model from %s\n", argv[1]);
    struct whisper_context* ctx = whisper_init_from_file(argv[1]);
    
    printf("Loading audio from %s\n", argv[2]);
    int n_samples;
    float* samples = load_wav(argv[2], &n_samples);
    
    printf("Audio samples: %d (%.1f seconds)\n", n_samples, n_samples / 16000.0f);
    
    // Simple params
    struct whisper_full_params params = {
        .strategy = 0,
        .n_threads = 4,
        .language = 0, // English
    };
    
    printf("Transcribing...\n");
    whisper_full(ctx, params, samples, n_samples);
    
    const char* text = whisper_full_get_segment_text(ctx, 0);
    if (text) {
        printf("\n=== TRANSCRIPTION ===\n");
        printf("%s\n", text);
        printf("=====================\n");
        
        printf("\nTimings:\n");
        printf("  Mel spectrogram: %ld us\n", (long)whisper_get_time_mel(ctx));
        printf("  Encode:          %ld us\n", (long)whisper_get_time_encode(ctx));
        printf("  Decode:          %ld us\n", (long)whisper_get_time_decode(ctx));
    }
    
    free(samples);
    whisper_free(ctx);
    
    return 0;
}
