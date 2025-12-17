#!/usr/bin/env python3
"""
convert_to_ggml.py - Convert Distil-Whisper models to GGML format

Copyright (c) 2024 The Authors
SPDX-License-Identifier: MIT

This script converts HuggingFace Distil-Whisper models to the GGML binary
format used by this speech recognition implementation.

Usage:
    python3 convert_to_ggml.py <model_path> <output_path> [--fp16]

Examples:
    python3 convert_to_ggml.py distil-whisper/distil-small.en models/distil-small.bin
    python3 convert_to_ggml.py ./local-model/ models/model.bin --fp16

Requirements:
    pip install torch transformers numpy
"""

import argparse
import struct
import sys
from pathlib import Path

import numpy as np

try:
    import torch
    from transformers import WhisperForConditionalGeneration, WhisperConfig
except ImportError:
    print("Error: Required packages not found.")
    print("Please install: pip install torch transformers numpy")
    sys.exit(1)


# GGML type codes (must match ggml.h)
GGML_TYPE_F32 = 0
GGML_TYPE_F16 = 1
GGML_TYPE_Q4_0 = 2
GGML_TYPE_Q4_1 = 3

# Magic number for GGML files
GGML_MAGIC = 0x67676D6C  # "ggml" in little-endian


def convert_to_fp16(tensor: np.ndarray) -> np.ndarray:
    """Convert tensor to float16."""
    return tensor.astype(np.float16)


def write_tensor(f, name: str, tensor: np.ndarray, use_fp16: bool = False):
    """Write a tensor in GGML format."""
    # Determine type and convert if needed
    if use_fp16 and tensor.dtype == np.float32:
        tensor = convert_to_fp16(tensor)
        ftype = GGML_TYPE_F16
    elif tensor.dtype == np.float16:
        ftype = GGML_TYPE_F16
    else:
        tensor = tensor.astype(np.float32)
        ftype = GGML_TYPE_F32

    # Get dimensions
    shape = tensor.shape
    n_dims = len(shape)

    # Encode name
    name_bytes = name.encode('utf-8')
    name_len = len(name_bytes)

    # Write header: n_dims, name_len, ftype
    f.write(struct.pack('<i', n_dims))
    f.write(struct.pack('<i', name_len))
    f.write(struct.pack('<i', ftype))

    # Write dimensions
    for dim in shape:
        f.write(struct.pack('<i', dim))

    # Write name
    f.write(name_bytes)

    # Write data
    tensor.tofile(f)

    print(f"  {name}: {shape} ({['f32', 'f16'][ftype]})")


def get_whisper_hparams(config: WhisperConfig) -> dict:
    """Extract hyperparameters from Whisper config."""
    return {
        'n_vocab': config.vocab_size,
        'n_audio_ctx': config.max_source_positions,
        'n_audio_state': config.d_model,
        'n_audio_head': config.encoder_attention_heads,
        'n_audio_layer': config.encoder_layers,
        'n_text_ctx': config.max_target_positions,
        'n_text_state': config.d_model,
        'n_text_head': config.decoder_attention_heads,
        'n_text_layer': config.decoder_layers,
        'n_mels': config.num_mel_bins,
        'ftype': GGML_TYPE_F32,
    }


def convert_model(model_path: str, output_path: str, use_fp16: bool = False):
    """Convert a HuggingFace Whisper model to GGML format."""
    print(f"Loading model from: {model_path}")

    # Load model
    model = WhisperForConditionalGeneration.from_pretrained(
        model_path,
        torch_dtype=torch.float32,
        low_cpu_mem_usage=True
    )
    config = model.config

    # Get hyperparameters
    hparams = get_whisper_hparams(config)

    print("\nModel configuration:")
    for k, v in hparams.items():
        print(f"  {k}: {v}")

    # Update ftype if using fp16
    if use_fp16:
        hparams['ftype'] = GGML_TYPE_F16

    # Open output file
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    print(f"\nWriting to: {output_path}")

    with open(output_path, 'wb') as f:
        # Write magic number
        f.write(struct.pack('<I', GGML_MAGIC))

        # Write hyperparameters
        f.write(struct.pack('<i', hparams['n_vocab']))
        f.write(struct.pack('<i', hparams['n_audio_ctx']))
        f.write(struct.pack('<i', hparams['n_audio_state']))
        f.write(struct.pack('<i', hparams['n_audio_head']))
        f.write(struct.pack('<i', hparams['n_audio_layer']))
        f.write(struct.pack('<i', hparams['n_text_ctx']))
        f.write(struct.pack('<i', hparams['n_text_state']))
        f.write(struct.pack('<i', hparams['n_text_head']))
        f.write(struct.pack('<i', hparams['n_text_layer']))
        f.write(struct.pack('<i', hparams['n_mels']))
        f.write(struct.pack('<i', hparams['ftype']))

        # Write mel filters (pre-computed, 80 x 201)
        print("\nWriting mel filters...")
        mel_filters = compute_mel_filters(
            n_mels=hparams['n_mels'],
            n_fft=400,
            sample_rate=16000
        )
        write_tensor(f, "mel_filters", mel_filters, use_fp16=False)

        # Get state dict
        state_dict = model.state_dict()

        # Write encoder weights
        print("\nWriting encoder weights...")
        write_encoder_weights(f, state_dict, hparams, use_fp16)

        # Write decoder weights
        print("\nWriting decoder weights...")
        write_decoder_weights(f, state_dict, hparams, use_fp16)

    # Print file size
    file_size = output_path.stat().st_size
    print(f"\nDone! Output file size: {file_size / 1024 / 1024:.2f} MB")


def compute_mel_filters(n_mels: int, n_fft: int, sample_rate: int) -> np.ndarray:
    """Compute mel filterbank (matching whisper.c implementation)."""
    n_freqs = n_fft // 2 + 1
    filters = np.zeros((n_mels, n_freqs), dtype=np.float32)

    # Mel scale conversion
    def hz_to_mel(hz):
        return 2595.0 * np.log10(1.0 + hz / 700.0)

    def mel_to_hz(mel):
        return 700.0 * (10.0 ** (mel / 2595.0) - 1.0)

    min_mel = hz_to_mel(0)
    max_mel = hz_to_mel(sample_rate / 2)

    mel_points = np.linspace(min_mel, max_mel, n_mels + 2)
    hz_points = mel_to_hz(mel_points)
    bin_points = np.floor((n_fft + 1) * hz_points / sample_rate).astype(int)

    for i in range(n_mels):
        left = bin_points[i]
        center = bin_points[i + 1]
        right = bin_points[i + 2]

        for j in range(left, center):
            if center != left:
                filters[i, j] = (j - left) / (center - left)

        for j in range(center, right):
            if right != center:
                filters[i, j] = (right - j) / (right - center)

    return filters


def write_encoder_weights(f, state_dict: dict, hparams: dict, use_fp16: bool):
    """Write encoder weights to file."""
    prefix = "model.encoder"

    # Convolutional layers
    write_tensor(f, "encoder.conv1.weight",
                 state_dict[f"{prefix}.conv1.weight"].numpy(), use_fp16)
    write_tensor(f, "encoder.conv1.bias",
                 state_dict[f"{prefix}.conv1.bias"].numpy(), use_fp16)
    write_tensor(f, "encoder.conv2.weight",
                 state_dict[f"{prefix}.conv2.weight"].numpy(), use_fp16)
    write_tensor(f, "encoder.conv2.bias",
                 state_dict[f"{prefix}.conv2.bias"].numpy(), use_fp16)

    # Positional embedding
    write_tensor(f, "encoder.positional_embedding",
                 state_dict[f"{prefix}.embed_positions.weight"].numpy(), use_fp16)

    # Layer norm
    write_tensor(f, "encoder.ln_post.weight",
                 state_dict[f"{prefix}.layer_norm.weight"].numpy(), use_fp16)
    write_tensor(f, "encoder.ln_post.bias",
                 state_dict[f"{prefix}.layer_norm.bias"].numpy(), use_fp16)

    # Transformer blocks
    n_layers = hparams['n_audio_layer']
    for i in range(n_layers):
        layer_prefix = f"{prefix}.layers.{i}"

        # Self-attention layer norm
        write_tensor(f, f"encoder.blocks.{i}.attn_ln.weight",
                     state_dict[f"{layer_prefix}.self_attn_layer_norm.weight"].numpy(), use_fp16)
        write_tensor(f, f"encoder.blocks.{i}.attn_ln.bias",
                     state_dict[f"{layer_prefix}.self_attn_layer_norm.bias"].numpy(), use_fp16)

        # Self-attention Q, K, V
        write_tensor(f, f"encoder.blocks.{i}.attn.query.weight",
                     state_dict[f"{layer_prefix}.self_attn.q_proj.weight"].numpy(), use_fp16)
        write_tensor(f, f"encoder.blocks.{i}.attn.query.bias",
                     state_dict[f"{layer_prefix}.self_attn.q_proj.bias"].numpy(), use_fp16)
        write_tensor(f, f"encoder.blocks.{i}.attn.key.weight",
                     state_dict[f"{layer_prefix}.self_attn.k_proj.weight"].numpy(), use_fp16)
        # Note: key has no bias in Whisper
        write_tensor(f, f"encoder.blocks.{i}.attn.value.weight",
                     state_dict[f"{layer_prefix}.self_attn.v_proj.weight"].numpy(), use_fp16)
        write_tensor(f, f"encoder.blocks.{i}.attn.value.bias",
                     state_dict[f"{layer_prefix}.self_attn.v_proj.bias"].numpy(), use_fp16)

        # Self-attention output
        write_tensor(f, f"encoder.blocks.{i}.attn.out.weight",
                     state_dict[f"{layer_prefix}.self_attn.out_proj.weight"].numpy(), use_fp16)
        write_tensor(f, f"encoder.blocks.{i}.attn.out.bias",
                     state_dict[f"{layer_prefix}.self_attn.out_proj.bias"].numpy(), use_fp16)

        # MLP layer norm
        write_tensor(f, f"encoder.blocks.{i}.mlp_ln.weight",
                     state_dict[f"{layer_prefix}.final_layer_norm.weight"].numpy(), use_fp16)
        write_tensor(f, f"encoder.blocks.{i}.mlp_ln.bias",
                     state_dict[f"{layer_prefix}.final_layer_norm.bias"].numpy(), use_fp16)

        # MLP
        write_tensor(f, f"encoder.blocks.{i}.mlp.0.weight",
                     state_dict[f"{layer_prefix}.fc1.weight"].numpy(), use_fp16)
        write_tensor(f, f"encoder.blocks.{i}.mlp.0.bias",
                     state_dict[f"{layer_prefix}.fc1.bias"].numpy(), use_fp16)
        write_tensor(f, f"encoder.blocks.{i}.mlp.2.weight",
                     state_dict[f"{layer_prefix}.fc2.weight"].numpy(), use_fp16)
        write_tensor(f, f"encoder.blocks.{i}.mlp.2.bias",
                     state_dict[f"{layer_prefix}.fc2.bias"].numpy(), use_fp16)


def write_decoder_weights(f, state_dict: dict, hparams: dict, use_fp16: bool):
    """Write decoder weights to file."""
    prefix = "model.decoder"

    # Token embedding
    write_tensor(f, "decoder.token_embedding.weight",
                 state_dict[f"{prefix}.embed_tokens.weight"].numpy(), use_fp16)

    # Positional embedding
    write_tensor(f, "decoder.positional_embedding",
                 state_dict[f"{prefix}.embed_positions.weight"].numpy(), use_fp16)

    # Layer norm
    write_tensor(f, "decoder.ln.weight",
                 state_dict[f"{prefix}.layer_norm.weight"].numpy(), use_fp16)
    write_tensor(f, "decoder.ln.bias",
                 state_dict[f"{prefix}.layer_norm.bias"].numpy(), use_fp16)

    # Transformer blocks
    n_layers = hparams['n_text_layer']
    for i in range(n_layers):
        layer_prefix = f"{prefix}.layers.{i}"

        # Self-attention layer norm
        write_tensor(f, f"decoder.blocks.{i}.attn_ln.weight",
                     state_dict[f"{layer_prefix}.self_attn_layer_norm.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.attn_ln.bias",
                     state_dict[f"{layer_prefix}.self_attn_layer_norm.bias"].numpy(), use_fp16)

        # Self-attention Q, K, V
        write_tensor(f, f"decoder.blocks.{i}.attn.query.weight",
                     state_dict[f"{layer_prefix}.self_attn.q_proj.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.attn.query.bias",
                     state_dict[f"{layer_prefix}.self_attn.q_proj.bias"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.attn.key.weight",
                     state_dict[f"{layer_prefix}.self_attn.k_proj.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.attn.value.weight",
                     state_dict[f"{layer_prefix}.self_attn.v_proj.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.attn.value.bias",
                     state_dict[f"{layer_prefix}.self_attn.v_proj.bias"].numpy(), use_fp16)

        # Self-attention output
        write_tensor(f, f"decoder.blocks.{i}.attn.out.weight",
                     state_dict[f"{layer_prefix}.self_attn.out_proj.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.attn.out.bias",
                     state_dict[f"{layer_prefix}.self_attn.out_proj.bias"].numpy(), use_fp16)

        # Cross-attention layer norm
        write_tensor(f, f"decoder.blocks.{i}.cross_attn_ln.weight",
                     state_dict[f"{layer_prefix}.encoder_attn_layer_norm.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.cross_attn_ln.bias",
                     state_dict[f"{layer_prefix}.encoder_attn_layer_norm.bias"].numpy(), use_fp16)

        # Cross-attention Q, K, V
        write_tensor(f, f"decoder.blocks.{i}.cross_attn.query.weight",
                     state_dict[f"{layer_prefix}.encoder_attn.q_proj.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.cross_attn.query.bias",
                     state_dict[f"{layer_prefix}.encoder_attn.q_proj.bias"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.cross_attn.key.weight",
                     state_dict[f"{layer_prefix}.encoder_attn.k_proj.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.cross_attn.value.weight",
                     state_dict[f"{layer_prefix}.encoder_attn.v_proj.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.cross_attn.value.bias",
                     state_dict[f"{layer_prefix}.encoder_attn.v_proj.bias"].numpy(), use_fp16)

        # Cross-attention output
        write_tensor(f, f"decoder.blocks.{i}.cross_attn.out.weight",
                     state_dict[f"{layer_prefix}.encoder_attn.out_proj.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.cross_attn.out.bias",
                     state_dict[f"{layer_prefix}.encoder_attn.out_proj.bias"].numpy(), use_fp16)

        # MLP layer norm
        write_tensor(f, f"decoder.blocks.{i}.mlp_ln.weight",
                     state_dict[f"{layer_prefix}.final_layer_norm.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.mlp_ln.bias",
                     state_dict[f"{layer_prefix}.final_layer_norm.bias"].numpy(), use_fp16)

        # MLP
        write_tensor(f, f"decoder.blocks.{i}.mlp.0.weight",
                     state_dict[f"{layer_prefix}.fc1.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.mlp.0.bias",
                     state_dict[f"{layer_prefix}.fc1.bias"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.mlp.2.weight",
                     state_dict[f"{layer_prefix}.fc2.weight"].numpy(), use_fp16)
        write_tensor(f, f"decoder.blocks.{i}.mlp.2.bias",
                     state_dict[f"{layer_prefix}.fc2.bias"].numpy(), use_fp16)

    # Output projection (tied to token embedding in original, but we write separately)
    if "proj_out.weight" in state_dict:
        write_tensor(f, "decoder.proj.weight",
                     state_dict["proj_out.weight"].numpy(), use_fp16)
    else:
        # Use token embedding as projection (weight tying)
        write_tensor(f, "decoder.proj.weight",
                     state_dict[f"{prefix}.embed_tokens.weight"].numpy(), use_fp16)


def main():
    parser = argparse.ArgumentParser(
        description="Convert Distil-Whisper models to GGML format",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert from HuggingFace Hub
  python3 convert_to_ggml.py distil-whisper/distil-small.en models/distil-small.bin

  # Convert local model with fp16
  python3 convert_to_ggml.py ./my-model/ models/model.bin --fp16

Supported models:
  - distil-whisper/distil-small.en (Apache 2.0)
  - distil-whisper/distil-medium.en (Apache 2.0)
  - distil-whisper/distil-large-v2 (Apache 2.0)
  - openai/whisper-tiny.en (check license)
  - openai/whisper-base.en (check license)
        """
    )

    parser.add_argument(
        "model_path",
        help="Path to HuggingFace model (local path or hub ID)"
    )
    parser.add_argument(
        "output_path",
        help="Output path for GGML file"
    )
    parser.add_argument(
        "--fp16",
        action="store_true",
        help="Convert weights to float16 (smaller file, slightly less accurate)"
    )

    args = parser.parse_args()

    convert_model(args.model_path, args.output_path, args.fp16)


if __name__ == "__main__":
    main()
