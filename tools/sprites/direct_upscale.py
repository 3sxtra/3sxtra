#!/usr/bin/env python3
"""
Direct GPU upscale for sprite PNGs — no ComfyUI required.

Loads 4x-UltraSharpV2.safetensors via spandrel and runs inference
directly on the GPU. Alpha channel is preserved by upscaling RGB
and alpha mask separately, then recombining.

Performance features:
  - fp16 inference on RTX GPUs
  - torch.compile() JIT (Triton backend)
  - Threaded I/O: next image loads while current one processes
  - Tile-based processing for large sprites (avoids OOM)

Usage:
  python direct_upscale.py                          # defaults
  python direct_upscale.py --input path --output path
  python direct_upscale.py --tile-size 512 --fp32   # safer for edge cases
"""

import argparse
import logging
import os
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import torch
from PIL import Image

# Enable TF32 for ~2x fp32 matmul speedup on Ampere+ GPUs (RTX 30xx/40xx/50xx)
torch.set_float32_matmul_precision("high")

# Suppress noisy autotune benchmarking logs from torch.compile
logging.getLogger("torch._inductor.autotune_process").setLevel(logging.WARNING)
logging.getLogger("torch._inductor.select_algorithm").setLevel(logging.WARNING)

# ── Defaults ──────────────────────────────────────────────────────────────────

DEFAULT_MODEL = r"D:\ComfyUI_windows_portable\ComfyUI\models\upscale_models\4x-UltraSharpV2.safetensors"
DEFAULT_INPUT = r"D:\3sxtra\output\missing_hd_sprites"
DEFAULT_OUTPUT = r"D:\3sxtra\output\missing_hd_sprites_4x"
DEFAULT_TILE = 0  # 0 = no tiling (whole image)


# ── Image I/O (runs in threads) ──────────────────────────────────────────────

def load_image(path: str) -> np.ndarray:
    """Load PNG as float32 RGBA numpy array (H, W, 4)."""
    img = Image.open(path).convert("RGBA")
    return np.array(img, dtype=np.float32) / 255.0


def save_image(path: str, arr: np.ndarray):
    """Save float32 RGBA numpy array (H, W, 4) as PNG."""
    arr = np.nan_to_num(arr, nan=0.0)
    arr = np.clip(arr * 255.0, 0, 255).astype(np.uint8)
    Image.fromarray(arr, "RGBA").save(path, optimize=True)


# ── Tiled upscale ────────────────────────────────────────────────────────────

def upscale_tensor(model, img_tensor: torch.Tensor, tile_size: int, scale: int, device, dtype) -> torch.Tensor:
    """
    Upscale a (1, C, H, W) tensor, optionally tiling to avoid OOM.
    """
    if tile_size <= 0:
        # Whole-image mode
        with torch.no_grad():
            torch.compiler.cudagraph_mark_step_begin()
            return model(img_tensor.to(device=device, dtype=dtype)).float().clone()

    # Tiled mode with overlap to avoid seams
    overlap = 16
    _, c, h, w = img_tensor.shape
    out_h, out_w = h * scale, w * scale
    output = torch.zeros(1, c, out_h, out_w, device="cpu", dtype=torch.float32)
    weight = torch.zeros(1, 1, out_h, out_w, device="cpu", dtype=torch.float32)

    for y in range(0, h, tile_size - overlap):
        for x in range(0, w, tile_size - overlap):
            y2 = min(y + tile_size, h)
            x2 = min(x + tile_size, w)
            y1 = max(0, y2 - tile_size)
            x1 = max(0, x2 - tile_size)

            tile_in = img_tensor[:, :, y1:y2, x1:x2].to(device=device, dtype=dtype)
            with torch.no_grad():
                torch.compiler.cudagraph_mark_step_begin()
                tile_out = model(tile_in).float().clone().cpu()

            oy1, ox1 = y1 * scale, x1 * scale
            oy2, ox2 = oy1 + tile_out.shape[2], ox1 + tile_out.shape[3]
            output[:, :, oy1:oy2, ox1:ox2] += tile_out
            weight[:, :, oy1:oy2, ox1:ox2] += 1.0

    return output / weight.clamp(min=1.0)


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Direct GPU sprite upscale (no ComfyUI)")
    parser.add_argument("--model", default=DEFAULT_MODEL, help="Path to .safetensors upscale model")
    parser.add_argument("--input", default=DEFAULT_INPUT, help="Input directory of PNGs")
    parser.add_argument("--output", default=DEFAULT_OUTPUT, help="Output directory for upscaled PNGs")
    parser.add_argument("--tile-size", type=int, default=DEFAULT_TILE, help="Tile size (0 = whole image, use 512 for large sprites)")
    parser.add_argument("--fp32", action="store_true", help="Use fp32 instead of fp16")
    parser.add_argument("--compile", action="store_true", help="Enable torch.compile() (only helps if all images are the same size)")
    parser.add_argument("--skip-existing", action="store_true", help="Skip files that already exist in output")
    args = parser.parse_args()

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    dtype = torch.float32 if args.fp32 else torch.float16
    print(f"🖥️  Device: {device} ({torch.cuda.get_device_name(0) if device.type == 'cuda' else 'CPU'})")
    print(f"⚡ Precision: {'fp32' if args.fp32 else 'fp16'}")

    # ── Load model via spandrel ──
    import spandrel
    print(f"📦 Loading model: {os.path.basename(args.model)}")
    t0 = time.perf_counter()
    model_desc = spandrel.ModelLoader(device=device).load_from_file(args.model)
    model = model_desc.model.eval().to(dtype=dtype)
    scale = model_desc.scale
    print(f"   Scale: {scale}x  Loaded in {time.perf_counter() - t0:.1f}s")

    # ── torch.compile (opt-in, only useful when all images share dimensions) ──
    if args.compile:
        print("🔧 Compiling model (first image will be slower)...")
        try:
            model = torch.compile(model, mode="max-autotune")
        except Exception as e:
            print(f"   ⚠️  torch.compile() failed ({e}), continuing without it")

    # ── Gather input files ──
    input_dir = Path(args.input)
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    files = sorted([f for f in input_dir.iterdir() if f.suffix.lower() == ".png"])
    if args.skip_existing:
        before = len(files)
        files = [f for f in files if not (output_dir / f.name).exists()]
        print(f"📂 Skipping {before - len(files)} existing, {len(files)} remaining")

    if not files:
        print("✅ Nothing to process.")
        return

    print(f"📂 Input:  {input_dir}  ({len(files)} PNGs)")
    print(f"📂 Output: {output_dir}")
    print(f"{'─' * 60}")

    # ── Process with threaded I/O ──
    executor = ThreadPoolExecutor(max_workers=2)
    total = len(files)
    start_time = time.perf_counter()

    # Pre-load first image
    future_load = executor.submit(load_image, str(files[0]))

    for i, fpath in enumerate(files):
        # Wait for current image to load
        img_np = future_load.result()

        # Start loading next image in background
        if i + 1 < total:
            future_load = executor.submit(load_image, str(files[i + 1]))

        h, w = img_np.shape[:2]
        has_alpha = img_np.shape[2] == 4

        # Split RGB and alpha
        rgb = torch.from_numpy(img_np[:, :, :3]).permute(2, 0, 1).unsqueeze(0)  # (1, 3, H, W)
        if has_alpha:
            alpha = torch.from_numpy(img_np[:, :, 3:4]).permute(2, 0, 1).unsqueeze(0)  # (1, 1, H, W)

        # Upscale RGB
        rgb_up = upscale_tensor(model, rgb, args.tile_size, scale, device, dtype)

        # Upscale alpha (as 3-channel, take mean)
        if has_alpha:
            alpha_3ch = alpha.expand(-1, 3, -1, -1)
            alpha_up = upscale_tensor(model, alpha_3ch, args.tile_size, scale, device, dtype)
            alpha_up = alpha_up.mean(dim=1, keepdim=True)
            # Combine
            result = torch.cat([rgb_up.cpu(), alpha_up.cpu()], dim=1)
        else:
            result = rgb_up.cpu()

        result = result.squeeze(0).permute(1, 2, 0).clamp(0, 1).numpy()

        # Save in background
        out_path = str(output_dir / fpath.name)
        executor.submit(save_image, out_path, result)

        elapsed = time.perf_counter() - start_time
        rate = (i + 1) / elapsed
        eta = (total - i - 1) / rate if rate > 0 else 0
        print(f"  [{i+1}/{total}] {fpath.name}  {w}x{h} → {w*scale}x{h*scale}  "
              f"({rate:.1f} img/s, ETA {eta:.0f}s)", end="\r")

    executor.shutdown(wait=True)
    total_time = time.perf_counter() - start_time
    print(f"\n{'─' * 60}")
    print(f"🎉 Done! {total} images in {total_time:.1f}s ({total/total_time:.1f} img/s)")
    print(f"   Output: {output_dir}")


if __name__ == "__main__":
    main()
