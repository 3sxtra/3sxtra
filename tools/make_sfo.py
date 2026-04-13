import sys
import struct


def build_sfo(title, title_id, version):
    keys = b"APP_VER\0CATEGORY\0PS3_SYSTEM_VER\0TITLE\0TITLE_ID\0VERSION\0"

    app_ver = version.encode("utf-8") + b"\0"
    app_ver = app_ver.ljust(8, b"\0")

    category = b"HG\0\0"

    sys_ver = b"04.8100\0"
    sys_ver = sys_ver.ljust(8, b"\0")

    title_bytes = title.encode("utf-8") + b"\0"
    title_bytes = title_bytes.ljust(128, b"\0")

    title_id_bytes = title_id.encode("utf-8") + b"\0"
    title_id_bytes = title_id_bytes.ljust(16, b"\0")

    sys_version = b"01.00\0\0\0"

    entries = [
        ("APP_VER", app_ver, 0x0204),
        ("CATEGORY", category, 0x0204),
        ("PS3_SYSTEM_VER", sys_ver, 0x0204),
        ("TITLE", title_bytes, 0x0204),
        ("TITLE_ID", title_id_bytes, 0x0204),
        ("VERSION", sys_version, 0x0204),
    ]

    key_offset = 20 + 16 * len(entries)
    while key_offset % 4 != 0:
        key_offset += 1

    data_offset = key_offset + len(keys)
    while data_offset % 4 != 0:
        data_offset += 1

    header = struct.pack(
        "<4sIIII", b"\0PSF", 0x0101, key_offset, data_offset, len(entries)
    )

    index_table = b""
    k_off = 0
    d_off = 0
    for key, val, fmt in entries:
        index_table += struct.pack(
            "<HHIII", k_off, fmt, len(val.rstrip(b"\0")) + 1, len(val), d_off
        )
        k_off += len(key) + 1
        d_off += len(val)

    key_pad = b"\0" * (key_offset - len(header) - len(index_table))
    data_pad = b"\0" * (data_offset - key_offset - len(keys))

    data_payload = b"".join(val for _, val, _ in entries)

    out = header + index_table + key_pad + keys + data_pad + data_payload
    return out


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: make_sfo.py <output_file>")
        sys.exit(1)

    out_file = sys.argv[1]
    sfo_data = build_sfo("3SX Engine", "3SX00001", "01.00")

    with open(out_file, "wb") as f:
        f.write(sfo_data)

    sys.exit(0)
