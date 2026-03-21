#!/usr/bin/env python3
"""
Smoke tests for sprite_common.py — no AFS file required.

Run:  python -m pytest test_sprite_common.py -v
"""

import struct
import pytest

from sprite_common import (
    decode_color_abgr1555,
    decode_palette_banks,
    lz_ext_p6_fx,
    unswizzle,
    TileChip,
    COLORS_PER_BANK,
    PALETTE_BANK_BYTES,
    TOTAL_BANKS,
    CHAR_PAL_ROWS,
)


# ══════════════════════════════════════════════════════════════════════════════
# decode_color_abgr1555
# ══════════════════════════════════════════════════════════════════════════════


class TestDecodeColorABGR1555:
    def test_black(self):
        """val=0, idx=1 → (0, 0, 0, 255) — opaque black."""
        assert decode_color_abgr1555(0x0000, 1) == (0, 0, 0, 255)

    def test_transparent_index_zero(self):
        """val=0, idx=0 → fully transparent."""
        assert decode_color_abgr1555(0x0000, 0) == (0, 0, 0, 0)

    def test_nonzero_at_index_zero(self):
        """val≠0, idx=0 → opaque."""
        r, g, b, a = decode_color_abgr1555(0x7C00, 0)  # R=31, G=0, B=0
        assert a == 255

    def test_pure_red(self):
        """R=31, G=0, B=0 → (255, 0, 0, 255)."""
        r, g, b, a = decode_color_abgr1555(0x7C00, 1)
        assert r == 255
        assert g == 0
        assert b == 0

    def test_pure_green(self):
        """R=0, G=31, B=0 → (0, 255, 0, 255)."""
        r, g, b, a = decode_color_abgr1555(0x03E0, 1)
        assert g == 255
        assert r == 0
        assert b == 0

    def test_pure_blue(self):
        """R=0, G=0, B=31 → (0, 0, 255, 255)."""
        r, g, b, a = decode_color_abgr1555(0x001F, 1)
        assert b == 255

    def test_white(self):
        """R=31, G=31, B=31 → (255, 255, 255, 255)."""
        r, g, b, a = decode_color_abgr1555(0x7FFF, 1)
        assert (r, g, b) == (255, 255, 255)

    def test_accurate_rounding(self):
        """Verify (c5 * 255 + 15) // 31 gives correct intermediate values."""
        # R=16, G=16, B=16 → (132, 132, 132)
        val = (16 << 10) | (16 << 5) | 16
        r, g, b, a = decode_color_abgr1555(val, 1)
        assert r == (16 * 255 + 15) // 31
        assert r == g == b


# ══════════════════════════════════════════════════════════════════════════════
# decode_palette_banks
# ══════════════════════════════════════════════════════════════════════════════


class TestDecodePaletteBanks:
    def test_single_bank(self):
        """128 bytes → 1 bank of 64 colors."""
        data = b'\x00\x00' * COLORS_PER_BANK
        banks = decode_palette_banks(data)
        assert len(banks) == 1
        assert len(banks[0]) == COLORS_PER_BANK
        # All colors should be (0,0,0,0) because index 0 with val 0 → transparent
        assert banks[0][0] == (0, 0, 0, 0)

    def test_two_banks(self):
        """256 bytes → 2 banks."""
        data = b'\x00\x00' * (COLORS_PER_BANK * 2)
        banks = decode_palette_banks(data)
        assert len(banks) == 2

    def test_nonzero_color(self):
        """Verify a specific color is decoded correctly."""
        # Write pure red (R=31, G=0, B=0) at index 1
        data = bytearray(PALETTE_BANK_BYTES)
        struct.pack_into("<H", data, 2, 0x7C00)  # index 1, offset 2
        banks = decode_palette_banks(bytes(data))
        r, g, b, a = banks[0][1]
        assert r == 255
        assert g == 0
        assert b == 0
        assert a == 255

    def test_empty_data(self):
        """Empty data → 0 banks."""
        assert decode_palette_banks(b'') == []

    def test_partial_bank(self):
        """Data shorter than one bank → 0 banks (truncated)."""
        data = b'\x00' * (PALETTE_BANK_BYTES - 1)
        banks = decode_palette_banks(data)
        assert len(banks) == 0


# ══════════════════════════════════════════════════════════════════════════════
# lz_ext_p6_fx
# ══════════════════════════════════════════════════════════════════════════════


class TestLzExtP6Fx:
    def test_literal_bytes(self):
        """Literal mode (c == 0x00): each byte < 0x40 copies directly."""
        src = bytes([0x01, 0x02, 0x03, 0x10, 0x3F])
        result = lz_ext_p6_fx(src, 5)
        assert result == bytes([0x01, 0x02, 0x03, 0x10, 0x3F])

    def test_output_truncation(self):
        """Output should be exactly dst_size bytes."""
        src = bytes([0x01, 0x02, 0x03])
        result = lz_ext_p6_fx(src, 2)
        assert len(result) == 2
        assert result == bytes([0x01, 0x02])

    def test_empty_source(self):
        """Empty source → zero-filled output."""
        result = lz_ext_p6_fx(b'', 4)
        assert result == b'\x00\x00\x00\x00'

    def test_dst_larger_than_src(self):
        """Output larger than source with only literals → zero-padded."""
        src = bytes([0x05])
        result = lz_ext_p6_fx(src, 4)
        assert len(result) == 4
        assert result[0] == 0x05


# ══════════════════════════════════════════════════════════════════════════════
# unswizzle
# ══════════════════════════════════════════════════════════════════════════════


class TestUnswizzle:
    def test_output_size(self):
        """Output should be td*td bytes."""
        pdata = bytes(16 * 16)
        result = unswizzle(pdata, 16)
        assert len(result) == 16 * 16

    def test_deterministic(self):
        """Same input produces same output."""
        pdata = bytes(range(256))  # 16×16
        r1 = unswizzle(pdata, 16)
        r2 = unswizzle(pdata, 16)
        assert r1 == r2

    def test_small_tile(self):
        """8×8 tile doesn't crash."""
        pdata = bytes(64)
        result = unswizzle(pdata, 8)
        assert len(result) == 64


# ══════════════════════════════════════════════════════════════════════════════
# TileChip NamedTuple
# ══════════════════════════════════════════════════════════════════════════════


class TestTileChip:
    def test_named_access(self):
        tile = TileChip(dx=10, dy=20, attr=0x8000, code=5, td=16, dw=24, dh=32, px=b'\x00')
        assert tile.dx == 10
        assert tile.dy == 20
        assert tile.attr == 0x8000
        assert tile.code == 5
        assert tile.td == 16
        assert tile.dw == 24
        assert tile.dh == 32
        assert tile.px == b'\x00'

    def test_positional_access(self):
        """NamedTuple still supports index access for backward compat."""
        tile = TileChip(1, 2, 3, 4, 5, 6, 7, b'\xFF')
        assert tile[0] == 1
        assert tile[7] == b'\xFF'

    def test_immutable(self):
        tile = TileChip(0, 0, 0, 0, 8, 8, 8, b'')
        with pytest.raises(AttributeError):
            tile.dx = 99


# ══════════════════════════════════════════════════════════════════════════════
# Constants
# ══════════════════════════════════════════════════════════════════════════════


class TestConstants:
    def test_palette_bank_bytes(self):
        assert PALETTE_BANK_BYTES == COLORS_PER_BANK * 2

    def test_total_banks(self):
        assert TOTAL_BANKS == 512

    def test_char_pal_rows(self):
        assert CHAR_PAL_ROWS == 28


# ══════════════════════════════════════════════════════════════════════════════
# TexGroupEntry NamedTuple
# ══════════════════════════════════════════════════════════════════════════════


from sprite_common import TexGroupEntry, TEXGRPDAT


class TestTexGroupEntry:
    def test_named_access(self):
        e = TexGroupEntry(group_idx=52, num_of_1st=35024, apfn=1386,
                          to_tex=4568, desc="Stage 00 (Gill/Boss) sprites")
        assert e.group_idx == 52
        assert e.num_of_1st == 35024
        assert e.apfn == 1386
        assert e.to_tex == 4568
        assert "Gill" in e.desc

    def test_texgrpdat_uses_namedtuple(self):
        """All TEXGRPDAT entries should be TexGroupEntry instances."""
        assert len(TEXGRPDAT) > 0
        for entry in TEXGRPDAT:
            assert isinstance(entry, TexGroupEntry), f"{entry} is not TexGroupEntry"

    def test_texgrpdat_unique_group_ids(self):
        """Group indices should be unique."""
        ids = [e.group_idx for e in TEXGRPDAT]
        assert len(ids) == len(set(ids))


# ══════════════════════════════════════════════════════════════════════════════
# sprite_compositor module import
# ══════════════════════════════════════════════════════════════════════════════


class TestCompositorModule:
    def test_import(self):
        """sprite_compositor should be importable without errors."""
        import sprite_compositor
        assert hasattr(sprite_compositor, 'extract_stage_frame')
        assert hasattr(sprite_compositor, 'build_stage_colorram')
        assert hasattr(sprite_compositor, 'extract_char_frame')


# ══════════════════════════════════════════════════════════════════════════════
# Integration Tests (require AFS file — auto-skip if not found)
# ══════════════════════════════════════════════════════════════════════════════


import os

_DEFAULT_AFS = os.environ.get(
    "SF33RD_AFS",
    r"C:\Users\dov\AppData\Roaming\CrowdedStreet\3SX\resources\SF33RD.AFS"
)
_AFS_EXISTS = os.path.isfile(_DEFAULT_AFS)
_skip_no_afs = pytest.mark.skipif(not _AFS_EXISTS, reason="AFS file not found")


@_skip_no_afs
class TestIntegrationAFS:
    """Integration tests that read from the real AFS file."""

    def test_afs_read(self):
        """AFS should have ~1500+ entries."""
        from sprite_common import read_afs
        entries = read_afs(_DEFAULT_AFS)
        assert len(entries) > 1400

    def test_stage_frame_extraction(self):
        """Extract frame 0 of group 52 (Gill boss stage) → non-None RGBA image."""
        import struct
        from sprite_common import read_afs, read_afs_file, STAGE_PAL_AFS
        from sprite_compositor import extract_stage_frame, build_stage_colorram

        entries = read_afs(_DEFAULT_AFS)
        grp52 = next(e for e in TEXGRPDAT if e.group_idx == 52)

        pal_banks = build_stage_colorram(_DEFAULT_AFS, entries,
                                         STAGE_PAL_AFS[0])
        data = read_afs_file(_DEFAULT_AFS, entries[grp52.apfn])
        img = extract_stage_frame(data, grp52.to_tex, 0, pal_banks,
                                  colcd_base=300, rendering_mode=33)
        assert img is not None
        assert img.mode == "RGBA"
        assert img.size[0] > 0 and img.size[1] > 0
