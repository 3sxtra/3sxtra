#!/usr/bin/env python3
"""
Retile upscaled stage layer images into runtime BG tile overrides.

Reads a layer JSON metadata file (produced by extract_stage.py) and the
corresponding upscaled layer image, then splits it back into individual
128×128 tiles (scaled proportionally) named with the composite key format
expected by the renderer plugin: bg_{composite_key}.png

Usage:
  python retile_stage.py output/stages/stage_00_gill/layer_0.json [--upscaled layer_0_upscaled.png]
  python retile_stage.py output/stages/stage_00_gill/               # process all layers
  python retile_stage.py output/stages/stage_00_gill/ --output assets/sprites/
"""

import json
import os
import sys
import glob
from PIL import Image


def retile_layer(json_path, upscaled_path=None, output_dir=None):
    """Split an upscaled layer image back into individual tiles using metadata."""
    with open(json_path, "r") as f:
        meta = json.load(f)

    stage_dir = os.path.dirname(json_path)
    layer = meta["layer"]
    tile_size = meta["tile_size"]  # original tile size (128)
    grid_w = meta["grid_width"]  # 8
    grid_h = meta["grid_height"]  # 4
    tiles = meta["tiles"]

    if not tiles:
        print(f"  Layer {layer}: no tiles to retile")
        return 0

    # Find the upscaled image
    if upscaled_path is None:
        # Try common naming patterns
        candidates = [
            os.path.join(stage_dir, f"layer_{layer}_upscaled.png"),
            os.path.join(stage_dir, f"layer_{layer}_hd.png"),
            os.path.join(stage_dir, f"layer_{layer}_4x.png"),
            os.path.join(stage_dir, f"layer_{layer}.png"),  # fallback to original
        ]
        for c in candidates:
            if os.path.exists(c):
                upscaled_path = c
                break

    if upscaled_path is None or not os.path.exists(upscaled_path):
        print(f"  Layer {layer}: upscaled image not found (tried {candidates})")
        return 0

    img = Image.open(upscaled_path)
    img_w, img_h = img.size

    # Compute the expected original size and derive scale
    orig_w = grid_w * tile_size  # 1024
    orig_h = grid_h * tile_size  # 512
    scale_x = img_w / orig_w
    scale_y = img_h / orig_h

    if abs(scale_x - scale_y) > 0.01:
        print(f"  WARNING: non-uniform scale ({scale_x:.2f}x, {scale_y:.2f}y)")

    scale = scale_x
    hd_tile_size = int(tile_size * scale)

    if output_dir is None:
        output_dir = os.path.join(stage_dir, "retiled")
    os.makedirs(output_dir, exist_ok=True)

    count = 0
    for entry in tiles:
        row = entry["row"]
        col = entry["col"]
        composite_key = entry["composite_key"]

        # Compute pixel position in the upscaled image
        x = int(col * tile_size * scale)
        y = int(row * tile_size * scale)

        # Crop the tile
        tile_img = img.crop((x, y, x + hd_tile_size, y + hd_tile_size))

        # Save with runtime naming convention
        out_path = os.path.join(output_dir, f"bg_{composite_key}.png")
        tile_img.save(out_path)
        count += 1

    scale_str = f"{scale:.0f}x" if scale == int(scale) else f"{scale:.2f}x"
    print(
        f"  Layer {layer}: {count} tiles retiled at {scale_str} "
        f"({hd_tile_size}x{hd_tile_size}px) -> {output_dir}/"
    )
    return count


def main():
    if len(sys.argv) < 2:
        print(
            "Usage: retile_stage.py <layer.json or stage_dir> [--upscaled <image>] [--output <dir>]"
        )
        sys.exit(1)

    target = sys.argv[1]
    upscaled = None
    output = None

    # Parse optional args
    i = 2
    while i < len(sys.argv):
        if sys.argv[i] == "--upscaled" and i + 1 < len(sys.argv):
            upscaled = sys.argv[i + 1]
            i += 2
        elif sys.argv[i] == "--output" and i + 1 < len(sys.argv):
            output = sys.argv[i + 1]
            i += 2
        else:
            i += 1

    if os.path.isfile(target) and target.endswith(".json"):
        # Single layer
        retile_layer(target, upscaled, output)
    elif os.path.isdir(target):
        # Process all layer JSONs in the directory
        json_files = sorted(glob.glob(os.path.join(target, "layer_*.json")))
        if not json_files:
            print(f"No layer_*.json files found in {target}")
            sys.exit(1)

        total = 0
        for jf in json_files:
            total += retile_layer(jf, None, output)
        print(f"\nTotal: {total} tiles retiled")
    else:
        print(f"Error: {target} is not a .json file or directory")
        sys.exit(1)


if __name__ == "__main__":
    main()
