#!/bin/bash
#
# model_download.sh - Download and convert speech recognition models
#
# This script downloads Distil-Whisper models which are released under
# the Apache 2.0 license, avoiding any licensing concerns with OpenAI's
# original Whisper models.
#
# Distil-Whisper: https://huggingface.co/distil-whisper
# License: Apache 2.0

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODELS_DIR="$SCRIPT_DIR/models"

mkdir -p "$MODELS_DIR"

echo "=============================================="
echo "  Distil-Whisper Model Downloader"
echo "=============================================="
echo ""
echo "Available models (Apache 2.0 licensed):"
echo "  1) distil-small.en  (~500MB)  - Fast, English only"
echo "  2) distil-medium.en (~1.5GB)  - Better accuracy"
echo "  3) distil-large-v2  (~3GB)    - Best accuracy, multilingual"
echo ""

# Check for required Python packages
check_requirements() {
    echo "Checking Python requirements..."
    if ! python3 -c "import torch" 2>/dev/null; then
        echo "Error: PyTorch not found. Install with: pip install torch"
        exit 1
    fi
    if ! python3 -c "import transformers" 2>/dev/null; then
        echo "Error: Transformers not found. Install with: pip install transformers"
        exit 1
    fi
    if ! python3 -c "import numpy" 2>/dev/null; then
        echo "Error: NumPy not found. Install with: pip install numpy"
        exit 1
    fi
    echo "All requirements satisfied."
    echo ""
}

# Download and convert model
download_and_convert() {
    local model_id=$1
    local output_name=$2
    local use_fp16=$3

    echo "Downloading and converting: $model_id"
    echo "Output: $MODELS_DIR/$output_name"
    echo ""

    if [ "$use_fp16" = "true" ]; then
        python3 "$SCRIPT_DIR/convert_to_ggml.py" "$model_id" "$MODELS_DIR/$output_name" --fp16
    else
        python3 "$SCRIPT_DIR/convert_to_ggml.py" "$model_id" "$MODELS_DIR/$output_name"
    fi
}

# Parse arguments
MODEL=${1:-small}
USE_FP16=${2:-false}

if [ "$2" = "--fp16" ] || [ "$2" = "fp16" ]; then
    USE_FP16="true"
fi

case $MODEL in
    small|1|distil-small.en)
        check_requirements
        download_and_convert "distil-whisper/distil-small.en" "distil-small.en.bin" "$USE_FP16"
        ;;
    medium|2|distil-medium.en)
        check_requirements
        download_and_convert "distil-whisper/distil-medium.en" "distil-medium.en.bin" "$USE_FP16"
        ;;
    large|3|distil-large-v2)
        check_requirements
        download_and_convert "distil-whisper/distil-large-v2" "distil-large-v2.bin" "$USE_FP16"
        ;;
    help|--help|-h)
        echo "Usage: $0 [model] [--fp16]"
        echo ""
        echo "Models:"
        echo "  small   - distil-whisper/distil-small.en (default)"
        echo "  medium  - distil-whisper/distil-medium.en"
        echo "  large   - distil-whisper/distil-large-v2"
        echo ""
        echo "Options:"
        echo "  --fp16  - Use float16 for smaller file size"
        echo ""
        echo "Examples:"
        echo "  $0                  # Download distil-small.en"
        echo "  $0 small --fp16    # Download distil-small.en with fp16"
        echo "  $0 medium          # Download distil-medium.en"
        exit 0
        ;;
    *)
        echo "Unknown model: $MODEL"
        echo "Usage: $0 [small|medium|large] [--fp16]"
        echo "Run '$0 --help' for more information."
        exit 1
        ;;
esac

echo ""
echo "=============================================="
echo "  Download Complete!"
echo "=============================================="
echo ""
echo "Model saved to: $MODELS_DIR/"
echo ""
echo "Usage:"
echo "  ./whisper $MODELS_DIR/*.bin audio.wav"
echo ""
echo "License: Apache 2.0 (Distil-Whisper)"
echo "Source: https://huggingface.co/distil-whisper"
echo ""
