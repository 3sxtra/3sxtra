#!/usr/bin/env python3
"""
Batch upscale all sprite PNGs in output/sprites using ComfyUI API.

Uses the 4x-UltraSharpV2.safetensors upscale model.
Processes each character folder separately via the WAS Node Suite
"Load Image Batch" / "Image Save" nodes.

Alpha channel is preserved: the workflow splits alpha, upscales RGB and mask
separately, then recombines.

Usage:
  1. Start ComfyUI:  cd D:\\ComfyUI_windows_portable && run_nvidia_gpu.bat
  2. Run this script: python comfyui_batch_upscale.py [--server 127.0.0.1:8188]
"""

import argparse
import os
import sys
import urllib.request
import urllib.error
import uuid

from comfyui_api import build_upscale_workflow, queue_prompt, wait_for_completion

# ── Configuration ──────────────────────────────────────────────────────────────

SPRITES_ROOT   = os.environ.get("SF33RD_SPRITES_ROOT", r"D:\3sxtra\output\sprites")
SPRITES_OUTPUT = os.environ.get("SF33RD_SPRITES_4X", r"D:\3sxtra\output\sprites_4x")
UPSCALE_MODEL  = "4x-UltraSharpV2.safetensors"
DEFAULT_SERVER = "127.0.0.1:8188"

# ── Prompt builder ─────────────────────────────────────────────────────────────


def build_prompt(input_dir: str, output_dir: str, img_idx: int = 0) -> dict:
    """Build a ComfyUI prompt for batch sprite upscale."""
    input_path = input_dir.replace("\\", "/")
    output_path = output_dir.replace("\\", "/")

    loader_node = {
        "class_type": "Load Image Batch",
        "inputs": {
            "mode": "incremental_image",
            "seed": 0,
            "index": img_idx,
            "label": "sprite_",
            "path": input_path,
            "pattern": "*",
            "allow_RGBA_output": "true",
            "filename_text_extension": "false"
        }
    }

    save_node = {
        "class_type": "Image Save",
        "inputs": {
            "images": ["34", 0],
            "output_path": output_path,
            "filename_prefix": ["26", 1],   # original filename from loader
            "filename_delimiter": "_",
            "filename_number_padding": 1,
            "filename_number_start": "false",
            "extension": "png",
            "dpi": 300,
            "quality": 100,
            "optimize_image": "true",
            "lossless_webp": "false",
            "overwrite_mode": "prefix_as_filename",
            "show_history": "false",
            "show_history_by_prefix": "true",
            "embed_workflow": "false",
            "show_previews": "true"
        }
    }

    return build_upscale_workflow(UPSCALE_MODEL, loader_node, save_node)


# ── Helpers ───────────────────────────────────────────────────────────────────


def count_pngs(directory: str) -> int:
    """Count PNG files in a directory."""
    count = 0
    for f in os.listdir(directory):
        if f.lower().endswith(".png"):
            count += 1
    return count


# ── Main ───────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Batch upscale sprites via ComfyUI")
    parser.add_argument("--server", default=DEFAULT_SERVER, help="ComfyUI server address (default: 127.0.0.1:8188)")
    parser.add_argument("--folder", default=None, help="Process a single character folder instead of all")
    parser.add_argument("--dry-run", action="store_true", help="Print what would be processed without running")
    args = parser.parse_args()

    # Verify ComfyUI is running
    try:
        urllib.request.urlopen(f"http://{args.server}/system_stats", timeout=5)
        print(f"✅ Connected to ComfyUI at {args.server}")
    except Exception as e:
        print(f"❌ Cannot connect to ComfyUI at {args.server}")
        print(f"   Start ComfyUI first: cd D:\\ComfyUI_windows_portable && run_nvidia_gpu.bat")
        print(f"   Error: {e}")
        sys.exit(1)

    # Gather character folders
    if args.folder:
        folders = [args.folder]
    else:
        folders = sorted([
            d for d in os.listdir(SPRITES_ROOT)
            if os.path.isdir(os.path.join(SPRITES_ROOT, d))
        ])

    if not folders:
        print("❌ No character folders found in", SPRITES_ROOT)
        sys.exit(1)

    client_id = str(uuid.uuid4())
    total_images = 0
    total_folders = len(folders)

    os.makedirs(SPRITES_OUTPUT, exist_ok=True)

    print(f"\n🎮 Batch Upscale: {total_folders} character folders")
    print(f"📁 Source: {SPRITES_ROOT}")
    print(f"📂 Output: {SPRITES_OUTPUT} (all files together)")
    print(f"🔧 Model:  {UPSCALE_MODEL}")
    print(f"{'─' * 60}")

    for idx, folder_name in enumerate(folders, 1):
        input_dir  = os.path.join(SPRITES_ROOT, folder_name)
        png_count = count_pngs(input_dir)

        if png_count == 0:
            print(f"\n[{idx}/{total_folders}] {folder_name}: no PNGs, skipping.")
            continue

        total_images += png_count
        label = f"[{idx}/{total_folders}] {folder_name} ({png_count} images)"

        if args.dry_run:
            print(f"  🔍 {label}")
            continue

        print(f"\n{label}")

        # Submit one prompt per image (incremental mode)
        for img_idx in range(png_count):
            prompt = build_prompt(input_dir, SPRITES_OUTPUT, img_idx)

            try:
                prompt_id = queue_prompt(args.server, prompt, client_id)
                img_label = f"{folder_name} image {img_idx + 1}/{png_count}"
                wait_for_completion(args.server, prompt_id, img_label)
            except urllib.error.HTTPError as e:
                error_body = e.read().decode("utf-8", errors="replace")
                print(f"\n  ❌ API error for {folder_name} image {img_idx + 1}: {e.code}")
                print(f"     {error_body[:500]}")
                # Continue with next image
            except Exception as e:
                print(f"\n  ❌ Error for {folder_name} image {img_idx + 1}: {e}")

    print(f"\n{'─' * 60}")
    if args.dry_run:
        print(f"🔍 Dry run complete: {total_images} images across {total_folders} folders.")
    else:
        print(f"🎉 Done! Processed {total_images} images across {total_folders} folders.")
        print(f"   Output: {SPRITES_OUTPUT}")


if __name__ == "__main__":
    main()
