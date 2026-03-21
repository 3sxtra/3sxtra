#!/usr/bin/env python3
"""
Batch upscale all stage layer PNGs via ComfyUI API, then retile into runtime tiles.

Finds all layer_*.png files in the indexed stage extraction output,
upscales them via ComfyUI (4x-UltraSharpV2), then runs retile_stage.py
to split back into bg_{composite_key}.png tiles for the runtime.

Alpha channel is preserved: the workflow splits alpha, upscales RGB and mask
separately, then recombines.

Usage:
  1. Extract stages first:   python tools/extract_stage.py all --mode=indexed
  2. Start ComfyUI:          cd D:\\ComfyUI_windows_portable && run_nvidia_gpu.bat
  3. Run this script:        python comfyui_batch_upscale_stages.py
  4. Tiles output to:        assets/sprites/

Options:
  --server 127.0.0.1:8188   ComfyUI server address
  --stage 0                 Process a single stage
  --dry-run                 Preview without processing
  --skip-retile             Only upscale, don't retile
"""

import argparse
import glob
import json
import os
import subprocess
import sys
import urllib.request
import urllib.error
import uuid

from comfyui_api import build_upscale_workflow, queue_prompt, wait_for_completion

# ── Configuration ──────────────────────────────────────────────────────────────

STAGES_ROOT    = os.environ.get("SF33RD_STAGES_ROOT", r"D:\3sxtra\output\stages")
TILES_OUTPUT   = os.environ.get("SF33RD_TILES_OUTPUT", r"D:\3sxtra\assets\sprites")
UPSCALE_MODEL  = "4x-UltraSharpV2.safetensors"
DEFAULT_SERVER = "127.0.0.1:8188"
RETILE_SCRIPT  = os.path.join(os.path.dirname(os.path.abspath(__file__)), "retile_stage.py")

# ── Prompt builder ────────────────────────────────────────────────────────────


def build_prompt(input_path: str, output_path: str, filename_prefix: str) -> dict:
    """Build a ComfyUI prompt for a single stage layer upscale."""
    input_path = input_path.replace("\\", "/")
    output_path = output_path.replace("\\", "/")

    loader_node = {
        "class_type": "Load Image Batch",
        "inputs": {
            "mode": "incremental_image",
            "seed": 0,
            "index": 0,
            "label": "layer_",
            "path": os.path.dirname(input_path),
            "pattern": os.path.basename(input_path),
            "allow_RGBA_output": "true",
            "filename_text_extension": "false"
        }
    }

    save_node = {
        "class_type": "Image Save",
        "inputs": {
            "images": ["34", 0],
            "output_path": output_path,
            "filename_prefix": filename_prefix,
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


# ── Stage discovery ───────────────────────────────────────────────────────────

def find_stage_layers(stages_root: str, stage_filter: int = None):
    """
    Find all stage directories with layer_*.json metadata.
    Returns list of (stage_dir, json_path, layer_png_path) tuples.
    """
    results = []
    # Look for _indexed directories (these have the runtime background tiles)
    pattern = os.path.join(stages_root, "stage_*_indexed")
    for stage_dir in sorted(glob.glob(pattern)):
        # Extract stage index from directory name
        dirname = os.path.basename(stage_dir)
        try:
            stage_idx = int(dirname.split("_")[1])
        except (IndexError, ValueError):
            continue

        if stage_filter is not None and stage_idx != stage_filter:
            continue

        # Find all layer JSONs
        for json_path in sorted(glob.glob(os.path.join(stage_dir, "layer_*.json"))):
            layer_name = os.path.splitext(os.path.basename(json_path))[0]  # "layer_0"
            layer_png = os.path.join(stage_dir, f"{layer_name}.png")
            if os.path.exists(layer_png):
                results.append((stage_dir, json_path, layer_png, stage_idx, layer_name))

    return results


# ── Main ───────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Batch upscale stage layers via ComfyUI + retile")
    parser.add_argument("--server", default=DEFAULT_SERVER, help="ComfyUI server address")
    parser.add_argument("--stage", type=int, default=None, help="Process a single stage index")
    parser.add_argument("--dry-run", action="store_true", help="Preview without processing")
    parser.add_argument("--skip-retile", action="store_true", help="Only upscale, skip retiling")
    parser.add_argument("--output", default=TILES_OUTPUT, help="Retiled tiles output dir")
    args = parser.parse_args()

    # Verify ComfyUI is running
    if not args.dry_run:
        try:
            urllib.request.urlopen(f"http://{args.server}/system_stats", timeout=5)
            print(f"✅ Connected to ComfyUI at {args.server}")
        except Exception as e:
            print(f"❌ Cannot connect to ComfyUI at {args.server}")
            print(f"   Start ComfyUI first: cd D:\\ComfyUI_windows_portable && run_nvidia_gpu.bat")
            print(f"   Error: {e}")
            sys.exit(1)

    # Find all layers
    layers = find_stage_layers(STAGES_ROOT, args.stage)
    if not layers:
        print(f"❌ No stage layers found in {STAGES_ROOT}")
        print(f"   Run 'python tools/extract_stage.py all --mode=indexed' first")
        sys.exit(1)

    client_id = str(uuid.uuid4())
    total = len(layers)

    print(f"\n🏟️  Stage Layer Upscale + Retile Pipeline")
    print(f"📁 Source:  {STAGES_ROOT}")
    print(f"📂 Output:  {args.output}")
    print(f"🔧 Model:   {UPSCALE_MODEL}")
    print(f"📊 Layers:  {total}")
    print(f"{'─' * 60}")

    upscaled_dirs = set()

    for idx, (stage_dir, json_path, layer_png, stage_idx, layer_name) in enumerate(layers, 1):
        stage_name = os.path.basename(stage_dir)
        label = f"[{idx}/{total}] stage {stage_idx:02d} {layer_name}"

        if args.dry_run:
            with open(json_path) as f:
                meta = json.load(f)
            print(f"  🔍 {label} ({meta['tile_count']} tiles)")
            continue

        print(f"\n{label}")

        # Upscale the layer image
        upscaled_dir = os.path.join(stage_dir, "upscaled")
        os.makedirs(upscaled_dir, exist_ok=True)
        upscaled_dirs.add(stage_dir)

        prefix = f"{layer_name}_upscaled"
        prompt = build_prompt(layer_png, upscaled_dir, prefix)

        try:
            prompt_id = queue_prompt(args.server, prompt, client_id)
            success = wait_for_completion(args.server, prompt_id, label)
            if not success:
                continue
        except urllib.error.HTTPError as e:
            error_body = e.read().decode("utf-8", errors="replace")
            print(f"\n  ❌ API error: {e.code}")
            print(f"     {error_body[:500]}")
            continue
        except Exception as e:
            print(f"\n  ❌ Error: {e}")
            continue

        # The upscaled file is saved by ComfyUI with the prefix we gave
        # Find it in the upscaled directory
        upscaled_candidates = sorted(glob.glob(os.path.join(upscaled_dir, f"{prefix}*")))
        if upscaled_candidates:
            # Rename to the standard name retile_stage.py expects
            expected_name = os.path.join(stage_dir, f"{layer_name}_upscaled.png")
            os.replace(upscaled_candidates[0], expected_name)
            print(f"  📄 Saved: {os.path.basename(expected_name)}")

    # Retile all upscaled stages
    if not args.dry_run and not args.skip_retile and upscaled_dirs:
        print(f"\n{'─' * 60}")
        print(f"🔲 Retiling upscaled layers into runtime tiles...")
        print(f"   Output: {args.output}")

        os.makedirs(args.output, exist_ok=True)

        for stage_dir in sorted(upscaled_dirs):
            stage_name = os.path.basename(stage_dir)
            print(f"\n  📦 {stage_name}")
            try:
                result = subprocess.run(
                    [sys.executable, RETILE_SCRIPT, stage_dir, "--output", args.output],
                    capture_output=True, text=True
                )
                if result.stdout:
                    for line in result.stdout.strip().split("\n"):
                        print(f"    {line}")
                if result.returncode != 0 and result.stderr:
                    print(f"    ⚠️  {result.stderr.strip()[:200]}")
            except Exception as e:
                print(f"    ❌ Retile error: {e}")

    print(f"\n{'─' * 60}")
    if args.dry_run:
        print(f"🔍 Dry run: {total} layers across {len(set(s[3] for s in layers))} stages")
    else:
        print(f"🎉 Done! {total} layers upscaled and retiled.")
        print(f"   Tiles: {args.output}")
        print(f"   Rebuild (recompile.bat) to deploy to build output.")


if __name__ == "__main__":
    main()
