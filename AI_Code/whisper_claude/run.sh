#!/bin/bash
# Fully local Whisper transcription wrapper
# Usage: ./run.sh audio.wav

# Set library path to use local libraries
export LD_LIBRARY_PATH="$(dirname "$0")/lib:$LD_LIBRARY_PATH"

# Default model path
MODEL="${WHISPER_MODEL:-$HOME/models/ggml-base.en.bin}"

# Run whisper-full with local libraries
exec "$(dirname "$0")/whisper-full" -m "$MODEL" -f "$@"
