#!/usr/bin/env python3
"""
Shared ComfyUI API helpers — prompt submission, history polling, completion wait,
and the common alpha-preserving upscale workflow graph.

Used by comfyui_batch_upscale.py and comfyui_batch_upscale_stages.py.
"""

import json
import time
import urllib.request
import urllib.error


def queue_prompt(server, prompt, client_id):
    """Submit a prompt to ComfyUI and return prompt_id."""
    data = json.dumps({"prompt": prompt, "client_id": client_id}).encode("utf-8")

    req = urllib.request.Request(
        f"http://{server}/prompt",
        data=data,
        headers={"Content-Type": "application/json"},
    )
    resp = urllib.request.urlopen(req)
    result = json.loads(resp.read())
    return result["prompt_id"]


def get_history(server, prompt_id):
    """Get the execution history for a prompt."""
    url = f"http://{server}/history/{prompt_id}"
    resp = urllib.request.urlopen(url)
    return json.loads(resp.read())


def get_queue(server):
    """Get current queue status."""
    resp = urllib.request.urlopen(f"http://{server}/queue")
    return json.loads(resp.read())


def wait_for_completion(server, prompt_id, label=""):
    """Poll until a prompt finishes executing."""
    spinner = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"]
    i = 0
    while True:
        history = get_history(server, prompt_id)
        if prompt_id in history:
            status = history[prompt_id].get("status", {})
            if status.get("completed", False) or status.get("status_str") == "success":
                print(f"\r  ✅ {label} complete.                    ")
                return True
            if status.get("status_str") == "error":
                print(f"\r  ❌ {label} FAILED!")
                msgs = status.get("messages", [])
                for msg in msgs:
                    print(f"     {msg}")
                return False

        print(
            f"\r  {spinner[i % len(spinner)]} {label} processing...", end="", flush=True
        )
        i += 1
        time.sleep(1.0)


# ══════════════════════════════════════════════════════════════════════════════
# Shared upscale workflow graph
# ══════════════════════════════════════════════════════════════════════════════


def build_upscale_workflow(model_name, loader_node, save_node):
    """Build the common alpha-preserving 4x upscale workflow.

    The 7-node graph:
      LoadImage → SplitAlpha → UpscaleRGB  \\
                                              → JoinAlpha → SaveImage
                   SplitAlpha → Mask2Img → UpscaleMask → Img2Mask /

    Args:
        model_name:  upscale model filename (e.g. "4x-UltraSharpV2.safetensors")
        loader_node: dict with LoadImageBatch node config (class_type + inputs)
        save_node:   dict with ImageSave node config (class_type + inputs)

    Returns:
        Complete ComfyUI API prompt dict with node IDs matching original workflow.
    """
    return {
        # Node 21: Load upscale model
        "21": {
            "class_type": "UpscaleModelLoader",
            "inputs": {"model_name": model_name},
        },
        # Node 26: Image loader (caller-provided)
        "26": loader_node,
        # Node 30: Split image into RGB + Alpha mask
        "30": {"class_type": "SplitImageWithAlpha", "inputs": {"image": ["26", 0]}},
        # Node 20: Upscale the RGB image
        "20": {
            "class_type": "ImageUpscaleWithModel",
            "inputs": {"upscale_model": ["21", 0], "image": ["30", 0]},
        },
        # Node 31: Convert alpha mask to image (so we can upscale it)
        "31": {"class_type": "MaskToImage", "inputs": {"mask": ["30", 1]}},
        # Node 32: Upscale the alpha mask (as an image)
        "32": {
            "class_type": "ImageUpscaleWithModel",
            "inputs": {"upscale_model": ["21", 0], "image": ["31", 0]},
        },
        # Node 33: Convert upscaled alpha image back to mask
        "33": {
            "class_type": "ImageToMask",
            "inputs": {"image": ["32", 0], "channel": "red"},
        },
        # Node 34: Recombine upscaled RGB with upscaled alpha
        "34": {
            "class_type": "JoinImageWithAlpha",
            "inputs": {"image": ["20", 0], "alpha": ["33", 0]},
        },
        # Node 27: Image saver (caller-provided)
        "27": save_node,
    }
