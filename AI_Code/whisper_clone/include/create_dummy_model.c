// Create a dummy Whisper model file for testing
#include <stdio.h>
#include <stdlib.h>

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
} WhisperConfig;

int main() {
    FILE* f = fopen("dummy_whisper_model.bin", "wb");
    if (!f) {
        fprintf(stderr, "Failed to create file\n");
        return 1;
    }

    // Whisper tiny model dimensions
    WhisperConfig config = {
        .n_vocab = 51864,
        .n_audio_ctx = 1500,
        .n_audio_state = 384,
        .n_audio_head = 6,
        .n_audio_layer = 4,
        .n_text_ctx = 448,
        .n_text_state = 384,
        .n_text_head = 6,
        .n_text_layer = 4,
        .n_mels = 80,
        .ftype = 0
    };

    // Write config
    fwrite(&config, sizeof(WhisperConfig), 1, f);

    // Write dummy weights (zeros)
    // In a real model, these would be the actual trained weights
    int total_weights = 10000;  // simplified
    float* weights = calloc(total_weights, sizeof(float));
    fwrite(weights, sizeof(float), total_weights, f);

    free(weights);
    fclose(f);

    printf("Created dummy_whisper_model.bin with config:\n");
    printf("  n_vocab: %d\n", config.n_vocab);
    printf("  n_audio_state: %d\n", config.n_audio_state);
    printf("  n_audio_layer: %d\n", config.n_audio_layer);
    printf("  n_text_state: %d\n", config.n_text_state);
    printf("  n_mels: %d\n", config.n_mels);

    return 0;
}
