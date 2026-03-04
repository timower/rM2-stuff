#!/usr/bin/env python3
"""Generate glyph.h from JetBrains Mono Nerd Font TTF files.

Rasterizes font glyphs into the bitmap format expected by yaft's glyph.h.
Each glyph is a 16x32 (width=1) or 32x32 (width=2) 1-bit bitmap.

Bitmap encoding: MSB = leftmost pixel, LSB = rightmost.
  width=1: bits 15..0 of uint32_t
  width=2: bits 31..0 of uint32_t
"""

import argparse
import sys
import unicodedata
from PIL import Image, ImageDraw, ImageFont

CELL_WIDTH = 16
CELL_HEIGHT = 32

# Codepoint ranges to include
RANGES = [
    (0x0020, 0x007E),  # ASCII printable
    (0x00A0, 0x024F),  # Latin-1 Supplement + Latin Extended-A/B
    (0x0370, 0x04FF),  # Greek and Coptic + Cyrillic
    (0x2000, 0x2BFF),  # General Punctuation, Arrows, Math Operators, etc.
    (0x2500, 0x259F),  # Box Drawing + Block Elements
    (0x2600, 0x27BF),  # Misc Symbols + Dingbats
    (0xE0A0, 0xE0D7),  # Powerline symbols
    (0xE200, 0xF8FF),  # Nerd Font symbols (Seti-UI, Devicons, FA, etc.)
    (0xFFFD, 0xFFFD),  # Replacement character
]

# Required glyphs (terminal.cpp checks these exist)
REQUIRED_CODES = [0x0020, 0x003F, 0xFFFD]

# Box Drawing + Block Elements: must extend to cell edges
BOX_DRAWING_RANGE = (0x2500, 0x259F)
# Powerline symbols: should fill full cell height
POWERLINE_RANGE = (0xE0A0, 0xE0D7)


def is_box_drawing(code):
    return BOX_DRAWING_RANGE[0] <= code <= BOX_DRAWING_RANGE[1]


def is_powerline(code):
    return POWERLINE_RANGE[0] <= code <= POWERLINE_RANGE[1]


def codepoint_width(code):
    """Return display width: 2 for wide/fullwidth, 1 otherwise."""
    try:
        eaw = unicodedata.east_asian_width(chr(code))
    except (ValueError, OverflowError):
        return 1
    return 2 if eaw in ('W', 'F') else 1


def font_has_glyph(font, code):
    """Check if font has a real glyph (not .notdef) for this codepoint."""
    try:
        bbox = font.getbbox(chr(code))
        if bbox is None:
            return False
        return True
    except Exception:
        return False


def find_best_size(font_path, target_width=CELL_WIDTH, target_height=CELL_HEIGHT):
    """Find the largest point size fitting within the cell."""
    best = 8
    for size in range(8, 40):
        font = ImageFont.truetype(font_path, size)
        bbox = font.getbbox("M")
        w = bbox[2] - bbox[0]
        ascent, descent = font.getmetrics()
        total_h = ascent + descent
        if w <= target_width and total_h <= target_height:
            best = size
        else:
            break
    return best


def extend_box_drawing_edges(bitmap, cell_w, cell_h):
    """Extend box drawing lines to cell edges so they connect across cells.

    For each row, if there are lit pixels near (but not at) an edge, fill
    all pixels from the nearest lit pixel to the edge. Same for columns.
    """
    NEAR = 3

    def get_bit(val, col):
        return bool(val & (1 << ((cell_w - 1) - col)))

    def set_bit(val, col):
        return val | (1 << ((cell_w - 1) - col))

    # Extend LEFT: fill columns 0..nearest_lit for each row
    for row in range(cell_h):
        if not get_bit(bitmap[row], 0):
            for col in range(1, NEAR + 1):
                if col < cell_w and get_bit(bitmap[row], col):
                    for fill in range(0, col):
                        bitmap[row] = set_bit(bitmap[row], fill)
                    break

    # Extend RIGHT: fill columns nearest_lit..last for each row
    for row in range(cell_h):
        last = cell_w - 1
        if not get_bit(bitmap[row], last):
            for col in range(last - 1, last - NEAR - 1, -1):
                if col >= 0 and get_bit(bitmap[row], col):
                    for fill in range(col + 1, cell_w):
                        bitmap[row] = set_bit(bitmap[row], fill)
                    break

    # Extend TOP: fill rows 0..nearest_lit for each column
    for col in range(cell_w):
        if not get_bit(bitmap[0], col):
            for row in range(1, NEAR + 1):
                if row < cell_h and get_bit(bitmap[row], col):
                    for fill in range(0, row):
                        bitmap[fill] = set_bit(bitmap[fill], col)
                    break

    # Extend BOTTOM: fill rows nearest_lit..last for each column
    for col in range(cell_w):
        last_row = cell_h - 1
        if not get_bit(bitmap[last_row], col):
            for row in range(last_row - 1, last_row - NEAR - 1, -1):
                if row >= 0 and get_bit(bitmap[row], col):
                    for fill in range(row + 1, cell_h):
                        bitmap[fill] = set_bit(bitmap[fill], col)
                    break

    return bitmap


def rasterize_glyph(font, code, width):
    """Render a single glyph to a 1-bit bitmap array.

    Returns list of 32 uint32_t values, or None if glyph is missing.
    """
    char = chr(code)
    cell_w = CELL_WIDTH * width
    cell_h = CELL_HEIGHT

    # Create grayscale image
    img = Image.new('L', (cell_w, cell_h), 0)
    draw = ImageDraw.Draw(img)

    # Get font metrics for baseline positioning
    ascent, descent = font.getmetrics()

    # Get character bounding box
    bbox = font.getbbox(char)
    if bbox is None:
        return None

    char_w = bbox[2] - bbox[0]

    # Center horizontally
    x_offset = (cell_w - char_w) // 2 - bbox[0]

    # Baseline at ~75% of cell height (row 24 of 32)
    baseline_target = int(cell_h * 0.75)
    y_offset = baseline_target - ascent

    draw.text((x_offset, y_offset), char, fill=255, font=font)

    # Threshold to 1-bit
    pixels = img.load()
    bitmap = []
    for row in range(cell_h):
        val = 0
        for col in range(cell_w):
            if pixels[col, row] >= 128:
                # MSB = leftmost pixel
                bit_pos = (cell_w - 1) - col
                val |= (1 << bit_pos)
        bitmap.append(val)

    # Post-process: extend box drawing to cell edges
    if is_box_drawing(code):
        bitmap = extend_box_drawing_edges(bitmap, cell_w, cell_h)

    return bitmap


def enumerate_codepoints(font):
    """Get all codepoints in our ranges that the font supports."""
    codepoints = set()
    for start, end in RANGES:
        for code in range(start, end + 1):
            if font_has_glyph(font, code):
                codepoints.add(code)

    # Ensure required codes are present
    for code in REQUIRED_CODES:
        codepoints.add(code)

    return sorted(codepoints)


def generate_glyphs(font_path, name="glyphs"):
    """Generate all glyph entries for a font file."""
    # Find best size
    size = find_best_size(font_path)
    font = ImageFont.truetype(font_path, size)
    print(f"  Font size: {size}pt", file=sys.stderr)

    codepoints = enumerate_codepoints(font)
    print(f"  Codepoints found: {len(codepoints)}", file=sys.stderr)

    entries = []
    for code in codepoints:
        width = codepoint_width(code)
        bitmap = rasterize_glyph(font, code, width)
        if bitmap is None:
            # For required glyphs, use empty bitmap
            if code in REQUIRED_CODES:
                bitmap = [0] * CELL_HEIGHT
            else:
                continue
        entries.append((code, width, bitmap))

    print(f"  Glyphs generated: {len(entries)}", file=sys.stderr)
    return entries


def format_bitmap_value(val, width):
    """Format a bitmap value as hex."""
    if width == 2:
        if val == 0:
            return "0x0"
        return f"0x{val:X}"
    else:
        if val == 0:
            return "0x0"
        return f"0x{val:X}"


def write_glyph_entries(f, entries, array_name):
    """Write glyph array entries to file."""
    f.write(f"static const struct glyph_t {array_name}[] = {{\n")

    for i, (code, width, bitmap) in enumerate(entries):
        vals = [format_bitmap_value(v, width) for v in bitmap]

        # Format: { code, width, { v0, v1, ... v31 } }
        # Break bitmap into lines of 8 values
        f.write(f"  {{ {code}, {width}, {{ ")
        for row_start in range(0, 32, 8):
            chunk = vals[row_start:row_start + 8]
            line = ", ".join(chunk)
            if row_start == 0:
                f.write(line)
            else:
                f.write(f",\n{' ' * (len(str(code)) + 13)}{line}")
        f.write(" } }")
        if i < len(entries) - 1:
            f.write(",")
        f.write("\n")

    f.write("};\n")


def preview_glyph(font_path, code):
    """Print ASCII art preview of a glyph."""
    size = find_best_size(font_path)
    font = ImageFont.truetype(font_path, size)
    width = codepoint_width(code)
    bitmap = rasterize_glyph(font, code, width)

    if bitmap is None:
        print(f"Glyph U+{code:04X} not found in font")
        return

    cell_w = CELL_WIDTH * width
    char_name = unicodedata.name(chr(code), "?")
    print(f"U+{code:04X} ({chr(code)}) '{char_name}' width={width} size={size}pt")
    print("+" + "-" * cell_w + "+")
    for row in range(CELL_HEIGHT):
        line = "|"
        for col in range(cell_w):
            bit_pos = (cell_w - 1) - col
            if bitmap[row] & (1 << bit_pos):
                line += "#"
            else:
                line += " "
        line += "|"
        print(line)
    print("+" + "-" * cell_w + "+")


def main():
    parser = argparse.ArgumentParser(description="Generate yaft glyph.h from TTF fonts")
    parser.add_argument("--regular", required=True, help="Path to regular weight TTF")
    parser.add_argument("--bold", required=True, help="Path to bold weight TTF")
    parser.add_argument("-o", "--output", required=True, help="Output glyph.h path")
    parser.add_argument("--preview", type=str, help="Preview a codepoint (e.g. 0x41) and exit")
    parser.add_argument("--dry-run", action="store_true", help="Print stats without writing")
    args = parser.parse_args()

    if args.preview:
        code = int(args.preview, 0)
        preview_glyph(args.regular, code)
        return

    print("Generating regular glyphs...", file=sys.stderr)
    regular = generate_glyphs(args.regular, "glyphs")

    print("Generating bold glyphs...", file=sys.stderr)
    bold = generate_glyphs(args.bold, "bold_glyphs")

    if args.dry_run:
        print(f"\nDry run summary:", file=sys.stderr)
        print(f"  Regular: {len(regular)} glyphs", file=sys.stderr)
        print(f"  Bold: {len(bold)} glyphs", file=sys.stderr)
        # Show range coverage
        for name, start, end in [
            ("ASCII", 0x20, 0x7E),
            ("Latin Extended", 0xA0, 0x24F),
            ("Box Drawing", 0x2500, 0x259F),
            ("Powerline", 0xE0A0, 0xE0D7),
            ("Nerd Font", 0xE200, 0xF8FF),
        ]:
            count = sum(1 for c, w, b in regular if start <= c <= end)
            print(f"  {name} (U+{start:04X}..U+{end:04X}): {count} glyphs", file=sys.stderr)
        return

    print(f"Writing {args.output}...", file=sys.stderr)
    with open(args.output, 'w') as f:
        f.write("struct glyph_t {\n")
        f.write("  uint32_t code;\n")
        f.write("  uint8_t width;\n")
        f.write("  uint32_t bitmap[32];\n")
        f.write("};\n\n")
        f.write(f"enum {{ CELL_WIDTH = {CELL_WIDTH}, CELL_HEIGHT = {CELL_HEIGHT} }};\n\n")
        write_glyph_entries(f, regular, "glyphs")
        f.write("\n")
        write_glyph_entries(f, bold, "bold_glyphs")

    print("Done.", file=sys.stderr)


if __name__ == "__main__":
    main()
