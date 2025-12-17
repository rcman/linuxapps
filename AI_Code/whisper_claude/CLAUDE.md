# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an **independent, from-scratch C implementation** of a speech-to-text system based on the Whisper architecture. It is designed to run speech recognition models in GGML format with minimal dependencies (only libc, libm, and libpthread).

## License

This project is licensed under the **MIT License**. See the `LICENSE` file for details.

**Important**: This code is an original implementation and is NOT derived from:
- OpenAI's Whisper (https://github.com/openai/whisper)
- ggerganov's whisper.cpp (https://github.com/ggerganov/whisper.cpp)
- ggerganov's ggml (https://github.com/ggerganov/ggml)

The implementation is based on the publicly documented Whisper architecture from the research paper:
> "Robust Speech Recognition via Large-Scale Weak Supervision" - Radford et al., 2022

## Model Recommendations

For commercial use or to avoid licensing concerns, we recommend using **Distil-Whisper** models:
- License: Apache 2.0 (permissive, commercial-friendly)
- Source: https://huggingface.co/distil-whisper
- Run `bash model_download.sh` to get started

## Build System

Build the project using make:

```bash
make              # Build the whisper executable
make clean        # Remove object files and executable
make test         # Run test transcription (requires model and test.wav)
```

Build configuration:
- Compiler: gcc
- Standard: C11
- Optimization: -O3
- Dependencies: -lm -lpthread

## Running the Program

Basic usage:
```bash
./whisper <model.bin> <audio.wav>
```

Example:
```bash
./whisper models/tiny.bin test.wav
```

The audio file must be 16-bit mono PCM WAV at 16kHz sample rate.

## Model Setup

Download a pre-converted GGML model:
```bash
bash model_download.sh
```

This downloads `ggml-tiny.en.bin` from HuggingFace to the `models/` directory.

## Architecture

### Core Components

The codebase consists of four main modules:

1. **ggml** (ggml.h/c) - Tensor computation library
   - Provides low-level tensor operations and graph computation
   - Supports multiple data types (F32, F16, quantized Q4_0/Q4_1)
   - Handles multi-threaded execution via computation plans
   - Tensors have shape (ne[]), strides (nb[]), and data pointers

2. **whisper** (whisper.h/c) - Whisper model implementation
   - Model loading from GGML binary format
   - Mel spectrogram generation from PCM audio
   - Encoder: Convolutional layers + Transformer blocks
   - Decoder: Token embeddings + autoregressive generation
   - Full pipeline: `whisper_full()` handles audio -> text conversion

3. **tokenizer** (tokenizer.h/c) - Text tokenization
   - Vocabulary management (51,865 tokens for Whisper)
   - Text encoding/decoding for model input/output
   - Language and task token handling

4. **main** (main.c) - CLI interface
   - Simple WAV file loader (assumes 44-byte PCM header)
   - Command-line argument parsing
   - Result display with timing information

### Data Flow

```
Audio WAV → load_wav() → PCM samples (float[])
           ↓
PCM samples → whisper_full() → Mel spectrogram
           ↓
Mel spectrogram → Encoder (conv + transformer)
           ↓
Audio features → Decoder (autoregressive)
           ↓
Token sequence → Tokenizer → Text output
```

### Model Structure (whisper_context)

The `whisper_context` struct in whisper.c contains:
- `model`: All model weights (encoder/decoder)
  - Convolutional layers (conv1, conv2)
  - Positional embeddings
  - Transformer blocks (attention + MLP layers)
  - Layer normalization parameters
- `tokenizer`: Vocabulary and token conversion
- `mel_filters`: Pre-computed mel-frequency filters
- `logits`, `tokens`: Inference state
- Timing fields: `t_mel_us`, `t_encode_us`, `t_decode_us`

### Key Constants (whisper.c)

- `WHISPER_SAMPLE_RATE`: 16000 Hz
- `WHISPER_N_FFT`: 400 (FFT size)
- `WHISPER_HOP_LENGTH`: 160 samples
- `WHISPER_N_MEL`: 80 mel bands
- `WHISPER_N_VOCAB`: 51865 tokens

### Memory Management

- GGML uses arena-style allocation via `ggml_init(mem_size)`
- Models loaded from file allocate tensors in GGML context
- Audio samples allocated per inference, freed after processing
- No garbage collection - manual `free()` required

## Whisper Parameters

The `whisper_full_params` struct controls inference:
- `strategy`: Decoding strategy (0 = greedy)
- `n_threads`: Thread count for parallel computation
- `language`: Language ID (0 = English)

## Development Notes

- Model weights are loaded directly from binary GGML format
- The mel spectrogram conversion happens in `whisper_pcm_to_mel()`
- Encoder processes audio in 30-second chunks
- Decoder runs autoregressively, generating one token at a time
- Timing is tracked with microsecond precision using system timers
