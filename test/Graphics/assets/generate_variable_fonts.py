#!/usr/bin/env python3
"""Generate the variable font assets used by the sf::Font unit tests.

Two TrueType fonts are written next to this script:

* ``sfml-variable.ttf``
    A variable font with three variation axes -- ``wght`` (weight),
    ``wdth`` (width) and ``csln`` (a custom slant axis) -- and three
    named instances
    ("Regular", "Bold", "Condensed Italic"). Glyph outlines, advances
    (HVAR), vertical metrics (MVAR) and named instances are all built
    from three interpolated masters so that shaping, kerning, line
    spacing and rasterized glyphs visibly respond to axis changes.

* ``sfml-static.ttf``
    A regular non-variable font sharing the same character coverage.
    It is used to verify that the variable font API degrades
    gracefully on static fonts.

The committed ``.ttf`` files are the output of this script. They can be
regenerated with::

    python3 -m pip install fonttools
    python3 generate_variable_fonts.py
"""

from __future__ import annotations

import os

from fontTools import varLib
from fontTools.designspaceLib import (
    AxisDescriptor,
    DesignSpaceDocument,
    InstanceDescriptor,
    SourceDescriptor,
)
from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen


UPM = 1000
GLYPH_ORDER = [".notdef", "space", "A", "V", "H", "x", "zero"]
UNICODE_CMAP = {
    0x20: "space",
    0x41: "A",
    0x56: "V",
    0x48: "H",
    0x78: "x",
    0x30: "zero",
}

# One master per axis extreme so varLib generates an independent
# variation region for each axis. Combining several non-default axes in
# a single master would produce a region that only activates when *all*
# those axes move at once, which would make single-axis changes a no-op.
MASTER_LOCATIONS = {
    "regular": (400, 100, 0),
    "weight": (900, 100, 0),
    "width": (400, 125, 0),
    "slant": (400, 100, 12),
}

# Vertical metrics differ on the weight master on purpose so varLib
# emits an MVAR table; SFML must then report varying line spacing.
MASTER_METRICS = {
    "regular": (800, -200, 0),
    "weight": (860, -240, 30),
    "width": (800, -200, 0),
    "slant": (800, -200, 0),
}


def draw_glyph(pen, glyph_name, weight, width, slant):
    """Draw simple geometry whose outlines depend on every axis."""
    stroke = 60 + (weight - 100) * 0.09
    shear = slant * 0.8

    def rectangle(x_min, y_min, x_max, y_max):
        pen.moveTo((x_min + shear * y_min / 700.0, y_min))
        pen.lineTo((x_max + shear * y_min / 700.0, y_min))
        pen.lineTo((x_max + shear * y_max / 700.0, y_max))
        pen.lineTo((x_min + shear * y_max / 700.0, y_max))
        pen.closePath()

    if glyph_name == ".notdef":
        rectangle(100, 0, 500, 700)
    elif glyph_name == "space":
        return
    elif glyph_name == "A":
        rectangle(120, 0, 120 + stroke, 700)
        rectangle(480 - stroke, 0, 480, 700)
        rectangle(120, 320, 480, 320 + stroke)
    elif glyph_name == "V":
        rectangle(120, 700 - stroke, 120 + stroke, 700)
        rectangle(480 - stroke, 700 - stroke, 480, 700)
        rectangle(280 - stroke / 2, 0, 280 + stroke / 2, stroke)
    elif glyph_name == "H":
        rectangle(100, 0, 100 + stroke, 700)
        rectangle(500 - stroke, 0, 500, 700)
        rectangle(100, 330, 500, 330 + stroke)
    elif glyph_name == "x":
        rectangle(150, 0, 150 + stroke * 0.8, 500)
        rectangle(350 - stroke * 0.8, 0, 350, 500)
    elif glyph_name == "zero":
        rectangle(200, 0, 200 + stroke, 700)
        rectangle(400 - stroke, 0, 400, 700)
        rectangle(200, 0, 400, stroke)
        rectangle(200, 700 - stroke, 400, 700)


def build_glyphs(weight, width, slant):
    glyphs = {}
    for glyph_name in GLYPH_ORDER:
        pen = TTGlyphPen(None)
        draw_glyph(pen, glyph_name, weight, width, slant)
        glyphs[glyph_name] = pen.glyph()
    return glyphs


def build_horizontal_metrics(width):
    scale = width / 100.0
    return {
        ".notdef": (600, 0),
        "space": (int(300 * scale), 0),
        "A": (int(600 * scale), 0),
        "V": (int(600 * scale), 0),
        "H": (int(620 * scale), 0),
        "x": (int(420 * scale), 0),
        "zero": (int(500 * scale), 0),
    }


def build_master_font(weight, width, slant, ascent, descent, line_gap):
    builder = FontBuilder(UPM, isTTF=True)
    builder.setupGlyphOrder(GLYPH_ORDER)
    builder.setupCharacterMap(UNICODE_CMAP)
    builder.setupGlyf(build_glyphs(weight, width, slant))
    builder.setupHorizontalMetrics(build_horizontal_metrics(width))
    builder.setupHorizontalHeader(ascent=ascent, descent=descent, lineGap=line_gap)
    builder.setupNameTable(
        {
            "familyName": "SFML Test Variable",
            "styleName": "Regular",
            "fullName": "SFML Test Variable Regular",
            "psName": "SFMLTestVariable-Regular",
        }
    )
    builder.setupOS2(
        sTypoAscender=ascent,
        sTypoDescender=descent,
        sTypoLineGap=line_gap,
        usWinAscent=ascent,
        usWinDescent=-descent,
    )
    builder.setupPost()
    builder.setupMaxp()
    return builder.font


def build_variable_font(output_path, work_dir):
    master_paths = {}
    for label, (weight, width, slant) in MASTER_LOCATIONS.items():
        ascent, descent, line_gap = MASTER_METRICS[label]
        font = build_master_font(weight, width, slant, ascent, descent, line_gap)
        path = os.path.join(work_dir, f"master-{label}.ttf")
        font.save(path)
        master_paths[label] = path

    document = DesignSpaceDocument()
    for tag, name, minimum, default, maximum in (
        ("wght", "Weight", 100, 400, 900),
        ("wdth", "Width", 75, 100, 125),
        # A custom tag (instead of the registered "slnt" tag) keeps this
        # an ordinary interpolation axis: FreeType handles "slnt" as a
        # synthetic oblique and would ignore its gvar deltas. The axis is
        # one-sided on the positive side, a layout FreeType supports.
        ("csln", "Custom Slant", 0, 0, 12),
    ):
        axis = AxisDescriptor()
        axis.tag = tag
        axis.name = name
        axis.minimum = minimum
        axis.default = default
        axis.maximum = maximum
        document.addAxis(axis)

    for label, (weight, width, slant) in MASTER_LOCATIONS.items():
        source = SourceDescriptor()
        source.path = master_paths[label]
        source.location = {"Weight": weight, "Width": width, "Custom Slant": slant}
        source.familyName = "SFML Test Variable"
        source.styleName = label
        if label == "regular":
            source.copyInfo = True
            source.copyGroups = True
            source.copyLib = True
        document.addSource(source)

    for style_name, location in (
        ("Regular", {"Weight": 400, "Width": 100, "Custom Slant": 0}),
        ("Bold", {"Weight": 800, "Width": 100, "Custom Slant": 0}),
        ("Condensed Italic", {"Weight": 400, "Width": 75, "Custom Slant": 10}),
    ):
        instance = InstanceDescriptor()
        instance.location = location
        instance.familyName = "SFML Test Variable"
        instance.styleName = style_name
        document.addInstance(instance)

    designspace_path = os.path.join(work_dir, "sfml-variable.designspace")
    document.write(designspace_path)

    variable_font, _, _ = varLib.build(designspace_path)
    add_kerning(variable_font)
    variable_font.save(output_path)


def build_static_font(output_path):
    font = build_master_font(400, 100, 0, 800, -200, 0)
    font["name"].setName("SFML Test Static", 1, 3, 1, 0x409)
    font["name"].setName("Regular", 2, 3, 1, 0x409)
    font["name"].setName("SFML Test Static Regular", 4, 3, 1, 0x409)
    font["name"].setName("SFMLTestStatic-Regular", 6, 3, 1, 0x409)
    font.save(output_path)


def add_kerning(font):
    """Add GPOS pair kerning that HarfBuzz applies while shaping."""
    features = """
feature kern {
    pos A V -40;
    pos V A -30;
} kern;
"""
    addOpenTypeFeaturesFromString(font, features)


def main():
    assets_dir = os.path.dirname(os.path.abspath(__file__))
    # The test executable runs with test/Graphics as its working directory,
    # so the fonts are emitted next to the other test fonts (tuffy.ttf).
    test_dir = os.path.dirname(assets_dir)
    work_dir = os.path.join(assets_dir, "variable-font-build")
    os.makedirs(work_dir, exist_ok=True)

    font_names = ("sfml-variable.ttf", "sfml-static.ttf")
    for output_dir in (test_dir, assets_dir):
        variable_path = os.path.join(output_dir, font_names[0])
        static_path = os.path.join(output_dir, font_names[1])
        build_variable_font(variable_path, work_dir)
        build_static_font(static_path)
        print(f"Wrote {variable_path}")
        print(f"Wrote {static_path}")


if __name__ == "__main__":
    main()
