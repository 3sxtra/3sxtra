import os
import csv
import math
from PIL import Image


def get_bank_file(output_dir, page, bank, w, h):
    if bank >= 32 or (w == 5 and h == 3):
        return os.path.join(
            output_dir, f"page_{page}_face_banks", f"face_bank_{bank:03d}.png"
        )
    return os.path.join(output_dir, f"page_{page}_banks", f"bank_{bank:02d}.png")


def build_perfect_atlases():
    csv_path = os.path.abspath("ui_bank_map.csv")
    output_dir = os.path.abspath("output/ui_pages")

    if not os.path.exists(csv_path):
        print(f"Cannot find {csv_path}")
        return

    # Load base transparent canvases and active cell masks
    canvases = {}
    active_cells = {}
    mapped_cells = {}

    for i in range(5):
        canvases[i] = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
        mapped_cells[i] = {}
        active_cells[i] = set()

        # Determine which cells are actually non-empty in the raw page
        base_page = os.path.join(output_dir, f"page_{i}.png")
        if os.path.exists(base_page):
            with Image.open(base_page) as im:
                im = im.convert("RGBA")
                for cy in range(32):
                    for cx in range(32):
                        region = im.crop((cx * 8, cy * 8, cx * 8 + 8, cy * 8 + 8))
                        # If any pixel is not fully transparent
                        if any(p[3] > 0 for p in region.getdata()):
                            active_cells[i].add((cx, cy))

    # Parse what we firmly know
    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            page = int(row["page"])
            if page > 4:
                continue

            bank = int(row["bank"])
            cx = int(row["cell_x"])
            cy = int(row["cell_y"])
            w = int(row["width"])
            h = int(row["height"])

            for yy in range(cy, cy + h):
                for xx in range(cx, cx + w):
                    if (xx, yy) not in mapped_cells[page]:
                        mapped_cells[page][(xx, yy)] = (bank, w, h)

    # First Pass: Paste Knowns
    # Second Pass: Deduce Unknowns
    for page in range(5):
        for cy in range(32):
            for cx in range(32):
                if (cx, cy) not in active_cells[page]:
                    continue

                target_bank = None
                target_w = 1
                target_h = 1

                if (cx, cy) in mapped_cells[page]:
                    target_bank, target_w, target_h = mapped_cells[page][(cx, cy)]
                else:
                    # Deduce via Nearest Neighbor
                    best_dist = float("inf")
                    for (kx, ky), (kbank, kw, kh) in mapped_cells[page].items():
                        # Don't use face banks as inference points to prevent smearing giant face palette palettes onto HUD elements
                        if kbank >= 32 or (kw == 5 and kh == 3):
                            continue
                        dist = math.hypot(cx - kx, cy - ky)
                        if dist < best_dist:
                            best_dist = dist
                            target_bank = kbank

                if target_bank is not None:
                    bank_file = get_bank_file(
                        output_dir, page, target_bank, target_w, target_h
                    )
                    if os.path.exists(bank_file):
                        with Image.open(bank_file) as src:
                            px, py = cx * 8, cy * 8
                            region = src.crop((px, py, px + 8, py + 8))
                            canvases[page].paste(region, (px, py))

    # Save perfect atlases
    for i, canvas in canvases.items():
        out_path = os.path.join(output_dir, f"perfect_page_{i}.png")
        canvas.save(out_path)
        print(f"Saved {out_path}")


if __name__ == "__main__":
    build_perfect_atlases()
