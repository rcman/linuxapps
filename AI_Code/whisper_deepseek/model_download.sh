#!/bin/bash
# download.sh
mkdir -p models
cd models

# Download a small Whisper model (converted to GGML)
echo "Downloading tiny.en model..."
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin

# Or convert your own:
echo "To convert your own model:"
echo "1. Download from HuggingFace:"
echo "   git lfs install"
echo "   git clone https://huggingface.co/openai/whisper-tiny"
echo "2. Convert to GGML:"
echo "   python3 ../whisper.cpp/models/convert-pt-to-ggml.py ./whisper-tiny/ ./models/ 0"
